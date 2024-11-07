#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Rev Tracing
#include "../../include/rev-macros.h"

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

int main( int argc, char** argv ) {
  uint64_t TID        = ( forza_get_my_zone() << 2 ) + forza_get_my_zap();
  // 1 zap/zone: TIDs = 0, 4, 8, 12, ...
  // 2 zaps/zone: TIDs = 0,1; 4,5; 8,9; ...

  uint64_t logical_pe = TID;

  forza_debug_print( logical_pe, 0, 0xabba );

  forza_zone_barrier( forza_get_zaps_per_zone() );

  uint64_t array[10];
  // This sets array[0] to be a "global address - necessary for making it "migratable"
  // uint64_t* migr_addr = (uint64_t*)( (uintptr_t)&array[0] | (1UL << 57) );
  forza_debug_print( logical_pe, (uint64_t) array, 0xcafe );

  for( size_t i = 0; i < 10; i++ ) {
    array[i] = (uint64_t) &array[i];
  }

  //forza_debug_print( logical_pe, (uint64_t)migr_addr, 0xdead );
  forza_debug_print( logical_pe, array[0], 0xdead );

  if( forza_get_my_zone() == 1 ) {
    forza_remote_update( (uintptr_t) &array[0], 0xabcd );
  }

  if( forza_get_my_zone() == 0 ) {
    while( array[0] != 0xabcd )
      ;
  }

  forza_debug_print( logical_pe, (uint64_t) &array[0], array[0] );

  //TRACE_ON;
  //*migr_addr = 0xdecafbad + logical_pe;
  //TRACE_OFF;

#if 0
  uint64_t total_zaps = forza_get_zaps_per_zone() * forza_get_zones_per_precinct();
  for( uint64_t i = 0; i < forza_get_zones_per_precinct(); i++ ) {
    for( uint64_t j = 0; j < forza_get_zaps_per_zone(); j++ ) {
      uint64_t dest_zap = ( i << 2 ) + j;
      if( dest_zap == TID )
        continue;
      forza_debug_print( 0x1122, dest_zap, forza_get_zones_per_precinct() );
      uint64_t src_wd = 0xa1000000 | TID;
      forza_send_word( src_wd, false );
      forza_send_word( 0xbeefUL, false );
      forza_send_word( 0xc0ffeeUL, false );
      uint64_t dest_wd = 0xdecaf0000 | dest_zap;
      forza_send_word( dest_wd, false );
      // Create the control word
      uint64_t dest_pe   = dest_zap;
      uint64_t dest_mbox = 1;
      uint64_t ctrl_word = dest_mbox << 33;
      ctrl_word |= dest_pe;
      uint64_t dest_zone = ( i << 16 );
      ctrl_word |= dest_zone;
      forza_send_word( ctrl_word, true );
    }
  }
#endif

#if 0
  uint64_t msg_array[9] = { 0 };
  zen_stat              = forza_read_zqm_status();
  for( uint64_t j = 0; j < ( total_zaps - 1 ); j++ ) {
    while( zen_stat == 0 )
      zen_stat = forza_read_zqm_status();
    for( unsigned i = 0; i < 8; i++ )
      msg_array[i] = forza_receive_word( 1 );  // dest_mbox above == 1
    forza_debug_print( logical_pe, 0x444, j );
    for( unsigned i = 0; i < 8; i = i + 2 )
      forza_debug_print( 0x9988 + i, msg_array[i], msg_array[i + 1] );
  }
#endif
  return 0;
}
