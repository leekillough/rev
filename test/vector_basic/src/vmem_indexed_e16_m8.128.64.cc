/*
 * vmem_indexed_e16_m8.128.64.cc
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// Standard includes
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// REV
#include "rev.h"

// Vector test configuration
typedef uint16_t elem_t;            // Element Type
const unsigned   VLEN       = 128;  // Width of register file entry
const unsigned   ELEN       = 64;   // Maximum bits per operation on vector element
const unsigned   SEW        = 16;   // Selected Element Width for adds
const unsigned   LMUL       = 8;    // Length multiplier
const unsigned   VLMAX      = 64;   // Max vector length LMUL * VLEN / SEW

const unsigned AVL          = 128;  // Source Data Size

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_vector[2] = { 0 };
elem_t   res[AVL]           = { 0 };
elem_t   xlt[AVL]           = { 0 };
elem_t   s0[AVL] = { 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00,
                     0x0e00, 0x0f00, 0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700, 0x1800, 0x1900, 0x1a00,
                     0x1b00, 0x1c00, 0x1d00, 0x1e00, 0x1f00, 0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700,
                     0x2800, 0x2900, 0x2a00, 0x2b00, 0x2c00, 0x2d00, 0x2e00, 0x2f00, 0x3000, 0x3100, 0x3200, 0x3300, 0x3400,
                     0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3a00, 0x3b00, 0x3c00, 0x3d00, 0x3e00, 0x3f00, 0x4000, 0x4100,
                     0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700, 0x4800, 0x4900, 0x4a00, 0x4b00, 0x4c00, 0x4d00, 0x4e00,
                     0x4f00, 0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5a00, 0x5b00,
                     0x5c00, 0x5d00, 0x5e00, 0x5f00, 0x6000, 0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700, 0x6800,
                     0x6900, 0x6a00, 0x6b00, 0x6c00, 0x6d00, 0x6e00, 0x6f00, 0x7000, 0x7100, 0x7200, 0x7300, 0x7400, 0x7500,
                     0x7600, 0x7700, 0x7800, 0x7900, 0x7a00, 0x7b00, 0x7c00, 0x7d00, 0x7e00, 0x8000 };

void set_vectors( uint64_t s ) {
  //vlmax = 8 * 128 / 64 = 16;
  asm volatile( "vsetivli x0, 16, e64, m8, ta, ma" );
#if 0
  // TODO vmv throws an assertion on spike. Bug or coding error?
  // Assertion failed: ((VLEN >> 3)/sizeof(T) > 0), function elt, file vector_unit.cc, line 71.
  asm volatile( "vmv.v.x v0, %0 " : : "r"(s) );
  asm volatile( "vmv.v.x v8, %0 " : : "r"(s) );
  asm volatile( "vmv.v.x v16, %0" : : "r"(s) );
  asm volatile( "vmv.v.x v24, %0" : : "r"(s) );
#else
  asm volatile( "vand.vi  v0, v0, 0    \n\t vor.vx   v0, v0, %0    \n\t"
                "vand.vi  v8, v8, 0    \n\t vor.vx   v8, v8, %0    \n\t"
                "vand.vi  v16, v16, 0  \n\t vor.vx   v16, v16, %0 \n\t"
                "vand.vi  v24, v24,  0 \n\t vor.vx   v24, v24, %0 \n\t"
                :
                : "r"( s ) );
#endif
}

void set_v0_mask( uint64_t m ) {
  asm volatile( "vsetivli x0, 2, e64, m1, ta, ma" );
  asm volatile( "vand.vi  v0, v0, 0    \n\t vor.vx   v0, v0, %0    \n\t" : : "r"( m ) );
}

int check( uint16_t expected, uint16_t actual ) {
#if 0
  printf("Checking 0x%lx\n", (uint64_t) expected);
#endif
  if( expected == actual )
    return 0;
  printf( "Error: expected=0x%x actual=0x%x\n", expected, actual );
  return 1;
}

int sanity( elem_t a[], elem_t r[] ) {

  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );

  elem_t* pa = a;
  elem_t* pr = r;
  printf( "pa=0x%p pr=0x%p\n", (void*) pa, (void*) pr );

  set_vectors( 0x10 );  // safe index
  set_v0_mask( 0xffffffffffffffffULL );

  /*
  Vector indexed operations add the contents of each element of the vector offset operand specified by vs2 to
  the base effective address to give the effective address of each element. The data vector register group has
  EEW=SEW, EMUL=LMUL, while the offset vector register group has EEW encoded in the instruction and
  EMUL=(EEW/SEW)*LMUL.

  The vector offset operand is treated as a vector of byte-address offsets.

  The assembler syntax for indexed loads and stores uses eix instead of ex to indicate the
  statically encoded EEW is of the index not the data.

  //TODO additional cases are commented out that were illegal in spike but
  //     appear to have valid EMULs. What is condition we need to detect (or is this a spike bug)?
*/

  // LMUL=1
  asm volatile( "vsetivli x0, 2, e64, m1, ta, ma" );                // vlmax = 1*128/64 = 8
  asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = LMUL*EEW/SEW = 1*64/64 = 1
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 1*32/64 = 1/2
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1*16/64 = 1/4
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1*8/64  = 1/8
  asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 4, e32, m1, ta, ma" );                // vlmax = 1*128/32 = 16
  asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 1*64/32 = 2
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 1*32/32 = 1
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1/2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1/4
  asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 8, e16, m1, ta, ma" );  // vlmax = 1*128/16 = 32
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 1*64/16 = 4
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1/2
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 16, e8, m1, ta, ma" );  // vlmax = 1*128/8 = 64
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 8
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  // LMUL=2
  asm volatile( "vsetivli x0, 4, e64, m2, ta, ma" );                // vlmax = 2*128/64 = 4
  asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 2*64/64 = 2
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 1
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1/2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1/4
  asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 8, e32, m2, ta, ma" );  // vlmax = 2*128/32 = 8
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1/2
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 16, e16, m2, ta, ma" );  // vlmax = 2*128/16 = 16
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 8
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  uint64_t avl = 32;
  asm volatile( "vsetvli  x0, %0, e8, m2, ta, ma" : : "r"( avl ) );  // vlmax = 2*128/8 = 32
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 2*64/8 = 16 ( >e8 ) Illegal
  //asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 8
  //asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 2
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  // LMUL=4
  asm volatile( "vsetivli x0, 8, e64, m4, ta, ma" );  // vlmax = 4*128/64 = 8
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 1
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1/2
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  asm volatile( "vsetivli x0, 16, e32, m4, ta, ma" );  // vlmax = 4*128/32 = 16
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 8
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  avl = 32;
  asm volatile( "vsetvli x0, %0, e16, m4, ta, ma" : : "r"( avl ) );  // vlmax = 4*128/16 = 32
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 4*64/16 = 16 ( >e8 ) Illegal
  //asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 8
  //asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 2
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  avl = 64;
  asm volatile( "vsetvli  x0, %0, e8, m4, ta, ma" : : "r"( avl ) );  // vlmax = 4*128/8 = 64
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 4*64/8 = 32 Illegal
  // asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"(pa) );   // EMUL = 4*32/8 = 16 Illegal
  //asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 8
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 4
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  // asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  // LMUL=8
  asm volatile( "vsetivli x0, 8, e64, m8, ta, ma" );  // vlmax = 8*128/64 = 16
  //asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"( pa ) );  // EMUL = 8*64/64 = 8
  asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 2
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 1
  //asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  avl = 32;
  asm volatile( "vsetvli x0, %0, e32, m8, ta, ma" : : "r"( avl ) );  // vlmax = 8*128/32 = 32
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 8*64/32 = 16 Illegal
  //asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"( pa ) );  // EMUL = 8
  //asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 4
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 2
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  avl = 64;
  asm volatile( "vsetvli x0, %0, e16, m8, ta, ma" : : "r"( avl ) );  // vlmax = 8*128/16 = 64
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 32 Illegal
  // asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"(pa) );   // EMUL = 16 Illegal
  //asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"( pa ) );  // EMUL = 8
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 4
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  // asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  //asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  avl = 128;
  asm volatile( "vsetvli  x0, %0, e8, m8, ta, ma" : : "r"( avl ) );  // vlmax = 8*128/8 = 128
  // asm volatile( "vluxei64.v  v8, (%0), v10, v0.t" : : "r"(pa) );   // EMUL = 64 Illegal
  // asm volatile( "vluxei32.v  v8, (%0), v12, v0.t" : : "r"(pa) );   // EMUL = 32 Illegal
  // asm volatile( "vluxei16.v  v8, (%0), v14, v0.t" : : "r"(pa) );   // EMUL = 16 Illegal
  asm volatile( "vluxei8.v   v8, (%0), v16, v0.t" : : "r"( pa ) );  // EMUL = 8
  // asm volatile( "vsuxei64.v  v8, (%0), v10, v0.t" : : "r"( pr ) );
  // asm volatile( "vsuxei32.v  v8, (%0), v12, v0.t" : : "r"( pr ) );
  // asm volatile( "vsuxei16.v  v8, (%0), v14, v0.t" : : "r"( pr ) );
  asm volatile( "vsuxei8.v   v8, (%0), v16, v0.t" : : "r"( pr ) );

  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  return 0;
}

