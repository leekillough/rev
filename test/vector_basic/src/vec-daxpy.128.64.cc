/*
 * vec-daxpy.128.64.cc
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

//--------------------------------------------------------------------------
// Input/Reference Data

typedef double data_t;
#include "vec-daxpy.h"

// void
// vec_daxpy(size_t n, const double a, const double *x, const double *y, double* z)
// {
//   size_t i;
//   for (i=0; i<n; i++)
//     z[i] = a * x[i] + y[i];
// }
//
// register arguments:
//     a0      n
//     fa0     a
//     a1      x
//     a2      y
//     a3      z

void vec_daxpy( size_t n, const double a, const double* x, const double* y, double* z ) {
  asm volatile( "vec_daxpy_loop:                         \n\t"
                "vsetvli    a4, a0, e64, m8, ta, ma  \n\t"
                "vle64.v    v0, (a1)                 \n\t"
                "sub        a0, a0, a4               \n\t"
                "slli       a4, a4, 3                \n\t"
                "add        a1, a1, a4               \n\t"
                "vle64.v    v8, (a2)                 \n\t"
                "vfmacc.vf  v8, fa0, v0              \n\t"
                "vse64.v    v8, (a3)                 \n\t"
                "add        a2, a2, a4               \n\t"
                "add        a3, a3, a4               \n\t"
                "bnez       a0, vec_daxpy_loop       \n\t"
                "ret                                 \n\t" );
}

int main( int argc, char** argv ) {

  data_t results_data[DATA_SIZE];

  // counter 0 = cycles
  // counter 1 = instructions
  unsigned counters_vector[2] = { 0 };
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  vec_daxpy( DATA_SIZE, input0, input1_data, input2_data, results_data );
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // verify double
  for( int i = 0; i < DATA_SIZE; i++ ) {
#if 0
    // TODO REV fastprint is not handling floating point correctly
    printf("Checking %1.8f (0x%lx)\n", verify_data[i], (uint64_t) verify_data[i]);;
#endif
    if( results_data[i] != verify_data[i] ) {
      printf(
        "Error: Actual=%1.8f (0x%lx)  Expected=%1.8f (0x%lx)\n",
        results_data[i],
        (uint64_t) results_data[i],
        verify_data[i],
        (uint64_t) verify_data[i]
      );
      assert( false );
    }
  }

#ifndef USE_SPIKE
  printf( "[vec-daxpy.128.64 rev cycles] %d\n", counters_vector[0] );
  printf( "[vec-daxpy.128.64 rev instrs] %d\n", counters_vector[1] );
#else
  printf( "[vec-daxpy.128.64 spike cycles] %d\n", counters_vector[0] );
  printf( "[vec-daxpy.128.64 spike instrs] %d\n", counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}
