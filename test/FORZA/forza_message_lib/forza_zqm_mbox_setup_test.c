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

enum RET_STATUS {
	SUCCESS,
	DONE_MSG,
	FAIL,
	INVALID_NUM_MBOX,
	INVALID_MBOX,
	INVALID_LPE,
	INVALID_PRECINCT,
	INVALID_ZONE,
	ZEN_NOT_READY,
	BAD_MSG_OP,
	MALLOC_FAIL
};

#define NMBOX 8  // HW restricts the number of mailboxes to 8

int forza_zqm_pe_setup( uint64_t logical_pe, uint64_t n_mailboxes) {
	
	// Currently assumes single AID that can have up to #HARTS/2 PEs per zone
	int max_pes = forza_get_harts_per_zap()/2;

	// Check inputs
	if (logical_pe > max_pes - 1)
		return INVALID_LPE;
	if (n_mailboxes > NMBOX)
		return INVALID_NUM_MBOX;
	
	forza_zqm_setup(logical_pe, n_mailboxes);
	return SUCCESS;
}  

int main( int argc, char** argv ) {
  int TID = forza_get_my_zap();
  if( forza_get_my_zone() > 0 )
    return 0;

  forza_debug_print( 0x0AD, 0, 0xcafe );
  // forza_zqm_setup( logical_pe, n_mailboxes );
  //forza_zqm_setup( 0x0AD, 6 );
  int result = forza_zqm_pe_setup(0x1, 6);
	if (result)
		assert(0);

  uint64_t nzaps = ( forza_get_my_zone() == 0 ) ? forza_get_zaps_per_zone() : 0;
  forza_zone_barrier( nzaps );

  return 0;
}
