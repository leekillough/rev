/*
 * mask1_e8_m8.128.64.cc
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
const unsigned  AVL         = 128;  // Application Vector Length (elements)

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_vector[2] = { 0 };

elem_t s0[AVL] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
                   0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                   0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                   0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b,
                   0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
                   0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
                   0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f };

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

void store_v0_mask( uint64_t m[] ) {
  asm volatile( "vsetvli x0, %0, e8, m8, ta, ma" : : "r"( AVL ) );  // vlmax = 8 * 128 / 8
  asm volatile( "vsm.v  v0, (%0)" : : "r"( m ) );                   // evl = ceil(vl/8)
}

int check( elem_t expected, elem_t actual ) {
#if 0
  printf("Checking 0x%lx\n", (uint64_t) expected);
#endif
  if( expected == actual )
    return 0;
  printf( "Error: expected=0x%lx actual=0x%lx\n", (uint64_t) expected, (uint64_t) actual );
  return 1;
}

void report( int testnum, int rc ) {
  if( rc ) {
    printf( "Error: mask1_e8_m8.128.64 test %d rc=%d\n", testnum, rc );
    assert( 0 );
  } else {
    printf( "test %d OK\n", testnum );
  }
#ifndef USE_SPIKE
  printf( "[mask1_e8_m8.128.64 test%d rev cycles] %d\n", testnum, counters_vector[0] );
  printf( "[mask1_e8_m8.128.64 test%d rev instrs] %d\n", testnum, counters_vector[1] );
#else
  printf( "[mask1_e8_m8.128.64 test%d spike cycles] %d\n", testnum, counters_vector[0] );
  printf( "[mask1_e8_m8.128.64 test%d spike instrs] %d\n", testnum, counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}

unsigned time0, time1, inst0, inst1;

inline void STATS_ON() {
  RDTIME( time0 );
  RDINSTRET( inst0 );
}

inline void STATS_OFF() {
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;
}

int main( int argc, char** argv ) {
  int      rc      = 0;
  uint64_t mask[2] = { 0 };       // 128-bit mask
  elem_t*  pa      = &( s0[0] );  // test data

  /* **************************************************************** */
  printf( "test 1: mask <- 0\n" );
  set_vectors( -1ULL );
  int count = AVL;
  STATS_ON();
  while( count > 0 ) {
    unsigned newvl = 0;
    VSETVLI( newvl, AVL, e8, m8, ta, ma );
    asm volatile( "vand.vi  v8, v8, 0, v0.t \n\t"
                  "vmsne.vi v0, v8, 0       \n\t" );
    count = count - newvl;
  }
  store_v0_mask( mask );
  STATS_OFF();
  for( unsigned i = 0; i < 128; i++ ) {
    uint8_t expected = 0;
    uint8_t actual   = i < 64 ? ( mask[0] >> i ) & 1 : ( mask[1] >> ( i % 64 ) ) & 1;
    rc |= check( expected, actual );
  }
  report( 1, rc );

  /* **************************************************************** */
  for( unsigned elem = 0; elem < 128; elem++ ) {
    printf( "test 2.%d: mask element %d\n", elem, elem );
    set_vectors( -1ULL );
    int count = AVL;
    STATS_ON();
    while( count > 0 ) {
      unsigned newvl = 0;
      VSETVLI( newvl, AVL, e8, m8, ta, ma );
      asm volatile( "vle8.v v8, (%0)          \n\t"
                    "vmseq.vx v0, v8, %1      \n\t"
                    :
                    : "r"( pa ), "r"( elem ) );
      count = count - newvl;
    }
    store_v0_mask( mask );
    for( unsigned i = 0; i < 128; i++ ) {
      uint8_t expected = i == elem ? 1 : 0;
      uint8_t actual   = i < 64 ? ( mask[0] >> i ) & 1 : ( mask[1] >> ( i % 64 ) ) & 1;
      rc |= check( expected, actual );
    }
    STATS_OFF();
    report( 2, rc );
  }

  return rc;
}