int indexed_load16( uint64_t mask, elem_t a[], elem_t r[], elem_t x[] ) {
#if 0
  for (unsigned i=0;i<DSIZE;i++)
    printf("a[%d]=0x%x &=0x%zx\n", i, a[i], &(a[i]) );
#endif
  for( unsigned i = 0; i < AVL; i++ )  // clear result
    r[i] = 0;
  set_vectors( -1 );    // all bits 1 for all vectors
  set_v0_mask( mask );  // vector mask
  elem_t*  pa = &( a[0] );
  elem_t*  pr = &( r[0] );
  elem_t*  px = &( x[0] );
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  int count = AVL;
  while( count > 0 ) {
#if 0
    printf("pa=0x%zx pr=0x%zx px=0x%zx\n", pa, pr, px);
#endif
    unsigned newvl = 0;
    VSETVLI( newvl, AVL, e16, m8, ta, ma );
    asm volatile( "vle16.v    v16, (%0)           \t\n" : : "r"( px ) );
    asm volatile( "vluxei16.v v8, (%0), v16, v0.t \t\n" : : "r"( pa ) );  // EMUL = LMUL*EEW/SEW = 8*16/16 = 8
    asm volatile( "vse16.v    v8, (%0)            \t\n" : : "r"( pr ) );
    //pa += newvl;
    pr += newvl;
    px += newvl;
    count = count - newvl;
  }
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // Check
  int rc             = count;  // should be 0
  for( unsigned i = 0; i < AVL; i++ ) {
    bool mask1 = ( mask >> ( i % VLMAX ) ) & 1;
    if( mask1 ) {
      rc |= check( a[AVL - i - 1], r[i] );
    } else {
      rc |= check( -1, r[i] );
    }
  }

  return rc;
}

