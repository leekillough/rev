
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
  int       TID   = forza_get_my_zap();
  int       qsize = 10;
  uint64_t  qaddr[10];    // Storage buffer for incoming messages
  uint64_t* my_tail_ptr;  // Scratchpad address that zen updates
  my_tail_ptr            = (uint64_t*) forza_scratchpad_alloc( 1 * sizeof( uint64_t* ) );
  *my_tail_ptr           = 0xdeadbeef;  // initialize contents of tail ptr to dummy data

  // This ptr is used to compare to the one read from the scratchpad (aka my_tail_ptr)
  uint64_t* cur_recv_ptr = &qaddr[0];

  // Buffer needs to be memory for now
  forza_zen_setup( (uint64_t) cur_recv_ptr, qsize * sizeof( uint64_t ), (uint64_t) my_tail_ptr );

  forza_zone_barrier( 2 );  // two executing harts

  // Packet to send from thread on zap 0 to zap 1
  uint64_t* pkt = (uint64_t*) forza_scratchpad_alloc( 1 * sizeof( uint64_t ) );
  *pkt          = 690;

  if( TID == 0 )
    forza_send( 1, (uint64_t) pkt, sizeof( uint64_t ) );

  // Variable to hold receive data
  uint64_t recv_pkt = 0;

  if( TID == 1 ) {
    // Spin until the zen has put a message into the storage buffer and updated the
    // pointer in the scratchpad
    while( (uint64_t) cur_recv_ptr == *my_tail_ptr )
      ;
    // Get the receive data and check it
    recv_pkt = *cur_recv_ptr;
    assert( recv_pkt == 690 );
    cur_recv_ptr++;  // Update my pointer for comparison (this would be needed for the next packet)
    forza_zen_credit_release( sizeof( uint64_t ) );
  }

  forza_scratchpad_free( (uint64_t) my_tail_ptr, sizeof( uint64_t* ) );
  forza_scratchpad_free( (uint64_t) pkt, sizeof( uint64_t ) );

  return 0;
}
