#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "forza_message_lib.h"

int main( int argc, char** argv ) {
  uint64_t TID = forza_get_my_zap();
  uint64_t zen_status = forza_read_zen_status();
	uint64_t logical_pe = 0x0UL;

  if( TID == 0 )
    logical_pe = 0xaUL;
  else
    logical_pe = 0xbUL;

  // Debug 0x1111 main: logical_pe and TIC
  {char msg[45] = "\ndebug:TX: main: 0x1111, logical_pe, TID\n";
	msg[8] = '0' + TID;
  rev_write(STDOUT_FILENO, msg, sizeof(msg));}
	forza_debug_print( 0x1111, logical_pe, TID );
	
	// Register the locical PE and number of mailboxes with the ZQM
  int ret = forza_zqm_pe_setup( logical_pe, 6 );
	if (ret)
		assert(0);

  // Wait for all zaps to complete
  forza_zone_barrier( forza_get_zaps_per_zone() );

  // TID 0 (PE 0x10) sends message to TID 1 (PE 0x20)
  if( TID == 0 ) {  
		uint64_t mb_id = 1UL;
		uint64_t data[7] = {0xa1UL, 0xbeefUL, 0xcafeUL, 0xdecafUL, 0xdeadUL, 0xdead1UL, 0xdead2UL};
		uint64_t precinct = 0UL;
		uint64_t zone = 0UL;
		uint64_t dest_pe = 0xbUL;
		forza_send(mb_id, data, precinct, zone, dest_pe);
#if 0  // send another message
		data[0] = 0xb1UL;
		forza_send(mb_id, data, precinct, zone, dest_pe);
#endif
		forza_send_done(mb_id, precinct, zone, dest_pe);
 	}

	// TID 1 Receives messages
	uint64_t mb_id = 1UL;
	uint64_t rcv_pkt[7]; 
	if ( TID == 1 ) {

		// Check mailbox for messages
		int msg_avail = forza_message_available(mb_id);
		while (msg_avail == NO_MSG)
			msg_avail = forza_message_available(mb_id);

	  // Get first message
		int result1 = forza_message_receive(mb_id, rcv_pkt, 7 * sizeof(uint64_t));
		if (result1 != SUCCESS)
			assert(0);

#if 0
 		// Get second message
    result1 = forza_message_receive(mb_id, rcv_pkt, 7 * sizeof(uint64_t));
    if (result1 != SUCCESS)
      assert(0);
#endif


		// Check mailbox for messages
    msg_avail = forza_message_available(mb_id);
    while (msg_avail == NO_MSG)
      msg_avail = forza_message_available(mb_id);

		// Get DONE message
		int result2 = forza_message_receive(mb_id, rcv_pkt, 0);
		if (result2 != DONE_MSG)
			assert(0);
		
}
	// Debug 0x4444 main: print received message
  {char msg[40] = "\ndebug:TX: main: print received pkt\n";
	msg[8] = '0' + TID;
  rev_write(STDOUT_FILENO, msg, sizeof(msg));}
	if (TID == 0) {
		char msg[70] = "\ndebug:T0: main: Expected 0, 0, 0, 0, 0, 0, 0, 0\n";
		rev_write(STDOUT_FILENO, msg, sizeof(msg));
  } else {
		char msg[70] = "\ndebug:T1: main: Expected a1, beef, cafe, decaf, dead, dead1, dead2\n";
		rev_write(STDOUT_FILENO, msg, sizeof(msg));
	}
  //forza_debug_print( 0x1111, logical_pe, TID );
  //forza_debug_print( 0x4444UL, 0x4444UL, 0x4444UL);
	forza_debug_print( rcv_pkt[0], rcv_pkt[1], rcv_pkt[2] );
 	forza_debug_print( rcv_pkt[3], rcv_pkt[4], rcv_pkt[5] );
	forza_debug_print( rcv_pkt[6], 0x0, 0x0 );
		
  return 0;
}
