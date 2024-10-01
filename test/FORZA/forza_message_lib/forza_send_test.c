#include "../../../common/syscalls/forza.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

#define NMBOX 8UL  // HW restricts the number of mailboxes to 8
#define ZQM_COMPONENT  
enum MSG_OP {
	DIRECT_OP,
	INDIRECT_OP,
	DONE_OP
};
#define MSG_OP_MASK 0x0000FF0000000000UL   // MSG_OP [47..40]

enum RET_STATUS {
  SUCCESS,
  DONE_MSG,
	NO_MSG,
  INVALID_NUM_MBOX,
  INVALID_MBOX,
  INVALID_LPE,
  INVALID_PRECINCT,
  INVALID_ZONE,
  ZEN_NOT_READY,
  INVALID_PTR,
	INVALID_PKT_SZ,
  MALLOC_FAIL
};

int forza_send(uint64_t mb_id, void * pkt, uint64_t precinct, uint64_t zone, uint64_t logical_pe) {

	// Currently assumes single AID that can have up to #HARTS/2 PEs per zone
  int max_pes = forza_get_harts_per_zap()/2;

	// Check for valid inputs
	if (mb_id >= NMBOX)
		return INVALID_MBOX;
	if (pkt == NULL)
		return INVALID_PTR;
	if (precinct >= forza_get_num_precincts())
		return INVALID_PRECINCT;
	if (zone >= forza_get_zones_per_precinct())
		return INVALID_ZONE;
	if (logical_pe >= max_pes)
 		return INVALID_LPE;
 
	// Check zen status
	uint64_t zenstat = forza_read_zen_status();
  // how to check it?

 	// Send 7 data words
	uint64_t * data = pkt;
	for (int i=0; i<7; i++)
 		forza_send_word( data[i], false );
  
	// Create the control word
  uint64_t ctrl_word = mb_id << 33;
	ctrl_word |= precinct << 20;
	ctrl_word != zone << 16;
	//ctrl_word |= ZQM_COMPONENT << 12; 
  ctrl_word |= logical_pe;
  forza_send_word( ctrl_word, true );

	uint64_t cntrs = forza_zen_get_cntrs();
 
 	// Debug 0x2222 send: control word and counter
	forza_debug_print( 0x2222, ctrl_word, cntrs);

	return SUCCESS;
}

int forza_send_done(uint64_t mb_id, uint64_t precinct, uint64_t zone, uint64_t logical_pe) {

  // Currently assumes single AID that can have up to #HARTS/2 PEs per zone
  int max_pes = forza_get_harts_per_zap()/2;

  // Check for valid inputs
  if (mb_id >= NMBOX)
    return INVALID_MBOX;
  if (precinct >= forza_get_num_precincts())
    return INVALID_PRECINCT;
  if (zone >= forza_get_zones_per_precinct())
    return INVALID_ZONE;
  if (logical_pe >= max_pes)
    return INVALID_LPE;

  // Check zen status
  uint64_t zenstat = forza_read_zen_status();
  // how to check it?

  // Create the control word
  uint64_t ctrl_word = DONE_OP;
	ctrl_word = ctrl_word << 40;
	ctrl_word |= mb_id << 33;
  ctrl_word |= precinct << 20;
  ctrl_word != zone << 16;
  //ctrl_word |= ZQM_COMPONENT << 12;
  ctrl_word |= logical_pe;
  forza_send_word( ctrl_word, true );

  uint64_t cntrs = forza_zen_get_cntrs();

  // Debug 0x2222 send: control word and counter
  forza_debug_print( 0x2222, ctrl_word, cntrs);

  return SUCCESS;
}

int forza_message_available(uint64_t mb_id) {

	// Check for valid inputs
  if (mb_id >= NMBOX)
    return INVALID_MBOX;
		
	// Check for message
	uint64_t zqmstat = forza_read_zqm_status();
  if ( zqmstat == 0 ) {
    return NO_MSG;
	} else {
		return SUCCESS;
	}
}

