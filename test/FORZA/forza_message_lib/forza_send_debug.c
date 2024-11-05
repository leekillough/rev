#include "../../../common/syscalls/forza.h"
#include "forza_message_lib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// NOTE: Requires #HARTs > logical_pe number (<0xb)

// DEBUG: This works for ONE_THD == 1: i.e. if I have only TID 0 send and only TID 1 reeive
#define ONE_THD   0
#define TEST_DONE 1

int main( uint32_t argc, char** argv ) {
  uint64_t TID        = forza_get_my_zap();
  uint64_t zen_status = forza_read_zen_status();
  uint64_t logical_pe = TID;

  // Debug 0x1111 main: logical_pe and TIC
  forza_debug_print( 0x1111, logical_pe, TID );
  // forza_zqm_setup( logical_pe, n_mailboxes );

  // Register the locical PE and number of mailboxes with the ZQM
  forza_zqm_setup( logical_pe, 6 );

  // Wait for all zaps to complete
  forza_zone_barrier( forza_get_zaps_per_zone() );

  // TID 0 (PE 0x10) sends message to TID 1 (PE 0x20)
  uint64_t mb_id   = 1UL;
  uint64_t data[7] = { 0xa1UL, 0xbeefUL, 0xcafeUL, 0xdecafUL, 0xdeadUL, 0xdead1UL, 0xdead2UL };
#if ONE_THD
  if( TID == 0 ) {
#endif
    uint64_t precinct = 0UL;
    uint64_t zone     = 0UL;
    uint64_t dest_pe;
    if( TID == 0 ) {
      data[0] = 0xa1UL;
      dest_pe = 1;
    } else {
      data[0] = 0xa0UL;
      dest_pe = 0;
    }

    forza_send( mb_id, data, precinct, zone, dest_pe );
#if TEST_DONE
    forza_send_done( mb_id, precinct, zone, dest_pe );
#endif
#if ONE_THD
  }
#endif

  // TID 1 Receives messages
  uint64_t rcv_pkt[7];
#if ONE_THD
  if( TID == 1 ) {
#endif
    // Check mailbox for messages
    uint32_t msg_avail = forza_message_available( mb_id );
    while( msg_avail == NO_MSG )
      msg_avail = forza_message_available( mb_id );
    // Get first message
    uint32_t result1 = forza_message_receive( mb_id, rcv_pkt, 7 * sizeof( uint64_t ) );
    if( result1 != SUCCESS )
      assert( 0 );
#if TEST_DONE
    // Check mailbox for messages
    msg_avail = forza_message_available( mb_id );
    while( msg_avail == NO_MSG )
      msg_avail = forza_message_available( mb_id );
    // Get DONE message
    uint32_t result2 = forza_message_receive( mb_id, rcv_pkt, 0 );
    if( result2 != DONE_MSG )
      assert( 0 );
    forza_debug_print( 0x4442, result1, result2 );
#endif
#if ONE_THD
  }
#endif

  // Check result
  if( TID == 0 ) {
    assert( rcv_pkt[0] == 0xa0UL );
  } else {
    assert( rcv_pkt[0] == 0xa1UL );
  }

  for( uint32_t i = 1; i < 7; i++ )
    assert( rcv_pkt[i] == data[i] );

  // Debug 0x4444 main: print received message
  forza_debug_print( 0x4444UL, 0x4444UL, 0x4444UL );
  forza_debug_print( rcv_pkt[0], rcv_pkt[1], rcv_pkt[2] );
  forza_debug_print( rcv_pkt[3], rcv_pkt[4], rcv_pkt[5] );
  forza_debug_print( rcv_pkt[6], 0x0, 0x0 );

  return 0;
}
