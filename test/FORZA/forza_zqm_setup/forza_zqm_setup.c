/*
 * forza_zqm_setup.c
 *
 * RISC-V ISA: RV64I
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 * Edited by Lucata Corp.
 */

#include "../../../common/syscalls/forza.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define zqm_test_nthreads 16
#define zqm_test_nwords_pthread 68
#define zqm_storage_total_nwords (zqm_test_nthreads * zqm_test_nwords_pthread)
static const uint64_t nwords = zqm_test_nthreads * zqm_test_nwords_pthread;
static const uint64_t size = nwords * 8;
static uint64_t zrq[zqm_storage_total_nwords];

int main(int argc, char **argv){
  // For the function, use constants where possible.
  //forza_zqm_setup(zrq_start_addr, zrq_size_in_bytes, min_hart, max_hart, sequential_thread_ld_flag);
  forza_zqm_setup((uint64_t)&zrq[0], size, 0UL, 15UL, 1UL);

  int i = 4;
  i = i + argc;
  return i;
}
