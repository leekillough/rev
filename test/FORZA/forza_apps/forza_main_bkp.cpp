#include "forza_selector.h"

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

// int my_atoi(char *x) {
//     int acc = 0;
//     while (*x >= '0' && *x <= '9') {
//         acc *= 10;
//         acc += (*x - '0');
//         x++;
//     }
//     return acc;
// }

void* forza_thread_init( void* test_args )
// void *forza_thread_init(ForzaThreadArgs *FArgs)
{
  // int MY_ACTOR = *test_args;
  ForzaThreadArgs* FArgs      = (ForzaThreadArgs*) test_args;
  int              MY_ACTOR   = FArgs->ActorID;
  int              THREADTYPE = FArgs->ThreadType;
  int              qsize      = 100;
  ForzaPkt         qaddr[100];
  uint64_t**       tail_ptr;
  uint64_t*        head_ptr;

  if( THREADTYPE == RECEIVER ) {
    tail_ptr  = (uint64_t**) forza_scratchpad_alloc( 1 * sizeof( ForzaPkt* ) );
    *tail_ptr = (uint64_t*) &qaddr;
    head_ptr  = (uint64_t*) &qaddr;
    forza_zen_setup( (uint64_t) &qaddr, qsize * sizeof( ForzaPkt ), (uint64_t) tail_ptr );
  }

  // forza_zone_barrier(LOCAL_ACTORS*THREADS_PER_ACTOR);

  // if(THREADTYPE == RECEIVER)
  // {
  //   // forza_packet_poll(MY_ACTOR, tail_ptr, head_ptr);
  // }
  // else if(THREADTYPE == SENDER)
  // {
  //   // forza_packet_send(MY_ACTOR);
  // }
  return NULL;
}

int main( int argc, char** argv ) {
  // assert(argc == 3);

  // LOCAL_ACTORS = atoi(argv[1]);
  // // assert(LOCAL_ACTORS == 0);

  // GLOBAL_ACTORS = atoi(argv[2]);
  // ACTOR_START_INDEX = atoi(argv[3]);
  // THREADS_PER_ACTOR = atoi(argv[4]);

  LOCAL_ACTORS      = 1;
  // GLOBAL_ACTORS = 2;
  ACTOR_START_INDEX = 0;
  THREADS_PER_ACTOR = 2;
  ForzaThreadArgs FArgs;

  // if(THREADS_PER_ACTOR*LOCAL_ACTORS >= forza_get_harts_per_zap())
  // {
  //   //TODO: print error and return;
  // }

  for( uint64_t i = 0; i < LOCAL_ACTORS; i++ ) {
    ACTOR_ID[i]      = i + ACTOR_START_INDEX;
    FArgs.ThreadType = SENDER;
    FArgs.ActorID    = ACTOR_ID[i];
    forza_thread_create( &actor_threads[( i * LOCAL_ACTORS )], (void*) forza_thread_init, (void*) &FArgs );
    for( uint64_t j = 1; j < THREADS_PER_ACTOR; j++ ) {
      FArgs.ThreadType = RECEIVER;
      FArgs.ActorID    = ACTOR_ID[i];
      forza_thread_create( &actor_threads[( i * LOCAL_ACTORS ) + j], (void*) forza_thread_init, (void*) &FArgs );
    }
  }

  for( uint64_t i = 0; i < LOCAL_ACTORS * THREADS_PER_ACTOR; i++ ) {
    forza_thread_join( actor_threads[i] );
  }
  return 0;
}
