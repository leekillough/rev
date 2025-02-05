#include "../../../common/syscalls/forza.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

// TWO ZONE EXAMPLE CODE; sends from zap in zone 1 to zone 0 (for now)

int main( int argc, char** argv ) {
  uint64_t TID  = forza_get_my_zap() + ( forza_get_my_zone() << 2 );

  //if( TID == 0 )
  //  return 0;

  uint64_t abba = forza_read_zen_status();
  forza_debug_print( 0xa1a1, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xa1deaddecafUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xb2b2, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xa2deadbeefUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xc3c3, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xb1beefcafeUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xd4d4, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xb2b2b3b3UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xe5e5, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xe1e2e3e4e5UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xf6f6, TID, abba );

  // Send a second pzop

  if( TID != 0 )
    forza_pzop_word( 0xaaaa1111UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0x1234, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xbbbb2222UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0x2345, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xcccc3333UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0x3456, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xdddd4444UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0x4567, TID, abba );

  if( TID != 0 )
    forza_pzop_word( 0xeeee5555UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0x5678, TID, abba );

  // delay block to keep the program from exiting on zone = zap = 0 too soon
  if( TID == 0 ) {
    for( uint64_t i = 0; i < 20; i++ )
      forza_debug_print( 0x12345678 + i, TID, abba );
  }

  return 0;
}
