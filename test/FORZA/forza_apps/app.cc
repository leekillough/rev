#include "forza_selector.h"

class TriangleSelector : public hclib::Selector<TrianglePkt> {
public:
  TriangleSelector( int64_t* cnts, sparsemat_t* mat ) : cnt_( cnts ), mat_( mat ) {
    mbx[REQUEST].process = [this]( TrianglePkt pkt, int sender_rank ) { this->req_process( pkt, sender_rank ); };
  }

private:
  //shared variables
  int64_t*     cnt_;
  sparsemat_t* mat_;

  void req_process( TrianglePkt pkg, int sender_rank ) {
    int64_t tempCount = 0;

    for( int64_t k = mat_->loffset[pkg.vj]; k < mat_->loffset[pkg.vj + 1]; k++ ) {
      if( pkg.w == mat_->lnonzero[k] ) {
        tempCount++;
        break;
      }
      if( pkg.w < mat_->lnonzero[k] ) {  // requires that nonzeros are increasing
        break;
      }
    }

    // update the share variable
    *cnt_ += tempCount;
  }
};

int forza_packet_send( int mytid, sparsemat_t* mmat ) {
  int ActorID                   = mytid;

  TriangleSelector* triSelector = new TriangleSelector( &cnt[ActorID], &mat[ActorID] );

  // forza_fprintf(1, "AJAY\n", print_args);

  hclib::finish( [=]() {
    triSelector->start();
    int64_t k, kk, pe;
    int64_t l_i, L_j;

    TrianglePkt  pkg;
    TrianglePkt* tpkt = (TrianglePkt*) forza_malloc( 1 * sizeof( TrianglePkt ) );

    // assert(mmat->lnumrows == 8);

    for( l_i = 0; l_i < mmat->lnumrows; l_i++ ) {
      for( k = mmat->loffset[l_i]; k < mmat->loffset[l_i + 1]; k++ ) {
        // L_i = l_i * THREADS + ActorID;
        L_j    = mmat->lnonzero[k];

        pe     = L_j % GLOBAL_ACTORS;
        pkg.vj = L_j / GLOBAL_ACTORS;
        for( kk = mmat->loffset[l_i]; kk < mmat->loffset[l_i + 1]; kk++ ) {
          pkg.w = mmat->lnonzero[kk];

          if( pkg.w > L_j ) {
            break;
          }

          // TrianglePkt *tpkt = (TrianglePkt *) forza_scratchpad_alloc(1*sizeof(TrianglePkt));
          tpkt->w  = pkg.w;
          tpkt->vj = pkg.vj;

          int retf = triSelector->send( REQUEST, tpkt, pe );
          assert( retf == 1 );
        }
      }
    }
    triSelector->done( REQUEST );
  } );

  // forza_fprintf(1, "AJAY\n", print_args);

  return 0;
}
