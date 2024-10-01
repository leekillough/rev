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

void* forza_thread1( void* FArgs ) {
  ForzaThreadArgs* args_recv = (ForzaThreadArgs*) FArgs;
	const char msg[20] = "\nThread1 init\n";
  rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  assert( 13 == args_recv->ActorID );
  return NULL;
}

void* forza_thread2( void* FArgs ) {
  ForzaThreadArgs* args_recv = (ForzaThreadArgs*) FArgs;
  const char msg[20] = "\nThread2 init\n";
  rev_write( STDOUT_FILENO, msg, sizeof( msg ) );
  assert( 14 == args_recv->ActorID );
  return NULL;
}


int main( int argc, char** argv ) {
  const char msg1[15] = "\nMain start\n";
  rev_write( STDOUT_FILENO, msg1, sizeof( msg1 ) );

  ForzaThreadArgs FArgs[2];
#if 1
	FArgs[0].ThreadType = 0;
  FArgs[0].ActorID    = 13;
	forza_thread_create(
    &actor_threads[0], (void*) forza_thread1, (void*) &(FArgs[0]) );
#endif 
#if 1
	FArgs[1].ThreadType = 0;
	FArgs[1].ActorID = 14;
	forza_thread_create(
    &actor_threads[1], (void*) forza_thread2, (void*) &(FArgs[1]) );
#endif

	const char msg2[25] = "\nAfter thread create\n";
  rev_write( STDOUT_FILENO, msg2, sizeof( msg2 ) );

#if 1
  forza_thread_join( actor_threads[0] );
#endif
#if 1
	forza_thread_join( actor_threads[1] );
#endif

	const char msg3[20] = "\nAfter join\n";
  rev_write( STDOUT_FILENO, msg3, sizeof( msg3 ) );
  return 0;
}
