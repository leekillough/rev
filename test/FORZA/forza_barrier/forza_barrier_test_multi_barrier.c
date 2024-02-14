/*
 * forza_barrier_test.c
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

  // barrier 1
  forza_zone_barrier(4);

  // do stuff
  int i = 9;
  i = i + argc;

  // barrier 2
  forza_zone_barrier(4);

  return i;
}
