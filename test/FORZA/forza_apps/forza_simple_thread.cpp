// #include "forza_selector.h"
#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

typedef unsigned long int forza_thread_t;
static forza_thread_t     actor_threads[16];

typedef struct ForzaThreadArgs_ {
  int ThreadType;
  int ActorID;
} ForzaThreadArgs;

static int forza_thread_create( forza_thread_t* tid, void* fn, void* arg ) {
  int ret;
  ret = rev_pthread_create( tid, NULL, fn, arg );
  return ret;
}

static int forza_thread_join( forza_thread_t tid ) {
  int ret;
  ret = rev_pthread_join( tid );
  return ret;
}

void* forza_thread_init( void* FArgs ) {
  ForzaThreadArgs* args_recv = (ForzaThreadArgs*) FArgs;
  assert( 13 == args_recv->ActorID );
  return NULL;
}

int main( int argc, char** argv ) {
  ForzaThreadArgs FArgs;
  FArgs.ThreadType = 0;
  FArgs.ActorID    = 13;
  forza_thread_create(
    &actor_threads[0], (void*) forza_thread_init, (void*) &FArgs );
  forza_thread_join( actor_threads[0] );
  return 0;
}