int forza_message_receive(uint64_t mb_id, uint64_t *pkt, size_t pkt_size) {

  // Note: Ignoring pkt_size for now, always reads all 8 words

  // Check for valid inputs
  if (mb_id >= NMBOX)
    return INVALID_MBOX;
  if (pkt == NULL)
    return INVALID_PTR;
	if (pkt_size > 7 * sizeof(uint64_t))
		return INVALID_PKT_SZ;

	// Wait for ZQM ready
  uint64_t zqmstat = forza_read_zqm_status();
#if 1
	if ( zqmstat == 0 )
		return NO_MSG;
#else
  while( zqmstat == 0 )
		zqmstat = forza_read_zqm_status();
#endif
	// Read control word and check op
	uint64_t ctrl_word = forza_receive_word ( mb_id );
  uint64_t msg_op = (ctrl_word & MSG_OP_MASK) >> 40;
	if (msg_op == DIRECT_OP) {
		// Read all 7 data words for now (ignore packet size)
		for( int i = 0; i < 7; i++ )
    	pkt[i] = forza_receive_word( mb_id ); 

		// Debug 0x3333 receive: msg_op and ZQM status
		forza_debug_print ( 0x3333, msg_op, zqmstat );
		return SUCCESS;
	} else if (msg_op == INDIRECT_OP) {
		// Debug 0x3333 receive: msg_op and ZQM status
    forza_debug_print ( 0x3333, msg_op, zqmstat );
    assert(0);  // INDIRECT msg op not yet supported
	} else if (msg_op == DONE_OP) {
		forza_debug_print ( 0x3333, msg_op, zqmstat );
    return DONE_MSG;
	} else {
		forza_debug_print ( 0x3333, msg_op, zqmstat );
    assert(0);  // Invalid message op
	}
}

int main( int argc, char** argv ) {
  uint64_t TID = forza_get_my_zap();
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

  // TID 0 (PE 0x10) sends message to TID 1 (PE 0x20)
  if( TID == 0 ) {  
		uint64_t mb_id = 1UL;
		uint64_t data[7] = {0xa1UL, 0xbeefUL, 0xcafeUL, 0xdecafUL, 0xdeadUL, 0xdead1UL, 0xdead2UL};
		uint64_t precinct = 0UL;
		uint64_t zone = 0UL;
		uint64_t dest_pe = 0xbUL;
		forza_send(mb_id, data, precinct, zone, dest_pe);
		forza_send_done(mb_id, precinct, zone, dest_pe);
 	}

	// TID 1 Receives messages
	uint64_t mb_id = 1UL;
	uint64_t rcv_pkt[7]; 
	if ( TID == 1 ) {
#if 1
		// Check mailbox for messages
		int msg_avail = forza_message_available(mb_id);
		while (msg_avail == NO_MSG)
			msg_avail = forza_message_available(mb_id);
#endif
	  // Get first message
		int result1 = forza_message_receive(mb_id, rcv_pkt, 7 * sizeof(uint64_t));
		if (result1 != SUCCESS)
			assert(0);

#if 1
		// Check mailbox for messages
    msg_avail = forza_message_available(mb_id);
    while (msg_avail == NO_MSG)
      msg_avail = forza_message_available(mb_id);
#endif
		// Get DONE message
		int result2 = forza_message_receive(mb_id, rcv_pkt, 0);
		if (result2 != DONE_MSG)
			assert(0);
		forza_debug_print(0x4442, result1, result2); 
	}

	// Debug 0x4444 main: print received message
  forza_debug_print( 0x4444UL, 0x4444UL, 0x4444UL);
	forza_debug_print( rcv_pkt[0], rcv_pkt[1], rcv_pkt[2] );
 	forza_debug_print( rcv_pkt[3], rcv_pkt[4], rcv_pkt[5] );
	forza_debug_print( rcv_pkt[6], 0x0, 0x0 );
		
  return 0;
}
