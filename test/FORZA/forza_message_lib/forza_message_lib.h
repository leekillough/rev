/**
 * @file forza_message_lib.h
 * @brief Interface to HW for low-level actor message send and receive support
 */


#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

#define DEBUG 0
#define NMBOX 8UL  // HW restricts the number of mailboxes to 8
#define ZQM_COMPONENT 0UL

/**
 * @brief Message op types
 */
enum MSG_OP {
	DIRECT_OP,
	INDIRECT_OP,
	DONE_OP
};
#define MSG_OP_MASK 0x0000FF0000000000UL   // MSG_OP [47..40]

/**
 * @brief Return Status Codes
 */
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

/**
 * @brief returns the number of logical PEs supported in HW for this zone
 *
 * Currently assumes singe AID that can use the total number of HARTs in the zone
 * Eventually this will be reduced to some subset of harts and may be
 * divided by AID
 */
int forza_get_num_logical_pes() {

	return(forza_get_harts_per_zap() * forza_get_zaps_per_zone());
}

/**
 * @brief Registers this thread's logical PE with the ZQM so that it can correctly
 * 	deliver messages 
 *
 */
int forza_zqm_pe_setup( uint64_t logical_pe, uint64_t n_mailboxes) {

  // Check inputs
  if (logical_pe > forza_get_num_logical_pes() - 1) {
	  #if DEBUG
		char msg[55] = "\ndebug: forza_zqm_pe_setup: INVALID_LPE\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
		#endif
    return INVALID_LPE;
	}
  if (n_mailboxes > NMBOX) {
		#if DEBUG
		char msg[55] = "\ndebug: forza_zqm_pe_setup: INVALID_NUM_MBOX\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
		#endif
    return INVALID_NUM_MBOX;
	}
  forza_zqm_setup(logical_pe, n_mailboxes);
  return SUCCESS;
}


/**
 * @brief Send a packet to the specified mailbox at the destination precinct:zone:logical_pe
 *
 * The packet must point to 56b of payload and will be sent as 7 64b words
 *
 */
int forza_send(uint64_t mb_id, void * pkt, uint64_t precinct, uint64_t zone, uint64_t logical_pe) {

	// Check for valid inputs
	if (mb_id >= NMBOX) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_send: INVALID_MBOX\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
		#endif
		return INVALID_MBOX;
	}
	if (pkt == NULL) {
	  #if DEBUG
    char msg[60] = "\ndebug: forza_send: INVALID_PTR, pkt == NULL\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
		#endif
		return INVALID_PTR;
	}
	if (precinct >= forza_get_num_precincts()) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_send: INVALID_PRECINCT\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
		return INVALID_PRECINCT;
	}
	if (zone >= forza_get_zones_per_precinct()) {
	  #if DEBUG
    char msg[55] = "\ndebug: forza_send: INVALID_ZONE\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
		return INVALID_ZONE;
	}
	if (logical_pe >= forza_get_num_logical_pes()) {
	 	#if DEBUG
    char msg[55] = "\ndebug: forza_send: INVALID_LPE\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
 		return INVALID_LPE;
 	}

	// Check zen status
	uint64_t zenstat = forza_read_zen_status();
  if (zenstat == 0) {
		char msg[55] = "\ndebug: forza_send: zenstat error\n";
  	rev_write(STDOUT_FILENO, msg, sizeof(msg));
		assert(0); 
	}

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
 
 	// Debug 0x2222 send: control word and counteri
	#if DEBUG
	{char msg[50] = "\ndebug: forza_send: 0x2222, ctrl_word, counters\n";
  rev_write(STDOUT_FILENO, msg, sizeof(msg));}
	forza_debug_print( 0x2222, ctrl_word, cntrs);
	#endif

	return SUCCESS;
}


/** @brief Sends a done message to the mailbox at the destination precinct:zone:logocal_pe
 *   to indiocate that no more messages will be sent from this PE to the destination mailbox
 *
 */
