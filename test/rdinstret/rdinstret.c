/*
 * rdinstret.c
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 *
 */

#include "syscalls.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define NOP1    asm( " nop" );
#define NOP2    NOP1 NOP1
#define NOP4    NOP2 NOP2
#define NOP8    NOP4 NOP4
#define NOP16   NOP8 NOP8
#define NOP32   NOP16 NOP16
#define NOP64   NOP32 NOP32
#define NOP128  NOP64 NOP64
#define NOP256  NOP128 NOP128
#define NOP512  NOP256 NOP256
#define NOP1024 NOP512 NOP512

int main( int argc, char** argv ) {
  size_t time1, time2;

  asm volatile( " rdinstret %0" : "=r"( time1 ) );

  // 1024 nops produces around 1024 times
  NOP1024;

  asm volatile( " rdinstret %0" : "=r"( time2 ) );

  size_t diff = time2 - time1;

  char retired[64];
  snprintf( retired, sizeof( retired ), "%zu instructions retired\n", diff );
  rev_write( 1, retired, strlen( retired ) );

  // Make sure the time is between 1024 and 1026
  if( diff < 1024 || diff > 1026 )
    asm( " .word 0" );

  return 0;
}
