/*
 * vadd.cc
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

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_scalar[2] = { 0 };
unsigned counters_vector[2] = { 0 };

uint32_t s0[]               = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
uint32_t s1[] = { 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700, 0x800, 0x900, 0xa00, 0xb00, 0xc00, 0xd00, 0xe00, 0xf00, 0x1000 };

void check_result( uint32_t result[] ) {
  for( uint32_t i = 0; i < 16; i++ ) {
    uint32_t expected = ( i + 1 ) + ( ( i + 1 ) << 8 );
    if( expected != result[i] ) {
      printf( "Error: 0x%x != 0x%x\n", expected, result[i] );
    }
    assert( expected == result[i] );
  }
}

void add_array( uint32_t a[], uint32_t b[], uint32_t c[] ) {
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  for( int i = 0; i < 16; i++ ) {
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
void vadd_array( uint32_t a[], uint32_t b[], uint32_t c[] ) {

#if 0
  printf( "REV VECTOR SUPPORT NOT YET AVAILABLE. Running scalar test instead.\n" );
  add_array( a, b, c );
#else
  unsigned  time0, time1, inst0, inst1;
  int       rc    = 0x99;
  unsigned  N     = 16;
  unsigned  ITERS = N >> 2;
  uint32_t* pa    = &( a[0] );
  uint32_t* pb    = &( b[0] );
  uint32_t* pc    = &( c[0] );
  RDTIME( time0 );
  RDINSTRET( inst0 );
  asm volatile( "add  a0, zero, %1  \n\t"  // Load N
                "add  a1, zero, %2  \n\t"  // Load pointer to a
                "add  a2, zero, %3  \n\t"  // Load pointer to b
                "add  a3, zero, %4  \n\t"  // Load pointer to c
                "add  a4, zero, %5  \n\t"  // Load expected iterations
                "vvaddint32: \n\t"
                //          VL=n, SEW=32b, LMUL=1, tail/mask agnostic
                "vsetvli t0,  a0,     e32,     m1,  ta, ma  \n\t"
                //
                "vle32.v v0, (a1)     \n\t"  // Get first vector
                "sub a0, a0, t0       \n\t"  // Decrement N
                "slli t0, t0, 2       \n\t"  // Multiply number done by 4 bytes
                "add a1, a1, t0       \n\t"  // Bump pointer to a
                "vle32.v v1, (a2)     \n\t"  // Get second vector
                "add a2, a2, t0       \n\t"  // Bump pointer to b
                "vadd.vv v2, v0, v1   \n\t"  // Sum Vectors
                "vse32.v v2, (a3)     \n\t"  // Store result
                "add a3, a3, t0       \n\t"  // Bump pointer to c
                "addi a4, a4, -1      \n\t"  // Decrement timeout
                "bltz a4, vvaddexit   \n\t"  // Fail on timeout
                "bnez a0, vvaddint32  \n\t"  // Loop back
                "vvaddexit: \n\t"
                "add %0, a4, zero     \n\t"  // Store result code
                : "=r"( rc )
                : "r"( N ), "r"( pa ), "r"( pb ), "r"( pc ), "r"( ITERS )
                : "t0", "a0", "a1", "a2", "a3"  // TODO: assembler cannot handle v0,v1,v2 here
  );

  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  if( rc ) {
    printf( "Error: vadd_array rc=%d\n", rc );
    assert( 0 );
  }

#endif
}

int main( int argc, char** argv ) {

  // Scalar
  uint32_t r_scalar[16] = { 0 };
  add_array( s0, s1, r_scalar );
  check_result( r_scalar );

  // Vector
  uint32_t r_vector[16] = { 0 };
  vadd_array( s0, s1, r_vector );
  check_result( r_vector );

#ifndef USE_SPIKE
  printf( "[vadd.cc rev cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd.cc rev instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#else
  printf( "[vadd.cc spike cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vadd.cc spike instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#endif
}
