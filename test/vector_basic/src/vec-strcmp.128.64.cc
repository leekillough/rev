/*
 * vec-strcmp.128.64.cc
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

int vec_strcmp( const char* src1, const char* src2 ) {
  // Using LMUL=2, but same register names work for larger LMULs
  // Max length vectors of bytes
  int rc = -1;
  asm volatile( "li t1, 0             \n\t"  // Initial pointer bump
                "loop:                    \n\t"
                "vsetvli t0, x0, e8, m2, ta, ma \n\t"
                "add a0, a0, t1       \n\t"  // Bump src1 pointer
                "vle8ff.v v8, (a0)    \n\t"  // Get src1 bytes
                "add a1, a1, t1       \n\t"  // Bump src2 pointer
                "vle8ff.v v16, (a1)   \n\t"  // Get src2 bytes
                "vmseq.vi v0, v8, 0   \n\t"  // Flag zero bytes in src1
                "vmsne.vv v1, v8, v16 \n\t"  // Flag if src1 != src2
                "vmor.mm v0, v0, v1   \n\t"  // Combine exit conditions
                "vfirst.m a2, v0      \n\t"  // ==0 or != ?
                "csrr t1, vl          \n\t"  // Get number of bytes fetched
                "bltz a2, loop        \n\t"  // Loop if all same and no zero byte
                "add a0, a0, a2       \n\t"  // Get src1 element address
                "lbu a3, (a0)         \n\t"  // Get src1 byte from memory
                "add a1, a1, a2       \n\t"  // Get src2 element address
                "lbu a4, (a1)         \n\t"  // Get src2 byte from memory
                "sub a0, a3, a4       \n\t"  // Return value.
                "ret                  \n\t"
                : "=r"( rc )
                : "r"( src1 ), "r"( src2 )
                : "t0", "t1"  // "a0", "a1", "a2", "a3", "a4"
  );
  return rc;
}

// counter 0 = cycles
// counter 1 = instructions
unsigned counters_scalar[2] = { 0 };
unsigned counters_vector[2] = { 0 };

const char* test_str        = "The quick brown fox jumped over the lazy dog";
const char* same_str        = "The quick brown fox jumped over the lazy dog";
const char* diff_str        = "The quick brown fox jumped over the lazy cat";

int main( int argc, char** argv ) {

  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );

  int r0 = vec_strcmp( test_str, same_str );
  assert( r0 == 0 );
  int r1 = vec_strcmp( test_str, diff_str );
  assert( r1 != 0 );

  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

#ifndef USE_SPIKE
  printf( "[vec-strcmp.128.64 rev cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vec-strcmp.128.64 rev instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#else
  printf( "[vec-strcmp.128.64 spike cycles] scalar=%d vector=%d\n", counters_scalar[0], counters_vector[0] );
  printf( "[vec-strcmp.128.64 spike instrs] scalar=%d vector=%d\n", counters_scalar[1], counters_vector[1] );
#endif
}
