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
  uint64_t TID = forza_get_my_zap();

  if( TID != 0 )
    return 0;

  if( forza_get_my_zone() != 0 )
    return 0;

  uint64_t abba = forza_read_zen_status();
  forza_debug_print( 0xa1a1, TID, abba );

  forza_spawn_word( 0xa1deaddecafUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xb2b2, TID, abba );

  forza_spawn_word( 0xa2deadbeefUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xc3c3, TID, abba );

  forza_spawn_word( 0xa3deadcafeUL );
  abba = forza_read_zen_status();
  forza_debug_print( 0xd4d4, TID, abba );

  return 0;
}
