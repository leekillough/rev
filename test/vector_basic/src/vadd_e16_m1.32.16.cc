/*
 * vadd_e16_m1.32.16.cc
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
typedef uint16_t elem_t;           // Element Type
const unsigned   VLEN       = 32;  // Width of register file entry
const unsigned   ELEN       = 16;  // Maximum bits per operation on vector element
const unsigned   SEW        = 16;  // Selected Element Width for adds
const unsigned   LMUL       = 1;   // Length multiplier
const unsigned   VLMAX      = 2;   // Max vector length LMUL * VLEN / SEW
const unsigned   AVL        = 16;  // Application Vector Length (elements)

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_scalar[2] = { 0 };
unsigned counters_vector[2] = { 0 };

elem_t s0[AVL]              = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 };
elem_t s1[AVL]              = {
  0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00, 0x0e00, 0x0f00, 0x1000 };

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
                "vsetvli t0,  a0,     e16,     m1,  ta, ma  \n\t"
                //
                "vle16.v v0, (a1)     \n\t"  // Get first vector
                "sub a0, a0, t0       \n\t"  // Decrement N
                "slli t0, t0, 1       \n\t"  // Divide by number of bytes per element
                "add a1, a1, t0       \n\t"  // Bump pointer to a
                "vle16.v v1, (a2)     \n\t"  // Get second vector
                "add a2, a2, t0       \n\t"  // Bump pointer to b
                "vadd.vv v2, v0, v1   \n\t"  // Sum Vectors
                "vse16.v v2, (a3)     \n\t"  // Store result
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
    printf( "Error: vadd_e16_m1.32.16 rc=%d\n", rc );
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
  printf( "[vadd_e16_m1.32.16 rev cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd_e16_m1.32.16 rev instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#else
  printf( "[vadd_e16_m1.32.16 spike cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd_e16_m1.32.16 spike instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#endif
}
