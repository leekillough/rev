/*
 * ex2.c
 *
 * RISC-V ISA: RV64I
 *
 * Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 */

#include "../../../common/syscalls/forza.h"
#include <stdlib.h>
#include <stdint.h>

uint64_t atom64 = 0;
int main(int argc, char **argv){
 //__atomic_fetch_add(&atom64, 2, __ATOMIC_RELAXED);

  uint64_t *addr;
  forza_zen_init(addr, (uint64_t)512);
  addr = (uint64_t *)forza_scratchpad_alloc(1 * sizeof(uint32_t));
  uint64_t size = sizeof(uint32_t);
  uint64_t dst = 1;
  forza_send(addr, size, dst);
  forza_scratchpad_free(addr, 1 * sizeof(uint32_t));

  int i = 9;
  i = i + argc;
  return i;
}
