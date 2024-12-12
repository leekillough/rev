#include "../../../common/syscalls/forza.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

int main( int argc, char** argv ) {
  uint64_t TID = forza_get_my_zap();
  //if (TID != 0)
  //  return 0;

  if( forza_get_my_zone() != 0 )
    return 0;

  if( forza_get_my_zap() >= 2 )
    return 0;

  uint64_t abba       = forza_read_zen_status();

  uint64_t logical_pe = 0xbeef;
#if 1
  if( TID == 0 )
    logical_pe = 0xADUL;
  else
    logical_pe = 0x93UL;
#endif

  forza_debug_print( logical_pe, TID, abba /*0xcafe*/ );
  // forza_zqm_setup( logical_pe, n_mailboxes );
  forza_zqm_setup( logical_pe, 6 );

  //forza_zone_barrier( forza_get_zaps_per_zone() );

  if( TID == 0 ) {
    forza_send_word( 0xa1UL, false );
    forza_send_word( 0xbeefUL, false );
    forza_send_word( 0xcafeUL, false );
    forza_send_word( 0xdecafUL, false );
    // Create the control word
    uint64_t dest_pe   = 0x93UL;
    uint64_t dest_mbox = 1;
    uint64_t ctrl_word = dest_mbox << 33;
    ctrl_word |= dest_pe;
    forza_send_word( ctrl_word, true );
    uint64_t cntrs = forza_zen_get_cntrs();
    forza_debug_print( 0xabcd, cntrs, 0xcdef );
  }

  uint64_t  msg_array[9] = { 0x0123456789 };
  uint64_t* msg_ptr;
  abba = forza_read_zqm_status();
  if( TID == 1 ) {
    while( abba == 0 )
      abba = forza_read_zqm_status();
    msg_ptr = (uint64_t*) forza_receive_word( 1, false );
    memcpy( msg_array, msg_ptr, 6 * sizeof( uint64_t ) );
    abba    = forza_read_zqm_status();
    msg_ptr = (uint64_t*) forza_receive_word( 1, true );
  }
  forza_debug_print( logical_pe, 0xcafe, abba );
  for( unsigned i = 0; i < 9; i = i + 3 )
    forza_debug_print( msg_array[i], msg_array[i + 1], msg_array[i + 2] );

  return 0;
}
