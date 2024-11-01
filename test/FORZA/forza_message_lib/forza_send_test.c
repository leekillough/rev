#include "../../../common/syscalls/forza.h"
#include "forza_message_lib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// NOTE: Requires #HARTs > logical_pe number (<0xb)

int main( int argc, char** argv ) {
  uint64_t TID        = forza_get_my_zap();
  uint64_t zen_status = forza_read_zen_status();
  uint64_t logical_pe = 0x0UL;

  if( TID == 0 )
    logical_pe = 0xaUL;
  else
    logical_pe = 0xbUL;

  // Debug 0x1111 main: logical_pe and TIC
  forza_debug_print( 0x1111, logical_pe, TID );
  // forza_zqm_setup( logical_pe, n_mailboxes );

  // Register the locical PE and number of mailboxes with the ZQM
  forza_zqm_setup( logical_pe, 6 );

  // Wait for all zaps to complete
  forza_zone_barrier( forza_get_zaps_per_zone() );

  uint64_t data[7] = { 0xa1UL, 0xbeefUL, 0xcafeUL, 0xdecafUL, 0xdeadUL, 0xdead1UL, 0xdead2UL };

  // TID 0 (PE 0x10) sends message to TID 1 (PE 0x20)
  if( TID == 0 ) {
    uint64_t mb_id    = 1UL;
    uint64_t precinct = 0UL;
    uint64_t zone     = 0UL;
    uint64_t dest_pe  = 0xbUL;
    forza_send( mb_id, data, precinct, zone, dest_pe );
    forza_send_done( mb_id, precinct, zone, dest_pe );
  }

  // TID 1 Receives messages
  uint64_t mb_id = 1UL;
  uint64_t rcv_pkt[7];
  if( TID == 1 ) {

    // Check mailbox for messages
    int msg_avail = forza_message_available( mb_id );
    while( msg_avail == NO_MSG )
      msg_avail = forza_message_available( mb_id );
    // Get first message
    int result1 = forza_message_receive( mb_id, rcv_pkt, 7 * sizeof( uint64_t ) );
    if( result1 != SUCCESS )
      assert( 0 );

    // Check mailbox for messages
    msg_avail = forza_message_available( mb_id );
    while( msg_avail == NO_MSG )
      msg_avail = forza_message_available( mb_id );
    // Get DONE message
    int result2 = forza_message_receive( mb_id, rcv_pkt, 0 );
    if( result2 != DONE_MSG )
      assert( 0 );
    forza_debug_print( 0x4442, result1, result2 );

    // Check results
    for( int i = 0; i < 7; i++ )
      assert( rcv_pkt[i] == data[i] );
  }

  // Debug 0x4444 main: print received message
  forza_debug_print( 0x4444UL, 0x4444UL, 0x4444UL );
  forza_debug_print( rcv_pkt[0], rcv_pkt[1], rcv_pkt[2] );
  forza_debug_print( rcv_pkt[3], rcv_pkt[4], rcv_pkt[5] );
  forza_debug_print( rcv_pkt[6], 0x0, 0x0 );

  return 0;
}
