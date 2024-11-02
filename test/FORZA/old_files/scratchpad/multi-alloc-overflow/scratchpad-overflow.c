#include "../../../../common/syscalls/forza.h"
#include "../../../../common/syscalls/syscalls.h"
#define assert( x )      \
  if( !( x ) ) {         \
    asm( ".byte 0x00" ); \
    asm( ".byte 0x00" ); \
    asm( ".byte 0x00" ); \
    asm( ".byte 0x00" ); \
  }

int main( int argc, char** argv ) {
#define SCRATCHPAD_CHUNKS 1024
#define CHUNK_SIZE        512
  // Make an array of alloced addresses
  uint64_t* Chunks[SCRATCHPAD_CHUNKS];

  // Fill up the scratchpad
  for( uint32_t i = 0; i <= SCRATCHPAD_CHUNKS; i++ ) {
    Chunks[i] = (uint64_t*) forza_scratchpad_alloc( CHUNK_SIZE );
  }

  assert( Chunks[SCRATCHPAD_CHUNKS] == NULL );

  // Free one of the segments and allocate again
  forza_scratchpad_free( Chunks[4], CHUNK_SIZE );

  // Now that we have room again, try to allocate again
  uint64_t* NewAddr = (uint64_t*) forza_scratchpad_alloc( CHUNK_SIZE );

  // Assert the allocation succeeded
  assert( NewAddr != NULL );

  return 0;
}