void report( int testnum, int rc ) {
  if( rc ) {
    printf( "Error: vmem_indexed_e16_m8.128.64 test %d rc=%d\n", testnum, rc );
    assert( 0 );
  } else {
    printf( "test %d OK\n", testnum );
  }
#ifndef USE_SPIKE
  printf( "[vmem_indexed_e16_m8.128.64 test%d rev cycles] %d\n", testnum, counters_vector[0] );
  printf( "[vmem_indexed_e16_m8.128.64 test%d rev instrs] %d\n", testnum, counters_vector[1] );
#else
  printf( "[vmem_indexed_e16_m8.128.64 test%d spike cycles] %d\n", testnum, counters_vector[0] );
  printf( "[vmem_indexed_e16_m8.128.64 test%d spike instrs] %d\n", testnum, counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}

int main( int argc, char** argv ) {
  int rc = -1;

  // create array of byte offsets that will reverse the order of the input array.

  for( unsigned i = 0; i < AVL; i++ )
    xlt[i] = ( AVL - i - 1 ) * sizeof( elem_t );

  printf( "test 0: sanity\n" );
  rc = sanity( s0, res );
  report( 0, rc );

  // VLMAX=64 ( v0 mask will only user lower 64-bits )
  printf( "test 1: indexed load using mask 0xffffffffffffffff\n" );
  rc = indexed_load16( 0xffffffffffffffffULL, s0, res, xlt );
  report( 1, rc );

  printf( "test 2: indexed load using mask 0x0\n" );
  rc = indexed_load16( 0x0ULL, s0, res, xlt );
  report( 2, rc );

  for( uint64_t bit = 0; bit < 64; bit++ ) {
    uint64_t mask = 1ULL << bit;
    printf( "test 3.%ld: indexed load using mask 0x%016lx\n", bit, mask );
    rc = indexed_load16( mask, s0, res, xlt );
    report( 3, rc );
  }

  return rc;
}