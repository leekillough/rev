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

// SINGLE ZONE EXAMPLE CODE

int main( int argc, char** argv ) {
  uint64_t TID = forza_get_my_zap() + ( forza_get_my_zone() << 2 );

  if( TID != 0 )
    return 0;

  //if( forza_get_my_zone() != 0 )
  //  return 0;

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

  if( TID == 0 )
    forza_spawn_word( 0xb1beefcafeUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xb2b2, TID, abba );

  if( TID == 0 )
    forza_spawn_word( 0xb2b2b3b3UL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xc3c3, TID, abba );

  return 0;
}
