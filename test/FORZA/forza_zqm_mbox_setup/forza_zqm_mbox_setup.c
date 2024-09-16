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
  int TID = forza_get_my_zap();
  if( forza_get_my_zone() > 0 )
    return 0;

  uint64_t mbox_reg_value = 0;

  mbox_reg_value |= 7;               // mboxes used
  mbox_reg_value |= ( 0x0AD << 8 );  // logical PE
  mbox_reg_value |= ( 0x0B << 19 );  // phys hart
  mbox_reg_value |= ( 0UL << 28 );   // phys zap
  mbox_reg_value |= ( 2UL << 60 );   // aid

  forza_debug_print( mbox_reg_value, 0, 0xcafe );
  forza_zqm_setup( mbox_reg_value );

  uint64_t nzaps = ( forza_get_my_zone() == 0 ) ? forza_get_zaps_per_zone() : 0;
  forza_zone_barrier( nzaps );

  return 0;
}
