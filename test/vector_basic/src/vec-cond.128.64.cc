/*
 * vec-cond.128.64.cc
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// Ref: RV Unprivileged Spec - Appendix C

// Standard includes
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// REV
#include "rev.h"

// (int16) z[i] = ((int8) x[i] < 5) ? (int16) a[i] : (int16) b[i];
void conditional( size_t n, const int8_t* x, const int16_t* a, const int16_t* b, int16_t* z ) {
  // a0 = n, a1 = x, a2 = a, a3 = b, a4 = z
  asm volatile( "loop:                            \n\t"
                "vsetvli  t0, a0, e8, m1, ta, ma  \n\t"  // Use 8b elements.
                "vle8.v   v0, (a1)                \n\t"  // Get x[i]
                "sub      a0, a0, t0              \n\t"  // Decrement element count
                "add      a1, a1, t0              \n\t"  // x[i] Bump pointer
                "vmslt.vi v0, v0, 5               \n\t"  // Set mask in v0
                "vsetvli  x0, x0, e16, m2, ta, mu \n\t"  // Use 16b elements.
                "slli     t0, t0, 1               \n\t"  // Multiply by 2 bytes
                "vle16.v  v2, (a2), v0.t          \n\t"  // z[i] = a[i] case
                "vmnot.m  v0, v0                  \n\t"  // Invert v0
                "add      a2, a2, t0              \n\t"  // a[i] bump pointer
                "vle16.v  v2, (a3), v0.t          \n\t"  // z[i] = b[i] case
                "add      a3, a3, t0              \n\t"  // b[i] bump pointer
                "vse16.v  v2, (a4)                \n\t"  // Store z
                "add      a4, a4, t0              \n\t"  // z[i] bump pointer
                "bnez     a0, loop                \n\t" );
}

int main( int argc, char** argv ) {

  const unsigned N = 128;
  assert( N < 256 );

  int8_t  X[N];  // array to compare against
  int16_t A[N];  // Select when X[i] < 5
  int16_t B[N];  // Select when X[i] >= 5
  int16_t Z[N];  // Result

  for( uint8_t i = 0; i < N; i++ ) {
    X[i] = i % 10;
    A[i] = 0x0a00 + i;
    B[i] = 0x0b00 + i;
    Z[i] = 0x0bad;
  }

  // counter 0 = cycles
  // counter 1 = instructions
  unsigned counters_vector[2] = { 0 };
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  conditional( N, X, A, B, Z );
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // verify
  for( uint8_t i = 0; i < N; i++ ) {
    int16_t expected = ( ( i % 10 ) < 5 ) ? 0x0a00 + i : 0x0b00 + i;
#if 0
    printf("Checking 0x%x\n", expected);
#endif
    if( Z[i] != expected ) {
      printf( "Error: Actual=0x%x Expected=0x%x\n", B[i], expected );
      assert( false );
    }
  }

#ifndef USE_SPIKE
  printf( "[vec-cond.128.64 rev cycles] %d\n", counters_vector[0] );
  printf( "[vec-cond.128.64 rev instrs] %d\n", counters_vector[1] );
#else
  printf( "[vec-cond.128.64 spike cycles] %d\n", counters_vector[0] );
  printf( "[vec-cond.128.64 spike instrs] %d\n", counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}
