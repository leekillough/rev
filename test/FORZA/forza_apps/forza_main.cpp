#include "bsrlib.h"
#include "forza_lib.h"

void forza_thread_init( void* FArgs_recv, sparsemat_t* mat_chunk ) {
  ForzaThreadArgs* FArgs      = (ForzaThreadArgs*) FArgs_recv;
  int              MY_ACTOR   = FArgs->ActorID;
  int              THREADTYPE = FArgs->ThreadType;
  int              qsize      = PKT_QUEUE_SIZE;
  ForzaPkt         qaddr[PKT_QUEUE_SIZE];
  uint64_t**       tail_ptr;
  uint64_t*        head_ptr;

  tail_ptr  = (uint64_t**) forza_scratchpad_alloc( 1 * sizeof( ForzaPkt* ) );
  *tail_ptr = (uint64_t*) &qaddr;
  head_ptr  = (uint64_t*) &qaddr;
  forza_zen_setup(
    (uint64_t) &qaddr, qsize * sizeof( ForzaPkt ), (uint64_t) tail_ptr );

  // forza_zone_barrier(GLOBAL_ACTORS+1);
  barrier( MY_ACTOR );

  forza_packet_send( MY_ACTOR, mat_chunk );

  return;
}

int main( int argc, char** argv ) {
  // GLOBAL_ACTORS = 2;
  // LOCAL_ACTORS = 1;
  // THREADS_PER_ACTOR = 1;
  int             MY_ACTOR = forza_get_my_zap();
  ForzaThreadArgs FArgs;
  FArgs.ThreadType = 0;
  FArgs.ActorID    = MY_ACTOR;
  // int MY_ACTOR = 0;
  mat = (sparsemat_t*) forza_malloc( GLOBAL_ACTORS * sizeof( sparsemat_t ) );
  cnt = (int64_t*) forza_malloc( GLOBAL_ACTORS * sizeof( int64_t ) );

  if( MY_ACTOR == 0 ) {

    for( int i = 0; i < GLOBAL_ACTORS; i++ ) {
      generate_graph( mat, false, false, i );
    }
  }

  // forza_zone_barrier(GLOBAL_ACTORS+1);
  barrier( MY_ACTOR );


  forza_thread_init( (void*) &FArgs, &mat[MY_ACTOR] );
  // forza_thread_create(&actor_threads[0], (void *) forza_thread_init, (void *) &FArgs);
  // forza_thread_join(actor_threads[0]);

  return 0;
}
