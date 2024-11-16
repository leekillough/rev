/*
 * vadd.vi_e8_m1.128.64.cc
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
const unsigned  LMUL        = 1;    // Length multiplier
const unsigned  VLMAX       = 16;   // Max vector length LMUL * VLEN / SEW
const unsigned  AVL         = 64;   // Application Vector Length (elements)
//const unsigned   VL         = 16;    // Elements operated on by a vector instruction (<=VLMAX, <=AVL)

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_scalar[2] = { 0 };
unsigned counters_vector[2] = { 0 };

elem_t s0[AVL]              = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
                                0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40 };

void check_result( elem_t result[] ) {
  unsigned count = 5;
  for( unsigned i = 0; i < AVL; i++ ) {
    elem_t expected = i + 1 + count;
#if 1
    printf( "Checking i=%d 0x%x\n", i, (unsigned) expected );
#endif
    if( expected != result[i] ) {
      printf( "Error: 0x%x != 0x%x\n", expected, (unsigned) result[i] );
    }
    assert( expected == result[i] );
    if( ( i + 1 ) % 16 == 0 )
      count++;
  }
}

void add_array( elem_t a[], elem_t c[] ) {
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  unsigned count = 5;
  for( unsigned i = 0; i < AVL; i++ ) {
    c[i] = a[i] + count;
    if( ( i + 1 ) % 16 == 0 )
      count++;
  }
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_scalar[0] = time1 - time0;
  counters_scalar[1] = inst1 - inst0;
}

void vadd_array( elem_t a[], elem_t c[] ) {

#if 0
  printf( "REV VECTOR SUPPORT NOT YET AVAILABLE. Running scalar test instead.\n" );
  add_array( a, b, c );
#else
  unsigned time0, time1, inst0, inst1;
  int      rc    = 0x99;
  unsigned ITERS = AVL / VLMAX;
  unsigned count = 5;
  elem_t*  pa    = &( a[0] );
  elem_t*  pb    = &( a[0] );  // just a placeholder to ease test porting
  elem_t*  pc    = &( c[0] );
  RDTIME( time0 );
  RDINSTRET( inst0 );
  asm volatile( "add  a0, zero, %1  \n\t"  // Load AVL
                "add  a1, zero, %2  \n\t"  // Load pointer to a
                // "add  a2, zero, %3  \n\t"  // Load pointer to b
                "add  a3, zero, %4  \n\t"  // Load pointer to c
                "add  a4, zero, %5  \n\t"  // Load expected iterations
                "add  t1, zero, %6  \n\t"  // Load count
                "_vadd_loop: \n\t"
                //            AVL,    SEW, LMUL, tail/mask agnostic
                "vsetvli t0,  a0,     e8,     m1,  ta, ma  \n\t"
                //
                "vle8.v v0, (a1)      \n\t"  // Get first vector
                "sub a0, a0, t0       \n\t"  // Decrement N
                "slli t0, t0, 0       \n\t"  // Divide by number of bytes per element
                "add a1, a1, t0       \n\t"  // Bump pointer to a
                // "vle8.v v1, (a2)      \n\t"  // Get second vector
                // "add a2, a2, t0       \n\t"  // Bump pointer to b
                "vadd.vx v2, v0, t1   \n\t"  // Sum Vector with scalar
                "vse8.v v2, (a3)      \n\t"  // Store result
                "addi t1, t1, 1       \n\t"  // Increment count
                "add a3, a3, t0       \n\t"  // Bump pointer to c
                "addi a4, a4, -1      \n\t"  // Decrement expected iterations
                "bltz a4, _vadd_done  \n\t"  // Fail on overrun
                "bnez a0, _vadd_loop  \n\t"  // Loop back
                "_vadd_done: \n\t"
                "add %0, a4, zero     \n\t"  // Store result code
                : "=r"( rc )
                : "r"( AVL ), "r"( pa ), "r"( pb ), "r"( pc ), "r"( ITERS ), "r"( count )
                : "t0", "t1", "a0", "a1", "a2", "a3"  // TODO: assembler cannot handle v0,v1,v2 here
  );

  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  if( rc ) {
    printf( "Error: vadd.vi_e8_m1.128.64 rc=%d\n", rc );
    assert( 0 );
  }

#endif
}

int main( int argc, char** argv ) {

  // Scalar
  elem_t r_scalar[AVL] = { 0 };
  add_array( s0, r_scalar );
  check_result( r_scalar );

  // Vector
  elem_t r_vector[AVL] = { 0 };
  vadd_array( s0, r_vector );
  check_result( r_vector );

#ifndef USE_SPIKE
  printf( "[vadd.vi_e8_m1.128.64 rev cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd.vi_e8_m1.128.64 rev instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#else
  printf( "[vadd.vi_e8_m1.128.64 spike cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd.vi_e8_m1.128.64 spike instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#endif
}
