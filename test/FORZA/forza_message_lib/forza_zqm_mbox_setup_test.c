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

// NOTE: This requires #HARTs > logical_pe

#include "../../../common/syscalls/forza.h"
#include "forza_message_lib.h"
#include <stdint.h>
#include <stdlib.h>

int main( int argc, char** argv ) {
  int TID = forza_get_my_zap();
  if( forza_get_my_zone() > 0 )
    return 0;

  forza_debug_print( 0x0AD, 0, 0xcafe );
  int result = forza_zqm_pe_setup( TID, 6 );
  if( result )
    assert( 0 );

  uint64_t nzaps = ( forza_get_my_zone() == 0 ) ? forza_get_zaps_per_zone() : 0;
  forza_zone_barrier( nzaps );

  return 0;
}
