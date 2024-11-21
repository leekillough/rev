/*
 * vec-sgemm.128.64.cc
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

#include "vec-sgemm.h"

// RV64IDV system
//
// void
// vec_sgemm_nn(size_t n,
//          size_t m,
//          size_t k,
//          const float*a,   // m * k matrix
//          size_t lda,
//          const float*b,   // k * n matrix
//          size_t ldb,
//          float*c,         // m * n matrix
//          size_t ldc)
//
//  c += a*b (alpha=1, no transpose on input matrices)
//  matrices stored in C row-major order

#define n a0
#define m a1
#define k a2
#define ap a3
#define astride a4
#define bp a5
#define bstride a6
#define cp a7
#define cstride t0
#define kt t1
#define nt t2
#define bnp t3
#define cnp t4
#define akp t5
#define bkp s0
#define nvl s1
#define ccp s2
#define amp s3

// Use args as additional temporaries
#define ft12 fa0
#define ft13 fa1
#define ft14 fa2
#define ft15 fa3

// #define FRAMESIZE 32
#define T xstr

// This version holds a 16*VLMAX block of C matrix in vector registers
// in inner loop, but otherwise does not cache or TLB tiling.
void vec_sgemm_nn (size_t, size_t, size_t, const float*, size_t, const float*, size_t, float*, size_t) 
{
  asm volatile(
    ".set FRAMESIZE, 32 \t\n"
    "ld " T(cstride) ", 0(sp)      \t\n"  // Get arg from stack frame	
    "addi sp, sp, -FRAMESIZE   \t\n"
    "sd s0, 0(sp)   \t\n"
    "sd s1, 8(sp)   \t\n"
    "sd s2, 16(sp)   \t\n"

    // Check for zero size matrices        
    "beqz " T(n) ", exit   \t\n"
    "beqz " T(m) ", exit   \t\n"
    "beqz " T(k) ", exit   \t\n"

    // Convert elements strides to byte strides.
    "slli " T(astride) ", " T(astride) ", 2   \t\n"
    "slli " T(bstride) ", " T(bstride) ", 2   \t\n"
    "slli " T(cstride) ", " T(cstride) ", 2   \t\n"

    "slti t6, " T(m) ", 16   \t\n"
    "bnez t6, end_rows   \t\n"

"c_row_loop: \t\n"  // Loop across rows of C blocks

    "mv " T(nt) ", " T(n) " \t\n"  // Initialize n counter for next row of C blocks

    "mv " T(bnp) ", " T(bp) " \t\n" // Initialize B n-loop pointer to start
    "mv " T(cnp) ", " T(cp) " \t\n" // Initialize C n-loop pointer

"c_col_loop: \t\n" // Loop across one row of C blocks
    "vsetvli " T(nvl) ", " T(nt) ", e32, ta, ma  \t\n"  // 32-bit vectors, LMUL=1

    "mv " T(akp) ", " T(ap) " \t\n"  // reset pointer into A to beginning
    "mv " T(bkp) ", " T(bnp) " \t\n" // step to next column in B matrix

    // Initalize current C submatrix block from memory.
    "vle32.v  v0, (" T(cnp) "); add " T(ccp) ", " T(cnp) ", " T(cstride) ";   \t\n"
    "vle32.v  v1, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v2, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v3, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v4, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v5, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v6, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v7, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v8, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v  v9, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v10, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v11, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v12, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v13, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v14, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vle32.v v15, (" T(ccp) ")   \t\n"


    "mv " T(kt) ", " T(k) " \t\n" // Initialize inner loop counter

    // Inner loop scheduled assuming 4-clock occupancy of vfmacc instruction and single-issue pipeline
    // Software pipeline loads
    "flw ft0, (" T(akp) "); add " T(amp) ", " T(akp) ", " T(astride) ";   \t\n"
    "flw ft1, (" T(amp) "); add " T(amp) ", " T(amp) ", " T(astride) ";   \t\n"
    "flw ft2, (" T(amp) "); add " T(amp) ", " T(amp) ", " T(astride) ";   \t\n"
    "flw ft3, (" T(amp) "); add " T(amp) ", " T(amp) ", " T(astride) ";   \t\n"
    // Get vector from B matrix
    "vle32.v v16, (" T(bkp) ")   \t\n"

    // Loop on inner dimension for current C block
 "k_loop: \t\n"
    "vfmacc.vf v0, ft0, v16   \t\n"
    "add " T(bkp) ", " T(bkp) ", " T(bstride) "   \t\n"
    "flw ft4, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v1, ft1, v16   \t\n"
    "addi " T(kt) ", " T(kt) ", -1   \t\n"  // Decrement k counter
    "flw ft5, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v2, ft2, v16   \t\n"
    "flw ft6, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "flw ft7, (" T(amp) ")   \t\n"
    "vfmacc.vf v3, ft3, v16   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "flw ft8, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v4, ft4, v16   \t\n"
    "flw ft9, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v5, ft5, v16   \t\n"
    "flw ft10, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v6, ft6, v16   \t\n"
    "flw ft11, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v7, ft7, v16   \t\n"
    "flw " T(ft12) ", (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v8, ft8, v16   \t\n"
    "flw " T(ft13) ", (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v9, ft9, v16   \t\n"
    "flw " T(ft14) ", (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v10, ft10, v16   \t\n"
    "flw " T(ft15) ", (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "addi " T(akp) ", " T(akp) ", 4       \t\n"    // Move to next column of a
    "vfmacc.vf v11, ft11, v16   \t\n"
    "beqz " T(kt) ", 1f   \t\n"              // Don't load past end of matrix
    "flw ft0, (" T(akp) ")   \t\n"
    "add " T(amp) ", " T(akp) ", " T(astride) "   \t\n"
"1:  vfmacc.vf v12, " T(ft12) ", v16 \t\n"
    "beqz " T(kt) ", 1f   \t\n"
    "flw ft1, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
"1:  vfmacc.vf v13, " T(ft13) ", v16 \t\n"
    "beqz " T(kt) ", 1f   \t\n"
    "flw ft2, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
"1:  vfmacc.vf v14, " T(ft14) ", v16 \t\n"
    "beqz " T(kt) ", 1f  \t\n"               // Exit out of loop
    "flw ft3, (" T(amp) ")   \t\n"
    "add " T(amp) ", " T(amp) ", " T(astride) "   \t\n"
    "vfmacc.vf v15, " T(ft15) ", v16   \t\n"
    "vle32.v v16, (" T(bkp) ")  \t\n"          // Get next vector from B matrix, overlap loads with jump stalls
    "j k_loop   \t\n"

"1:  vfmacc.vf v15, " T(ft15) ", v16 \t\n"
    
    // Save C matrix block back to memory
    "vse32.v  v0, (" T(cnp) "); add " T(ccp) ", " T(cnp) ", " T(cstride) ";   \t\n"
    "vse32.v  v1, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v2, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v3, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v4, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v5, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v6, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v7, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v8, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v  v9, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v10, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v11, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v12, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v13, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v14, (" T(ccp) "); add " T(ccp) ", " T(ccp) ", " T(cstride) ";   \t\n"
    "vse32.v v15, (" T(ccp) ")   \t\n"

    // Following tail instructions should be scheduled earlier in free slots during C block save.
    // Leaving here for clarity.

    // Bump pointers for loop across blocks in one row
    "slli t6, " T(nvl) ", 2   \t\n"
    "add " T(cnp) ", " T(cnp) ", t6     \t\n"                    // Move C block pointer over
    "add " T(bnp) ", " T(bnp) ", t6  \t\n"                       // Move B block pointer over
    "sub " T(nt) ", " T(nt) ", " T(nvl) "   \t\n"                       // Decrement element count in n dimension
    "bnez " T(nt) ", c_col_loop    \t\n"                  // Any more to do?

    // Move to next set of rows
    "addi " T(m) ", " T(m) ", -16 \t\n" // Did 16 rows above
    "slli t6, " T(astride) ", 4 \t\n" // Multiply astride by 16
    "add " T(ap) ", " T(ap) ", t6  \t\n"       // Move A matrix pointer down 16 rows
    "slli t6, " T(cstride) ", 4 \t\n" // Multiply cstride by 16
    "add " T(cp) ", " T(cp) ", t6 \t\n"        // Move C matrix pointer down 16 rows
    
    "slti t6, " T(m) ", 16   \t\n"
    "beqz t6, c_row_loop   \t\n"

    // Handle end of matrix with fewer than 16 rows.
    // Can use smaller versions of above decreasing in powers-of-2 depending on code-size concerns.
"end_rows: \t\n"
    // Not done.

"exit: \t\n"
    "ld s0, 0(sp)   \t\n"
    "ld s1, 8(sp)   \t\n"
    "ld s2, 16(sp)   \t\n"
    "addi sp, sp, FRAMESIZE   \t\n"
    "ret   \t\n"
 );
}

int main( int argc, char** argv ) {

  data_t results_data[ARRAY_SIZE] = {0};

  // counter 0 = cycles
  // counter 1 = instructions
  unsigned counters_vector[2] = { 0 };
  unsigned time0, time1, inst0, inst1;
  RDTIME( time0 );
  RDINSTRET( inst0 );
  vec_sgemm_nn(DIM_SIZE, DIM_SIZE, DIM_SIZE, input1_data, DIM_SIZE, input2_data, DIM_SIZE, results_data, DIM_SIZE);
  RDINSTRET( inst1 );
  RDTIME( time1 );
  counters_vector[0] = time1 - time0;
  counters_vector[1] = inst1 - inst0;

  // verify data
  for( int i = 0; i < ARRAY_SIZE; i++ ) {
#if 0
  // TODO REV fastprint is not handling floating point correctly
  printf("Checking %1.8f (0x%lx)\n", verify_data[i], (uint64_t) verify_data[i]);;
#endif
    if( results_data[i] != verify_data[i] ) {
      printf( "Error: Actual=%1.8f (0x%lx)  Expected=%1.8f (0x%lx)\n",
	      results_data[i], (uint64_t) results_data[i],
	      verify_data[i],  (uint64_t) verify_data[i] );
      assert( false );
    }
  }

#ifndef USE_SPIKE
  printf( "[vec-sgemm.128.64 rev cycles] %d\n", counters_vector[0] );
  printf( "[vec-sgemm.128.64 rev instrs] %d\n", counters_vector[1] );
#else
  printf( "[vec-sgemm.128.64 spike cycles] %d\n", counters_vector[0] );
  printf( "[vec-sgemm.128.64 spike instrs] %d\n", counters_vector[1] );
  if( counters_vector[0] > 0 ) {
    printf(
      "Warning: Cycles should be 0 indicating something has gone awry in Spike sim.  Likely a vector related memory exception\n"
    );
  }
#endif
}
