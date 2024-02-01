/*
 * config_ecalls.c
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

  int i_harts_per_zap = atoi(argv[1]);
  assert(i_harts_per_zap == forza_get_harts_per_zap());

  int i_zaps_per_zone = atoi(argv[2]);
  assert(i_zaps_per_zone == forza_get_zaps_per_zone());

  int i_zones_per_precinct = atoi(argv[3]);
  assert(i_zones_per_precinct == forza_get_zones_per_precinct());

  int i_num_precincts = atoi(argv[4]);
  assert(i_num_precincts == forza_get_num_precincts());

  int i_my_zap = atoi(argv[5]);
  assert(i_my_zap == forza_get_my_zap());

  int i_my_zone = atoi(argv[6]);
  assert(i_my_zone == forza_get_my_zone());

  int i_my_precinct = atoi(argv[7]);
  assert(i_my_precinct == forza_get_my_precinct());

  return 0;
}
