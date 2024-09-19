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

int main( int argc, char** argv ) {
  uint64_t TID        = forza_get_my_zap();
  //if (TID != 0)
  //  return 0;

  uint64_t abba       = forza_scratchpad_alloc( 32 );

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

  forza_zone_barrier( forza_get_zaps_per_zone() );

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
  }

  abba = forza_scratchpad_alloc( 64 );
  if( TID == 1 )
    while( abba == 0 )
      abba = forza_scratchpad_alloc( 64 );
  forza_debug_print( logical_pe, 0xcafe, abba );

  return 0;
}
