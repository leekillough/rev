/*
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

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

int main( int argc, char** argv ) {
  int TID             = forza_get_my_zap();

  uint64_t logical_pe = 0x11 * ( forza_get_my_zone() + 1 );

  forza_debug_print( logical_pe, 0, 0xcafe );
  forza_zqm_setup( logical_pe, 6 );

  forza_zone_barrier( forza_get_zaps_per_zone() );

  return 0;
}
