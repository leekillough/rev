/*
 * forza_barrier_test.c
 *
 * RISC-V ISA: RV64I
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 */

#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

int main( int argc, char** argv ) {

  forza_zone_barrier( 1 );

  int i = 9;
  i     = i + argc;
  return i;
}
