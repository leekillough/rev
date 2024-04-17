#include "forza_lib.h"
#include <functional>

void* operator new( std::size_t t ) {
  void* p = forza_malloc( t );
  return p;
}

void operator delete( void* p ) {
  forza_free( p, 2 );
}

namespace hclib {

template< typename T >
inline void launch( const char** deps, int ndeps, T&& lambda ) {
  // forza_fprintf(1, "Actor Launch\n", print_args);
  lambda();
}

template< typename T >
inline void finish( T&& lambda ) {
  // forza_fprintf(1, "Actor Finish\n", print_args);
  lambda();
}

template< typename T >
class Mailbox {
public:
  std::function< void( T, int ) > process;
};

template< typename T >
class Selector {
public:
  Mailbox< T > mbx[10];

  void         start() {
  }

  int send( int mb_id, T* pkt, int rank ) {

    ForzaPkt* fpkt =
      (ForzaPkt*) forza_scratchpad_alloc( 1 * sizeof( ForzaPkt ) );
    ;
    fpkt->w       = pkt->w;
    fpkt->vj      = pkt->vj;
    fpkt->type    = mb_id;
    uint64_t dest = rank * THREADS_PER_ACTOR;
    int      ret  = forza_send( dest, (uint64_t) fpkt, sizeof( ForzaPkt ) );
    return ret;
  }

  void done( int mb_id ) {
    ForzaPkt* fpkt =
      (ForzaPkt*) forza_scratchpad_alloc( 1 * sizeof( ForzaPkt ) );
    fpkt->w    = 0;
    fpkt->vj   = 0;
    fpkt->type = FORZA_DONE;
    for( int i = 0; i < GLOBAL_ACTORS; i++ ) {
      uint64_t dest = i * THREADS_PER_ACTOR;
      forza_send( i, (uint64_t) fpkt, sizeof( ForzaPkt ) );
    }
    return;
  }
};
}  // namespace hclib
