#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"
#include "forza_message_lib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Notes
// * Pthreads only work if one HART per ZAP
// * print statements start with "debug" so that you can grep easily

// Assumptions
// * This works for up to a full precinct with 8 zones and 4 zaps/zone (32 PEs)
// * SPMD model spawns the program at each ZAP

// Test different code paths
#define USE_PTHREAD 0  // 0: call actor_init, 1: forza_thread_create creates pthread to do actor_init

// Specify configuration and number of message sends
#define PE_PER_ZAP  1  // Number of PEs per ZAP since we have SPMD model
#define NSENDS      4UL
#define MAX_PES     32

typedef uint64_t      forza_thread_t;
static forza_thread_t actor_threads[16];

// Extra info for debug
struct ForzaThreadArgs_ {
  uint32_t thread_type;
  uint32_t total_pes;
  uint32_t local_pes;
  uint32_t global_pe;  // single zone so same as local pe
  uint32_t zap;
  uint32_t zone;
  uint32_t local_pe;
  uint32_t n_mailboxes;
  uint32_t n_sends;
  uint32_t n_receives;
};

// 56b packet for sending and receiving messages
struct Packet {
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
  uint64_t v4;
  uint64_t v5;
  uint64_t v6;
  uint64_t v7;
};

// Mailbox types used to specify destination mailbox for message
enum MailBoxType { REQUEST, RESPONSE, NMBOXES };

//enum ForzaThreadType {SENDER, RECEIVER};  // Currently both

// Creates a new pthread. Currently only works if there is one HART per ZAP
static uint32_t forza_thread_create( forza_thread_t* tid, void* fn, void* arg ) {
  uint32_t ret;
  ret = rev_pthread_create( tid, NULL, fn, arg );
  return ret;
}

// Joins a pthread back with its parent
static uint32_t forza_thread_join( forza_thread_t tid ) {
  uint32_t ret;
  ret = rev_pthread_join( tid );
  return ret;
}

// Try to receive request messages.
// Return if no messages available or if DONE message received
uint32_t receive_request_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {

  // Process REQUEST messages
  //   * Adds the value from the messages to a local sum
  //     and tracks the number of messages received
  struct Packet rcv_pkt = { 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };

  do {
    // Check if message available
    uint32_t msg_avail = forza_message_available( REQUEST );
    if( msg_avail == NO_MSG )
      return NO_MSG;

    // Get message
    uint32_t ret = forza_message_receive( REQUEST, (uint64_t*) &rcv_pkt, 7 * sizeof( uint64_t ) );
    if( ret == SUCCESS ) {
      // process message
      *sum += rcv_pkt.v1;
      ( *recv_count )++;

      char msg[65] = "\ndebug:PEX: receive_request_messages: received REQUEST msg C: #R\n";
      msg[9]       = '0' + fargs->global_pe;
      msg[59]      = '0' + rcv_pkt.v2;
      msg[63]      = '0' + *recv_count;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    } else if( ret == DONE_MSG ) {
      // check that we received all messages
      if( *recv_count != fargs->n_sends ) {
        char msg[80] = "\ndebug:PEX: receive_request_messages: bad REQUEST DONE message count: #R\n";
        msg[9]       = '0' + fargs->global_pe;
        msg[71]      = '0' + *recv_count;
        rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
        assert( 0 );
      } else {
        char msg[65] = "\ndebug:PEX: receive_request_messages: received REQUEST DONE msg\n";
        msg[9]       = '0' + fargs->global_pe;
        rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
        *done = 1;
      }
    } else {  // Invalid return status
      char msg[70] = "\ndebug:PEX: receive_request_messages: REQUEST message receive error\n";
      msg[9]       = '0' + fargs->global_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      forza_debug_print( 0xeee, 0xeee, ret );
      assert( 0 );
    }
  } while( !( *done ) );

  return DONE_MSG;
}

