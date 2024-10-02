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
  uint32_t* addr32;
  uint16_t* addr16;
  uint8_t*  addr8;
#define N 8
  addr   = (uint64_t*) forza_scratchpad_alloc( N * sizeof( uint64_t ) );
  addr32 = (uint32_t*) forza_scratchpad_alloc( N * sizeof( uint32_t ) );
  addr16 = (uint16_t*) forza_scratchpad_alloc( N * sizeof( uint16_t ) );
  addr8  = (uint8_t*) forza_scratchpad_alloc( N * sizeof( uint8_t ) );

  for( size_t i = 0; i < N; i++ ) {
    addr[i]   = i;
    addr32[i] = i;
    addr16[i] = i;
    addr8[i]  = i;
  }

  for( size_t i = 0; i < N; i++ ) {
    // if assert fails, will appear as an instruction decode error (Inst=0)
    assert( addr[i] == i );
    assert( addr32[i] == i );
    assert( addr16[i] == i );
    assert( addr8[i] == i );
  }

  forza_scratchpad_free( addr, N * sizeof( uint64_t ) );
  forza_scratchpad_free( addr32, N * sizeof( uint32_t ) );
  forza_scratchpad_free( addr16, N * sizeof( uint16_t ) );
  forza_scratchpad_free( addr8, N * sizeof( uint8_t ) );

  return 0;
}
