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
  int      num_buff_entries = 10;
  uint64_t mem_buff[num_buff_entries];  // Buffer for zen to write to
  //forza_debug_print((uint64_t)&mem_buff[0], (uint64_t)num_buff_entries, (uint64_t)TID);

  uint64_t* tail_ptr;
  tail_ptr                = (uint64_t*) forza_scratchpad_alloc( 1 * sizeof( uint64_t* ) );
  *tail_ptr               = 0xdeadbeef;

  uint64_t* my_buff_start = &mem_buff[0];
  uint64_t  mbox_id       = 0xa;
  forza_zqm_mbox_setup( (uint64_t) my_buff_start, num_buff_entries * sizeof( uint64_t ), (uint64_t) tail_ptr, mbox_id );

  uint64_t nzaps = ( forza_get_my_zone() == 0 ) ? forza_get_zaps_per_zone() : 0;
  forza_zone_barrier( nzaps );

  // Now that we've done the zen setup, every thread should have *tail_ptr == mem_buff_start
  if( forza_get_my_zone() == 0 ) {
    forza_debug_print( nzaps, (uint64_t) my_buff_start, *tail_ptr );
    assert( (uint64_t) my_buff_start == *tail_ptr );
  }
#if 0
  register uint64_t a = 0;
  if (a == 0) {
    forza_send(0, (uint64_t)addr, 2 * sizeof(uint64_t));
  }
#endif

  forza_scratchpad_free( (uint64_t) tail_ptr, 1 * sizeof( uint64_t* ) );

  return 0;
}
