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
// * N Precints: 1
// * N Zones/precint: 1
// * SPMD model spawns the program at each ZAP

// Test different code paths
#define USE_PTHREAD 0  // 0: call actor_init, 1: forza_thread_create creates pthread to do actor_init

// Specify configuration and number of message sends
#define PE_PER_ZAP  1  // Number of PEs per ZAP since we have SPMD model
#define NSENDS      4UL

typedef unsigned long int forza_thread_t;
static forza_thread_t     actor_threads[16];

// Extra info for debug
struct ForzaThreadArgs_ {
  int thread_type;
  int total_pes;
  int global_pe;  // single zone so same as local pe
  int zap;
  int local_pe;
  int n_mailboxes;
  int n_sends;
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
static int forza_thread_create( forza_thread_t* tid, void* fn, void* arg ) {
  int ret;
  ret = rev_pthread_create( tid, NULL, fn, arg );
  return ret;
}

// Joins a pthread back with its parent
static int forza_thread_join( forza_thread_t tid ) {
  int ret;
  ret = rev_pthread_join( tid );
  return ret;
}

// Try to receive request messages.
// Return if no messages available or if DONE message received
int receive_request_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {

  // Process REQUEST messages
  //   * Adds the value from the messages to a local sum
  //     and tracks the number of messages received
  struct Packet rcv_pkt = { 0, 0, 0, 0, 0, 0, 0 };

  do {
    // Check if message available
    int msg_avail = forza_message_available( REQUEST );
    if( msg_avail == NO_MSG )
      return NO_MSG;

    // Get message
    int ret = forza_message_receive( REQUEST, (uint64_t*) &rcv_pkt, 7 * sizeof( uint64_t ) );
    if( ret == SUCCESS ) {
      // process message
      *sum += rcv_pkt.v1;
      ( *recv_count )++;

      char msg[65] = "\ndebug:PEX: receive_request_messages: received REQUEST msg C: #R\n";
      msg[9]       = '0' + fargs->local_pe;
      msg[59]      = '0' + rcv_pkt.v2;
      msg[63]      = '0' + *recv_count;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    } else if( ret == DONE_MSG ) {
      // check that we received all messages
      if( *recv_count != fargs->n_sends ) {
        char msg[80] = "\ndebug:PEX: receive_request_messages: bad REQUEST DONE message count: #R\n";
        msg[9]       = '0' + fargs->local_pe;
        msg[71]      = '0' + *recv_count;
        rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
        assert( 0 );
      } else {
        char msg[65] = "\ndebug:PEX: receive_request_messages: received REQUEST DONE msg\n";
        msg[9]       = '0' + fargs->local_pe;
        rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
        *done = 1;
      }
    } else {  // Invalid return status
      char msg[70] = "\ndebug:PEX: receive_request_messages: REQUEST message receive error\n";
      msg[9]       = '0' + fargs->local_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
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
  uint64_t      precinct = 0;
  uint64_t      zone     = 0;
  struct Packet send_pkt = { 1 + fargs->local_pe, 0, 0, 0, 0, 0, 0 };
  uint64_t      dest_pe  = fargs->local_pe + 1;
  if( dest_pe == fargs->total_pes )
    dest_pe = 0;

  for( long i = 0; i < fargs->n_sends; i++ ) {
    send_pkt.v2  = i;
    int sendstat = forza_send( REQUEST, (uint64_t*) &send_pkt, precinct, zone, dest_pe );
    if( sendstat != SUCCESS ) {
      char msg[65] = "\ndebug:PEX: send_request_messages: REQUEST msg send C failed\n";
      msg[9]       = '0' + fargs->local_pe;
      msg[52]      = '0' + i;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    } else {
      char msg[65] = "\ndebug:PEX: send_request_messages: REQUEST msg send C succeeded\n";
      msg[9]       = '0' + fargs->local_pe;
      msg[52]      = '0' + i;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    }
  }

  // See if any messages to receive before we try to do done
  int recv_stat = receive_request_messages( fargs, recv_count, sum, done );

  // See if we can send DONE
  // Currently doesn't handle failure to send done
  // It would need to try to receive for a while then try again
  int sendstat  = forza_send_done( REQUEST, precinct, zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[70] = "\ndebug:PEX: send_request_messages: REQUEST msg send done failed\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[70] = "\ndebug:PEX: send_request_messages: REQUEST msg send done succeeded\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Receive any additional messages
  while( recv_stat != DONE_MSG ) {
    recv_stat = receive_request_messages( fargs, recv_count, sum, done );
  }
}

// Try to receive response messages
// Returns if no messages available or when DONE message received
int receive_response_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {
  // Process RESPONSE messages
  struct Packet rcv_pkt = { 0, 0, 0, 0, 0, 0, 0 };

  do {
    // Wait for message
    int msg_avail = forza_message_available( RESPONSE );
    if( msg_avail == NO_MSG )
      return NO_MSG;

    // Get message
    int ret = forza_message_receive( RESPONSE, (uint64_t*) &rcv_pkt, 7 * sizeof( uint64_t ) );
    if( ret == SUCCESS ) {
      // process message
      char msg[65] = "\ndebug:PEX: receive_response_message: received RESPONSE msg\n";
      msg[9]       = '0' + fargs->local_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      *recv_count += rcv_pkt.v1;
      *sum += rcv_pkt.v2;
    } else if( ret == DONE_MSG ) {
      // check that we received all messages
      char msg[65] = "\ndebug:PEX: receive_response_message: received RESPONSE DONE msg\n";
      msg[9]       = '0' + fargs->local_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      *done = 1;
    } else {  // Invalid return status
      char msg[70] = "\ndebug:PEX: receive_response_message: RESPONSE message receive error\n";
      msg[9]       = '0' + fargs->local_pe;
      rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
      assert( 0 );
    }
  } while( !( *done ) );

  {
    char msg[70] = "\ndebug:PEX: receive_response_message: RESPONSE msgcount = X sum = Y\n";
    msg[9]       = '0' + fargs->local_pe;
    msg[58]      = '0' + *recv_count;
    msg[66]      = '0' + *sum;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Check Results
  assert( *recv_count == fargs->n_sends );
  assert( *sum == ( NSENDS * ( fargs->local_pe + 1 ) ) );

  return DONE_MSG;
}

// Similar to processing of request messages
// Sends respone message, receives available messages, tries to send DONE message, receives again
int process_response_messages( struct ForzaThreadArgs_* fargs, int* recv_count, int* sum, int* done ) {
  // Send single ESPONSE message containing the number of received messages and
  // the computed sum
  uint64_t      precinct = 0;
  uint64_t      zone     = 0;
  struct Packet send_pkt = { *recv_count, *sum, 0, 0, 0, 0, 0 };
  uint64_t      dest_pe  = fargs->local_pe + 1;
  if( dest_pe == fargs->total_pes )
    dest_pe = 0;

  int sendstat = forza_send( RESPONSE, (uint64_t*) &send_pkt, precinct, zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[60] = "\ndebug:PEX: actor thread init: RESPONSE msg send failed\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[60] = "\ndebug:PEX: actor thread init: RESPONSE msg send succeeded\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Reset for receive
  *recv_count   = 0;
  *sum          = 0;
  *done         = 0;

  // See if any messages to receive before we try to do done
  int recv_stat = receive_response_messages( fargs, recv_count, sum, done );

  // See if we can send DONE
  sendstat      = forza_send_done( RESPONSE, precinct, zone, dest_pe );
  if( sendstat != SUCCESS ) {
    char msg[65] = "\ndebug:PEX: actor thread init: RESPONSE msg send done failed\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );
  } else {
    char msg[65] = "\ndebug:PEX: actor thread init: RESPONSE msg send done succeeded\n";
    msg[9]       = '0' + fargs->local_pe;
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
void* actor_thread_init( void* fcnargs ) {
  struct ForzaThreadArgs_* fargs      = (struct ForzaThreadArgs_*) fcnargs;
  int                      recv_count = 0;
  int                      sum        = 0;
  int                      done       = 0;

  {
    char msg[50] = "\ndebug:PEX: actor thread init\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Register this PE with ZQM
  int ret = forza_zqm_pe_setup( fargs->local_pe, fargs->n_mailboxes );
  if( ret ) {
    char msg[55] = "\ndebug:PEX: actor thread init: zqm setup failed\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
    assert( 0 );  // Shouldn't fail in simulator
  } else {
    char msg[55] = "\ndebug:PEX: actor thread init: zqm setup succeeded\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Wait for all threads to complete registrations
  forza_zone_barrier( forza_get_zaps_per_zone() );
  {
    char msg[55] = "\ndebug:PEX: actor thread init: after barrier1\n";
    msg[9]       = '0' + fargs->local_pe;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Send and receive request messages
  process_request_messages( fargs, &recv_count, &sum, &done );
  {
    char msg[60] = "\ndebug:PEX: actor thread init: REQUEST msgcount = X sum = Y\n";
    msg[9]       = '0' + fargs->local_pe;
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

  // Currently one pe per zone using SPMD model
  uint64_t n_zaps      = forza_get_zaps_per_zone();
  uint64_t n_local_pes = PE_PER_ZAP * n_zaps;
  uint64_t zap_id      = forza_get_my_zap();

  {
    char msg[50] = "\ndebug:ZAPX: main: Enter main\n";
    msg[10]      = '0' + zap_id;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }

  // Create local actor threads (PEs)
  // Currently function call, can also use forza_thread_create
  struct ForzaThreadArgs_ FArgs[2];  // one for each PE

  // This is kind of weird right now because of the SPMD model that spawns at each ZAP
  // Currently spawning only one thread for each zap
  for( int j = 0; j < n_local_pes / 2; j++ ) {
    int index                = zap_id * PE_PER_ZAP + j;
    FArgs[index].thread_type = 0;
    FArgs[index].total_pes   = n_local_pes;
    FArgs[index].global_pe   = index;
    FArgs[index].zap         = zap_id;
    FArgs[index].local_pe    = index;
    FArgs[index].n_mailboxes = NMBOXES;
    FArgs[index].n_sends     = NSENDS;
#if USE_PTHREAD
    forza_thread_create( &actor_threads[index], (void*) actor_thread_init, (void*) &( FArgs[index] ) );
#else
    actor_thread_init( &( FArgs[index] ) );
#endif
  }

#if USE_PTHREAD
  for( int j = 0; j < n_local_pes / 2; j++ ) {
    int index = zap_id * PE_PER_ZAP + j;
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
