/*
 * fw.c
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

#include <stdlib.h>

extern void fw( void ) {
  int a = 0;
  int b = 1;
  while( 1 ) {
    a = b;
  }
}

int main( int argc, char** argv ) {
  fw();
  return 0;
}
