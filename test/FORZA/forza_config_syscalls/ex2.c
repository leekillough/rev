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

#define assert(x)                                                              \
  do                                                                           \
    if (!(x)) {                                                                \
      asm(".dword 0x00000000");                                                \
    }                                                                          \
  while (0)

int main(int argc, char **argv){

  int harts_per_zap = 128; // Works
  //int harts_per_zap = atoi(argv[1]); // crashes with segfault
  assert(harts_per_zap == forza_get_harts_per_zap());

  return 0;
}
