#include "../../../common/syscalls/forza.h"
#include "../../../common/syscalls/syscalls.h"

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

#define I 10

typedef uint64_t      forza_thread_t;
static forza_thread_t actor_threads[16];

typedef struct ForzaThreadArgs_ {
  uint32_t ThreadType;
  uint32_t ActorID;
} ForzaThreadArgs;

static uint32_t forza_thread_create( forza_thread_t* tid, void* fn, void* arg ) {
  uint32_t ret;
  ret = rev_pthread_create( tid, NULL, fn, arg );
  return ret;
}

static uint32_t forza_thread_join( forza_thread_t tid ) {
  uint32_t ret;
  ret = rev_pthread_join( tid );
  return ret;
}

void* forza_thread1( void* FArgs ) {
  ForzaThreadArgs* args_recv = (ForzaThreadArgs*) FArgs;
  uint64_t         tid       = rev_gettid() % 10UL;
  for( uint32_t i = 0; i < I; i++ ) {
    char msg[20] = "\nTIDX: II: thread1\n";
    msg[4]       = '0' + tid;
    msg[8]       = '0' + i;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }
  assert( 13 == args_recv->ActorID );
  return NULL;
}

void* forza_thread2( void* FArgs ) {
  ForzaThreadArgs* args_recv = (ForzaThreadArgs*) FArgs;
  uint64_t         tid       = rev_gettid() % 10UL;
  for( uint32_t i = 0; i < I; i++ ) {
    char msg[20] = "\nTIDX: II: thread2\n";
    msg[4]       = '0' + tid;
    msg[8]       = '0' + i;
    rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  }
  assert( 14 == args_recv->ActorID );
  return NULL;
}

int main( int argc, char** argv ) {

  uint64_t tid      = rev_gettid() % 10UL;
  char     msg1[20] = "\nTIDX: Main start\n";
  msg1[4]           = '0' + tid;
  rev_write( STDOUT_FILENO, msg1, sizeof( msg1 ) );

  ForzaThreadArgs FArgs[2];
#if 1
  FArgs[0].ThreadType = 0;
  FArgs[0].ActorID    = 13;
  forza_thread_create( &actor_threads[0], (void*) forza_thread1, (void*) &( FArgs[0] ) );
#endif
#if 1
  FArgs[1].ThreadType = 0;
  FArgs[1].ActorID    = 14;
  forza_thread_create( &actor_threads[1], (void*) forza_thread2, (void*) &( FArgs[1] ) );
#endif

  char msg2[30] = "\nTIDX: After thread create\n";
  msg2[4]       = '0' + tid;
  rev_write( STDOUT_FILENO, msg2, sizeof( msg2 ) );

#if 1
  forza_thread_join( actor_threads[0] );
#endif
#if 1
  forza_thread_join( actor_threads[1] );
#endif

  char msg3[20] = "\nTIDX: After join\n";
  msg3[4]       = '0' + tid;
  rev_write( STDOUT_FILENO, msg3, sizeof( msg3 ) );
  return 0;
}
