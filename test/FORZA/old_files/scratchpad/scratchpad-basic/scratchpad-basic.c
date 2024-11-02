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
  uint64_t* addr;
#define N 10
  addr = (uint64_t*) forza_scratchpad_alloc( N * sizeof( uint32_t ) );

  for( uint32_t i = 0; i < N; i++ ) {
    addr[i] = i;
  }

  for( uint32_t i = 0; i < N; i++ ) {
    assert( addr[i] == i );
  }

  forza_scratchpad_free( addr, N * sizeof( uint32_t ) );

  return 0;
}
