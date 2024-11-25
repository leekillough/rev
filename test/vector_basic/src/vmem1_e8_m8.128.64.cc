/*
 * vmem1_e8_m8.128.64.cc
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
typedef uint8_t elem_t;             // Element Type
const unsigned  VLEN        = 128;  // Width of register file entry
const unsigned  ELEN        = 64;   // Maximum bits per operation on vector element
const unsigned  SEW         = 8;    // Selected Element Width for adds
const unsigned  LMUL        = 8;    // Length multiplier
const unsigned  VLMAX       = 128;  // Max vector length LMUL * VLEN / SEW
const unsigned  AVL         = 256;  // Application Vector Length (elements)

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_vector[2] = { 0 };

elem_t s0[AVL] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
                   0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                   0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                   0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b,
                   0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
                   0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
                   0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84,
                   0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
                   0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
                   0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd,
                   0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
                   0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3,
                   0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
                   0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

void set_vectors( uint64_t s ) {
  //vlmax = 8 * 128 / 64 = 16;
  asm volatile( "vsetivli x0, 16, e64, m8, ta, ma" );
#if 0
  // TODO vmv throws an assertion on spike. Bug or coding error?
  // Assertion failed: ((VLEN >> 3)/sizeof(T) > 0), function elt, file vector_unit.cc, line 71.
  asm volatile( "vmv.v.x v0, %0 " : : "r"(s) );
  asm volatile( "vmv.v.x v8, %0 " : : "r"(s));
  asm volatile( "vmv.v.x v16, %0" : : "r"(s));
  asm volatile( "vmv.v.x v24, %0" : : "r"(s));
#else
  asm volatile( "vand.vi  v0, v0, 0    \n\t vor.vx   v0, v0, %0    \n\t"
                "vand.vi  v8, v8, 0    \n\t vor.vx   v8, v8, %0    \n\t"
                "vand.vi  v16, v16, 0  \n\t vor.vx   v16, v16, %0 \n\t"
                "vand.vi  v24, v24,  0 \n\t vor.vx   v24, v24, %0 \n\t"
                :
                : "r"( s ) );
#endif
}

void load_v0_mask( uint64_t m[] ) {
  asm volatile( "vsetvli x0, %0, e8, m8, ta, ma" : : "r"( AVL ) );  // vlmax = 8 * 128 / 8
  asm volatile( "vlm.v  v0, (%0)" : : "r"( m ) );                   // evl = ceil(vl/8)
}

int check( elem_t expected, elem_t actual ) {
  if( expected == actual )
    return 0;
  printf( "Error: expected=0x%lx actual=0x%lx\n", (uint64_t) expected, (uint64_t) actual );
  return 1;
}

int unit_stride_load8( uint64_t mask[], elem_t a[], elem_t r[] ) {
  for( unsigned i = 0; i < AVL; i++ )  // clear result
    r[i] = 0;
  set_vectors( -1 );     // all bits 1 for all vectors
  load_v0_mask( mask );  // vector mask
  elem_t*  pa = &( a[0] );
  elem_t*  pr = &( r[0] );
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  int count = AVL;
  while( count > 0 ) {
    unsigned newvl = 0;
    VSETVLI( newvl, AVL, e8, m8, ta, ma );
    asm volatile( "vle8.v v8, (%0), v0.t \t\n" : : "r"( pa ) );  // EMUL = LMUL*EEW/SEW = 8*8/8 = 8
    asm volatile( "vse8.v v8, (%0)       \t\n" : : "r"( pr ) );
    pa += newvl;
    pr += newvl;
    count = count - newvl;
  }
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // Check
  int rc             = count;  // should be 0
  for( unsigned i = 0; i < AVL; i++ ) {
    uint64_t el    = i % VLMAX;
    uint64_t m     = el < 64 ? mask[0] : mask[1];
    bool     mask1 = ( m >> ( i % 64 ) ) & 1;
    if( mask1 ) {
      rc |= check( a[i], r[i] );
    } else {
      rc |= check( -1, r[i] );
    }
  }

  return rc;
}

void report( int testnum, int rc ) {
  if( rc ) {
    printf( "Error: vmem1_e8_m8.128.64 test %d rc=%d\n", testnum, rc );
    assert( 0 );
  } else {
    printf( "test %d OK\n", testnum );
  }
#ifndef USE_SPIKE
  printf( "[vmem1_e8_m8.128.64 test%d rev cycles] %d\n", testnum, counters_vector[0] );
  printf( "[vmem1_e8_m8.128.64 test%d rev instrs] %d\n", testnum, counters_vector[1] );
#else
  printf( "[vmem1_e8_m8.128.64 test%d spike cycles] %d\n", testnum, counters_vector[0] );
  printf( "[vmem1_e8_m8.128.64 test%d spike instrs] %d\n", testnum, counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}

int main( int argc, char** argv ) {
  int    rc        = -1;
  elem_t res[AVL]  = { 0 };

  // VLMAX=128 ( Full v0 register used in mask  )
  uint64_t mask[2] = { 0 };

  printf( "test 1: unit stride load using mask 0xffffffffffffffff_ffffffffffffffff\n" );
  mask[0] = 0xffffffffffffffffULL;
  mask[1] = 0xffffffffffffffffULL;
  rc      = unit_stride_load8( mask, s0, res );
  report( 1, rc );

  printf( "test 2: unit stride load using mask 0x0\n" );
  mask[0] = 0x0ULL;
  mask[1] = 0x0ULL;
  rc      = unit_stride_load8( mask, s0, res );
  report( 2, rc );

  for( uint64_t bit = 0; bit < 128; bit++ ) {
    if( bit < 64 ) {
      mask[0] = 1ULL << bit;
      mask[1] = 0ULL;
    } else {
      mask[0] = 0ULL;
      mask[1] = 1ULL << ( bit % 64 );
    }
    printf( "test 3.%ld: unit stride load using mask 0x%016lx 0x%016lx\n", bit, mask[1], mask[0] );
    rc = unit_stride_load8( mask, s0, res );
    report( 3, rc );
  }

  return rc;
}
