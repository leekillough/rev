
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
  int        TID   = atoi( argv[1] );
  int        qsize = 10;
  uint64_t   qaddr[10];
  uint64_t** tptr =
    (uint64_t**) forza_scratchpad_alloc( 1 * sizeof( uint64_t* ) );

  *tptr          = &qaddr;
  uint64_t* hptr = &qaddr;

  forza_zen_setup( &qaddr, qsize * sizeof( uint64_t ), tptr );

  uint64_t* pkt = (uint64_t*) forza_scratchpad_alloc( 1 * sizeof( uint64_t ) );
  *pkt          = 690;

  if( TID == 0 )
    forza_send( 1, pkt, sizeof( uint64_t ) );

  uint64_t recv_pkt = 0;

  if( TID == 1 ) {
    while( *tptr == hptr )
      ;
    recv_pkt = (uint64_t) *hptr;
    assert( recv_pkt == 690 );
    hptr++;
    forza_zen_credit_release( sizeof( uint64_t ) );
  }

  forza_scratchpad_free( tptr, sizeof( uint64_t ) );
  forza_scratchpad_free( pkt, sizeof( uint64_t ) );

  return 0;
}
