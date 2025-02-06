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

// TWO ZONE EXAMPLE CODE; sends pzop from zap in zone 1 to zone 0 (for now)
// This routing is explicitly defined in the ZEN code because we're not sending
// real addresses for routing a pzop

int main( int argc, char** argv ) {
  uint64_t TID  = forza_get_my_zap() + ( forza_get_my_zone() << 2 );

  uint64_t abba = forza_read_zen_status();
  forza_debug_print( 0xa1a1, TID, abba );

  if( TID == 0 )
    forza_spawn_word( 0xa1deaddecafUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xb2b2, TID, abba );

  if( TID == 0 )
    forza_spawn_word( 0xa2deadbeefUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xc3c3, TID, abba );

  uint64_t flag = abba & ( 0x1UL << 32 );
  while( flag != 0 ) {
    abba = forza_read_zen_status();
    flag = abba & ( 0x1UL << 32 );
    forza_debug_print( 0xabcdef, abba, flag );
  }

  // Send a pzop
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

  return 0;
}
