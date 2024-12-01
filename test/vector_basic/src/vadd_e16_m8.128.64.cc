/*
 * vadd_e16_m8.128.64.cc
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
const unsigned   AVL        = 128;  // Application Vector Length (elements)

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_scalar[2] = { 0 };
unsigned counters_vector[2] = { 0 };

elem_t s0[AVL] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
                   0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
                   0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                   0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
                   0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
                   0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72,
                   0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80 };
elem_t s1[AVL] = {
  0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00, 0x0e00, 0x0f00, 0x1000,
  0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700, 0x1800, 0x1900, 0x1a00, 0x1b00, 0x1c00, 0x1d00, 0x1e00, 0x1f00, 0x2000,
  0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600, 0x2700, 0x2800, 0x2900, 0x2a00, 0x2b00, 0x2c00, 0x2d00, 0x2e00, 0x2f00, 0x3000,
  0x3100, 0x3200, 0x3300, 0x3400, 0x3500, 0x3600, 0x3700, 0x3800, 0x3900, 0x3a00, 0x3b00, 0x3c00, 0x3d00, 0x3e00, 0x3f00, 0x4000,
  0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700, 0x4800, 0x4900, 0x4a00, 0x4b00, 0x4c00, 0x4d00, 0x4e00, 0x4f00, 0x5000,
  0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700, 0x5800, 0x5900, 0x5a00, 0x5b00, 0x5c00, 0x5d00, 0x5e00, 0x5f00, 0x6000,
  0x6100, 0x6200, 0x6300, 0x6400, 0x6500, 0x6600, 0x6700, 0x6800, 0x6900, 0x6a00, 0x6b00, 0x6c00, 0x6d00, 0x6e00, 0x6f00, 0x7000,
  0x7100, 0x7200, 0x7300, 0x7400, 0x7500, 0x7600, 0x7700, 0x7800, 0x7900, 0x7a00, 0x7b00, 0x7c00, 0x7d00, 0x7e00, 0x7f00, 0x8000 };

void check_result( elem_t result[] ) {
  for( unsigned i = 0; i < AVL; i++ ) {
    elem_t expected = ( i + 1 ) + ( ( i + 1 ) << 8 );
#if 0
    printf("Checking i=%d 0x%x\n", i, expected);
#endif
    if( expected != result[i] ) {
      printf( "Error: 0x%x != 0x%x\n", expected, result[i] );
    }
    assert( expected == result[i] );
  }
}

void add_array( elem_t a[], elem_t b[], elem_t c[] ) {
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  for( unsigned i = 0; i < AVL; i++ ) {
    c[i] = a[i] + b[i];
  }
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_scalar[0] = time1 - time0;
  counters_scalar[1] = inst1 - inst0;
}

/*
See riscv-unpriveleged spec Appendix C.1 Vector-vector add example
*/
void vadd_array( elem_t a[], elem_t b[], elem_t c[] ) {

#if 0
  printf( "REV VECTOR SUPPORT NOT YET AVAILABLE. Running scalar test instead.\n" );
  add_array( a, b, c );
#else
  unsigned time0, time1, inst0, inst1;
  int      rc    = 0x99;
  unsigned ITERS = AVL / VLMAX;
  elem_t*  pa    = &( a[0] );
  elem_t*  pb    = &( b[0] );
  elem_t*  pc    = &( c[0] );
  RDTIME( time0 );
  RDINSTRET( inst0 );
  asm volatile( "add  a0, zero, %1  \n\t"  // Load AVL
                "add  a1, zero, %2  \n\t"  // Load pointer to a
                "add  a2, zero, %3  \n\t"  // Load pointer to b
                "add  a3, zero, %4  \n\t"  // Load pointer to c
                "add  a4, zero, %5  \n\t"  // Load expected iterations
                "_vadd_loop: \n\t"
                //            AVL,    SEW, LMUL, tail/mask agnostic
                "vsetvli t0,  a0,     e16,     m8,  ta, ma  \n\t"
                //
                "vle16.v v0, (a1)     \n\t"  // Get first vector
                "sub a0, a0, t0       \n\t"  // Decrement N
                "slli t0, t0, 1       \n\t"  // Divide by number of bytes per element
                "add a1, a1, t0       \n\t"  // Bump pointer to a
                "vle16.v v8, (a2)     \n\t"  // Get second vector
                "add a2, a2, t0       \n\t"  // Bump pointer to b
                "vadd.vv v16, v0, v8  \n\t"  // Sum Vectors
                "vse16.v v16, (a3)    \n\t"  // Store result
                "add a3, a3, t0       \n\t"  // Bump pointer to c
                "addi a4, a4, -1      \n\t"  // Decrement expected iterations
                "bltz a4, _vadd_done  \n\t"  // Fail on overrun
                "bnez a0, _vadd_loop  \n\t"  // Loop back
                "_vadd_done: \n\t"
                "add %0, a4, zero     \n\t"  // Store result code
                : "=r"( rc )
                : "r"( AVL ), "r"( pa ), "r"( pb ), "r"( pc ), "r"( ITERS )
                : "t0", "a0", "a1", "a2", "a3"  // TODO: assembler cannot handle v0,v1,v2 here
  );

  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  if( rc ) {
    printf( "Error: vadd_e16_m8.128.64 rc=%d\n", rc );
    assert( 0 );
  }

#endif
}

int main( int argc, char** argv ) {

  // Scalar
  elem_t r_scalar[AVL] = { 0 };
  add_array( s0, s1, r_scalar );
  check_result( r_scalar );

  // Vector
  elem_t r_vector[AVL] = { 0 };
  vadd_array( s0, s1, r_vector );
  check_result( r_vector );

#ifndef USE_SPIKE
  printf( "[vadd_e16_m8.128.64 rev cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd_e16_m8.128.64 rev instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#else
  printf( "[vadd_e16_m8.128.64 spike cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd_e16_m8.128.64 spike instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#endif
}
