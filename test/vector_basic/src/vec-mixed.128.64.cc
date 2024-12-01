/*
 * vec-mixed.128.64.cc
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// Ref: https://github.com/riscv-software-src/riscv-tests
// Modified for REV environment

// Standard includes
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// REV
#include "rev.h"

// Code using one width for predicate and different width for masked compute.
//    int8_t a[]; int32_t b[], c[];
//    for (i=0; i<n; i++) { b[i] = (a[i] < 5) ? c[i] : 1; }
//
// Mixed-width code that keeps SEW/LMUL=8
void mixed( size_t n, const int8_t* a, const int32_t* b, int32_t* c ) {
  // a0 = n, a1 = a, a2 = b, a3 = c
  asm volatile( "loop:                             \n\t"
                "vsetvli a4, a0, e8, m1, ta, ma  \n\t"  // Byte vector for predicate calc
                "vle8.v v1, (a1)                 \n\t"  // Load a[i]
                "add a1, a1, a4                  \n\t"  // Bump pointer.
                "vmslt.vi v0, v1, 5              \n\t"  // a[i] < 5?
                "vsetvli x0, a0, e32, m4, ta, mu \n\t"  // Vector of 32-bit values.
                "sub a0, a0, a4                  \n\t"  // Decrement count
                "vmv.v.i v4, 1                   \n\t"  // Splat immediate to destination
                "vle32.v v4, (a3), v0.t          \n\t"  // Load requested elements of C, others undisturbed
                "sll t1, a4, 2                   \n\t"
                "add a3, a3, t1                  \n\t"  // Bump pointer.
                "vse32.v v4, (a2)                \n\t"  // Store b[i].
                "add a2, a2, t1                  \n\t"  // Bump pointer.
                "bnez a0, loop                   \n\t"  // Any more?
  );
}

int main( int argc, char** argv ) {

  const unsigned N = 128;
  int8_t         A[N];
  int32_t        B[N];
  int32_t        C[N];

  for( unsigned i = 0; i < N; i++ ) {
    A[i] = i % 10;
    B[i] = 0xbadbeef;
    C[i] = i * 0x1000;
  }

  // counter 0 = cycles
  // counter 1 = instructions
  unsigned counters_vector[2] = { 0 };
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  mixed( N, A, B, C );
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // verify
  for( unsigned i = 0; i < N; i++ ) {
    int32_t expected = ( ( i % 10 ) < 5 ) ? i * 0x1000 : 1;
#if 0
    printf("Checking 0x%x\n", expected);
#endif
    if( B[i] != expected ) {
      printf( "Error: Actual=0x%x Expected=0x%x\n", B[i], expected );
      assert( false );
    }
  }

#ifndef USE_SPIKE
  printf( "[vec-mixed.128.64 rev cycles] %d\n", counters_vector[0] );
  printf( "[vec-mixed.128.64 rev instrs] %d\n", counters_vector[1] );
#else
  printf( "[vec-mixed.128.64 spike cycles] %d\n", counters_vector[0] );
  printf( "[vec-mixed.128.64 spike instrs] %d\n", counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}