// 1. Send request messages to other PE
// 2. Switch to receive any available request messages
// 3. Send DONE message (this simple version doesn't handle failure and try again)
// 4. Switch back to receiving any remaining request messages
// This is basically providing a simple verion of "yield"
// since we don't have separate send and receive threads.
// Ultimately we will use pthread create to solve this.
void process_request_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {

  // Send REQUEST messages to destination PE
  uint64_t      precinct     = 0;
  struct Packet send_pkt     = { 1 + fargs->local_pe, 0, 0, 0, 0, 0, 0 };
  uint64_t      glob_dest_pe = fargs->global_pe + 1;
  if( glob_dest_pe == fargs->total_pes )
    glob_dest_pe = 0;
  uint64_t dest_pe   = glob_dest_pe % fargs->local_pes;
  uint64_t dest_zone = glob_dest_pe / ( PE_PER_ZAP * forza_get_zaps_per_zone() );

  for( uint32_t i = 0; i < fargs->n_sends; i++ ) {
    send_pkt.v2       = i;
    uint32_t sendstat = forza_send( REQUEST, (uint64_t*) &send_pkt, precinct, dest_zone, dest_pe );
    if( sendstat != SUCCESS ) {
      char msg[65] = "\ndebug:PEX: send_request_messages: REQUEST msg send C failed\n";
      msg[9]       = '0' + fargs->global_pe;
      msg[52]      = '0' + i;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    } else {
      char msg[65] = "\ndebug:PEX: send_request_messages: REQUEST msg send C succeeded\n";
      msg[9]       = '0' + fargs->global_pe;
      msg[52]      = '0' + i;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    }
  }

  // See if any messages to receive before we try to do done
  uint32_t recv_stat = receive_request_messages( fargs, recv_count, sum, done );

  // See if we can send DONE
  // skk TODO
  // uint64_t outstanding = 0;
  // uint32_t result = forza_get_outstanding(mb_id, &outstanding);
  // Currently doesn't handle failure to send done
  // Should check # outstanding messages. If not zero try receive then check again
  // Once there are no outstanding messages left, it can send done and go back
  // to receiving.
  uint32_t sendstat  = forza_send_done( REQUEST, precinct, dest_zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[70] = "\ndebug:PEX: send_request_messages: REQUEST msg send done failed\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[70] = "\ndebug:PEX: send_request_messages: REQUEST msg send done succeeded\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Receive any additional request messages
  while( recv_stat != DONE_MSG ) {
    recv_stat = receive_request_messages( fargs, recv_count, sum, done );
  }
}

// Try to receive response messages
// Returns if no messages available or when DONE message received
uint32_t receive_response_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {
  // Process RESPONSE messages
  struct Packet rcv_pkt = { 0, 0, 0, 0, 0, 0, 0 };

  do {
    // Wait for message
    uint32_t msg_avail = forza_message_available( RESPONSE );
    if( msg_avail == NO_MSG )
      return NO_MSG;

    // Get message
    uint32_t ret = forza_message_receive( RESPONSE, (uint64_t*) &rcv_pkt, 7 * sizeof( uint64_t ) );
    if( ret == SUCCESS ) {
      // process message
      char msg[65] = "\ndebug:PEX: receive_response_message: received RESPONSE msg\n";
      msg[9]       = '0' + fargs->global_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      *recv_count += rcv_pkt.v1;
      *sum += rcv_pkt.v2;
    } else if( ret == DONE_MSG ) {
      // check that we received all messages
      char msg[65] = "\ndebug:PEX: receive_response_message: received RESPONSE DONE msg\n";
      msg[9]       = '0' + fargs->global_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      *done = 1;
    } else {  // Invalid return status
      char msg[70] = "\ndebug:PEX: receive_response_message: RESPONSE message receive error\n";
      msg[9]       = '0' + fargs->global_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      assert( 0 );
    }
  } while( !( *done ) );

  {
    char msg[70] = "\ndebug:PEX: receive_response_message: RESPONSE msgcount = X sum = Y\n";
    msg[9]       = '0' + fargs->global_pe;
    msg[58]      = '0' + *recv_count;
    msg[66]      = '0' + *sum;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }
  {
    char msg[70] = "\ndebug:PEX: receive_response_message: EXPECTED msgcount = X sum = Y\n";
    msg[9]       = '0' + fargs->global_pe;
    msg[58]      = '0' + fargs->n_sends;
    msg[66]      = '0' + ( fargs->n_sends * ( fargs->local_pe + 1 ) );
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Check Results
  uint32_t expected_sum = fargs->n_sends * ( fargs->local_pe + 1 );
  if( *recv_count != fargs->n_sends )
    assert( 0 );
  if( *sum != expected_sum )
    assert( 0 );

  return DONE_MSG;
}

// Similar to processing of request messages
// Sends respone message, receives available messages, tries to send DONE message, receives again
uint32_t process_response_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {
  // Send single ESPONSE message containing the number of received messages and
  // the computed sum
  uint64_t      precinct     = 0;
  struct Packet send_pkt     = { *recv_count, *sum, 0, 0, 0, 0, 0 };
  uint64_t      glob_dest_pe = fargs->global_pe - 1;
  if( fargs->global_pe == 0 )
    glob_dest_pe = fargs->total_pes - 1;
  uint64_t dest_pe   = glob_dest_pe % fargs->local_pes;
  uint64_t dest_zone = glob_dest_pe / ( PE_PER_ZAP * forza_get_zaps_per_zone() );

  uint32_t sendstat  = forza_send( RESPONSE, (uint64_t*) &send_pkt, precinct, dest_zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[60] = "\ndebug:PEX: actor thread init: RESPONSE msg send failed\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[60] = "\ndebug:PEX: actor thread init: RESPONSE msg send succeeded\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Reset for receive
  *recv_count        = 0;
  *sum               = 0;
  *done              = 0;

  // See if any messages to receive before we try to do done
  uint32_t recv_stat = receive_response_messages( fargs, recv_count, sum, done );

  // See if we can send DONE
  sendstat           = forza_send_done( RESPONSE, precinct, dest_zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[65] = "\ndebug:PEX: actor thread init: RESPONSE msg send done failed\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[65] = "\ndebug:PEX: actor thread init: RESPONSE msg send done succeeded\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  while( recv_stat != DONE_MSG ) {
    recv_stat = receive_response_messages( fargs, recv_count, sum, done );
  }

  return DONE_MSG;
}

//-----------------------------------------------------------------------
// Implements the entire actor thread
//   * Registers the thread with the ZQM
//   * Processes REQUEST messages: Send and receive
//   * Processes RESPONSE messages: Send and receive
//
// May be called as a function or passed as function pointer to forza_thread_create
//
void* actor_thread_init( struct ForzaThreadArgs_* fargs ) {

  uint32_t recv_count = 0;
  uint32_t sum        = 0;
  uint32_t done       = 0;

  {
    char msg[50] = "\ndebug:PEX: actor thread init\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Register this PE with ZQM
  uint32_t ret = forza_zqm_pe_setup( fargs->local_pe, fargs->n_mailboxes );
  if( ret ) {
    char msg[55] = "\ndebug:PEX: actor thread init: zqm setup failed\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );  // Shouldn't fail in simulator
  } else {
    char msg[55] = "\ndebug:PEX: actor thread init: zqm setup succeeded\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Wait for all threads to complete registrations
  forza_zone_barrier( forza_get_zaps_per_zone() );
  {
    char msg[55] = "\ndebug:PEX: actor thread init: after barrier1\n";
    msg[9]       = '0' + fargs->global_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Send and receive request messages
  process_request_messages( fargs, &recv_count, &sum, &done );
  {
    char msg[60] = "\ndebug:PEX: actor thread init: REQUEST msgcount = X sum = Y\n";
    msg[9]       = '0' + fargs->global_pe;
    msg[50]      = '0' + recv_count;
    msg[58]      = '0' + sum;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Send and receive response messages
  process_response_messages( fargs, &recv_count, &sum, &done );

  // Wait for all threads to complete receiving messages
  forza_zone_barrier( forza_get_zaps_per_zone() );

  return NULL;
}

//--------------------------------------------------------------------------------------------
int main() {

  uint64_t n_zaps      = forza_get_zaps_per_zone();
  uint64_t n_zones     = forza_get_zones_per_precinct();
  uint64_t n_precincts = forza_get_num_precincts();
  uint64_t n_local_pes = PE_PER_ZAP * n_zaps;  // PEs per zone
  uint64_t zap_id      = forza_get_my_zap();
  uint64_t zone_id     = forza_get_my_zone();
  uint64_t precinct_id = forza_get_my_precinct();

  if( n_zaps * n_zones * n_precincts > MAX_PES ) {
    char msg[50] = "\ndebug: System size exceeds MAX_PEs\n";
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  }

  {
    char msg[50] = "\ndebug:ZAPX: ZONEX: main: Enter main\n";
    msg[10]      = '0' + zap_id;
    msg[17]      = '0' + zone_id;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }
  {
    char msg[55] = "\ndebug:ZAPX: main: n_zones=W, n_zaps=X, n_local_pes=Y\n";
    msg[27]      = '0' + n_zones;
    msg[37]      = '0' + n_zaps;
    msg[52]      = '0' + n_local_pes;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Create local actor threads (PEs)
  // Currently function call, can also use forza_thread_create
  struct ForzaThreadArgs_ FArgs[MAX_PES];  // one for each PE

  // This is kind of weird right now because of the SPMD model that spawns at each ZAP
  // Currently spawning only one thread for each zap
  for( uint32_t j = 0; j < PE_PER_ZAP; j++ ) {  // Each thread at a zap will create the local thread(s)
    //index = Zone * PE/ZONE + ZAP * PE/ZAP;
    //PE/ZONE = PE/ZAP * ZAP/ZONE;
    uint32_t index           = zone_id * n_zaps * PE_PER_ZAP + zap_id * PE_PER_ZAP + j;
    FArgs[index].thread_type = 0;
    FArgs[index].total_pes   = PE_PER_ZAP * n_zaps * n_zones;
    FArgs[index].local_pes   = n_local_pes;
    FArgs[index].global_pe   = index;
    FArgs[index].zap         = zap_id;
    FArgs[index].zone        = zone_id;
    FArgs[index].local_pe    = index % n_local_pes;  // PE within zone
    FArgs[index].n_mailboxes = NMBOXES;
    FArgs[index].n_sends     = NSENDS;

    {
      char msg[55] = "\ndebug:Px:ZONEx:ZAPx:LPEx main: index=y\n";
      msg[8]       = '0' + precinct_id;
      msg[14]      = '0' + zone_id;
      msg[19]      = '0' + zap_id;
      msg[24]      = '0' + FArgs[index].local_pe;
      msg[38]      = '0' + index;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    }

#if USE_PTHREAD
    forza_thread_create( &actor_threads[index], (void*) actor_thread_init, (void*) &( FArgs[index] ) );
#else
    actor_thread_init( &( FArgs[index] ) );
#endif
  }

#if USE_PTHREAD
  for( uint32_t j = 0; j < PE_PER_ZAP; j++ ) {
    uint32_t index = zap_id * PE_PER_ZAP + j;
    forza_thread_join( actor_threads[index] );
  }
#endif

  {
    char msg[30] = "\ndebug:ZAPX: main: complete\n";
    msg[10]      = '0' + zap_id;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  return 0;
}
