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

int main(int argc, char **argv){
  uint64_t addr;
  forza_zen_init(addr, (uint64_t)512);

  int i = 9;
  i = i + argc;
  return i;
}
