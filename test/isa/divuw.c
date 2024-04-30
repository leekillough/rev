/*
 * divuw.c
 *
 * RISC-V ISA: RV32I
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 */

#include "isa_test_macros.h"
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

int main( int argc, char** argv ) {

  // #-------------------------------------------------------------
  // # Arithmetic tests
  // #-------------------------------------------------------------


  TEST_RR_OP( 2, divuw, 3, 20, 6 );
  TEST_RR_OP( 3, divuw, 715827879, -20 << 32 >> 32, 6 );
  TEST_RR_OP( 4, divuw, 0, 20, -6 );
  TEST_RR_OP( 5, divuw, 0, -20, -6 );

  TEST_RR_OP( 6, divuw, -1 << 31, -1 << 31, 1 );
  TEST_RR_OP( 7, divuw, 0, -1 << 31, -1 );

  TEST_RR_OP( 8, divuw, -1, -1 << 31, 0 );
  TEST_RR_OP( 9, divuw, -1, 1, 0 );
  TEST_RR_OP( 10, divuw, -1, 0, 0 );

  asm volatile( " bne x0, gp, pass;" );
  asm volatile( "pass:" );
  asm volatile( "j continue" );

  asm volatile( "fail:" );
  assert( false );

  asm volatile( "continue:" );
  asm volatile( "li ra, 0x0" );

  return 0;
}