int forza_send_done(uint64_t mb_id, uint64_t precinct, uint64_t zone, uint64_t logical_pe) {

  // Check for valid inputs
  if (mb_id >= NMBOX) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_send_done: INVALID_MBOX\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_MBOX;
	}
	if (precinct >= forza_get_num_precincts()) {
    #if DEBUG
    char msg[55] = "\ndebug: forza_send_done: INVALID_PRECINCT\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_PRECINCT;
  }
  if (zone >= forza_get_zones_per_precinct()) {
    #if DEBUG
    char msg[55] = "\ndebug: forza_send_done: INVALID_ZONE\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_ZONE;
  }
  if (logical_pe >= forza_get_num_logical_pes()) {
    #if DEBUG
    char msg[55] = "\ndebug: forza_send_done: INVALID_LPE\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_LPE;
  }

  // Check zen status
  uint64_t zenstat = forza_read_zen_status();
  if (zenstat == 0) {
		char msg[55] = "\ndebug: forza_send_done: zen stat error\n";
  	rev_write(STDOUT_FILENO, msg, sizeof(msg));
		assert(0);  // Fix this later to check status bits. This really shouldn't happen now.
	}

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
	#if DEBUG
	{char msg[55] = "\ndebug: forza_send_done: 0x2222, ctrl_word, counters\n";
  rev_write(STDOUT_FILENO, msg, sizeof(msg));}
	forza_debug_print( 0x2222, ctrl_word, cntrs);
	#endif

  return SUCCESS;
}

/**
 * @brief Checks to see if a message is available at the given mailbox
 *
 */
int forza_message_available(uint64_t mb_id) {

	// Check for valid inputs
  if (mb_id >= NMBOX) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_message_available: INVALID_MBOX\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_MBOX;
	}

	// Check for message
	uint64_t zqmstat = forza_read_zqm_status();
  if ( zqmstat == 0 ) {
    return NO_MSG;
	} else {
		return SUCCESS;
	}
}

/** 
 * @brief Copies the first message from the specified mailbox into the location
 * pointed to by the pkt pointer. Note that it currently copies 56b and ignores
 * the packet size if it is less than that.
 */
int forza_message_receive(uint64_t mb_id, uint64_t *pkt, size_t pkt_size) {

  // Note: Ignoring pkt_size for now, always reads all 8 words

  // Check for valid inputs
  if (mb_id >= NMBOX) {
	 	#if DEBUG
    char msg[55] = "\ndebug: forza_message_receive: INVALID_MBOX\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_MBOX;
	}
  if (pkt == NULL) {
		#if DEBUG
    char msg[60] = "\ndebug: forza_message_receive: INVALID_PTR, pkt == NULL\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
    return INVALID_PTR; 
	}
	if (pkt_size > 7 * sizeof(uint64_t)) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_message_receive: INVALID_PKT_SZ\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
		return INVALID_PKT_SZ;
	}

	// Wait for ZQM ready
  uint64_t zqmstat = forza_read_zqm_status();
	if ( zqmstat == 0 ) {
		#if DEBUG
    char msg[55] = "\ndebug: forza_message_receive: NO_MSG\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
    #endif
		return NO_MSG;
	}

	// Read control word and check op
	uint64_t ctrl_word = forza_receive_word ( mb_id );
  uint64_t msg_op = (ctrl_word & MSG_OP_MASK) >> 40;
	if (msg_op == DIRECT_OP) {
		// Read all 7 data words for now (ignore packet size)
		for( int i = 0; i < 7; i++ )
    	pkt[i] = forza_receive_word( mb_id ); 

		// Debug 0x3333 receive: msg_op and ZQM status
		#if DEBUG
		{char msg[40] = "\ndebug: forza_message_receive: SUCCESS\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));}
		forza_debug_print ( 0x3333, msg_op, zqmstat );
		#endif
		return SUCCESS;
	} else if (msg_op == INDIRECT_OP) {
		// Debug 0x3333 receive: msg_op and ZQM status
		{char msg[70] = "\ndebug: forza_message_receive: INDIRECT_OP not supported\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));}
    forza_debug_print ( 0x3333, msg_op, zqmstat );
    assert(0);  // INDIRECT msg op not yet supported
	} else if (msg_op == DONE_OP) {
	  #if DEBUG
		{char msg[40] = "\ndebug: forza_message_receive: DONE_OP\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));}
		forza_debug_print ( 0x3333, msg_op, zqmstat );
		#endif
    return DONE_MSG;
	} else {
		{char msg[50] = "\ndebug: forza_message_receive: Invalid message op\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));}
		forza_debug_print ( 0x3333, msg_op, zqmstat );
    assert(0);  // Invalid message op
	}
}

