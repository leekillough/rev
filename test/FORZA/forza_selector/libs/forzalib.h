
#include "forza_utils.h"
#include <stdio.h>
#include <stdlib.h>

int forza_thread_create( forza_thread_t* tid, void* fn, void* arg ) {
  int ret;
  ret = rev_pthread_create( tid, NULL, fn, arg );
  return ret;
}

int forza_thread_join( forza_thread_t tid ) {
  int ret;
  ret = rev_pthread_join( tid );
  return ret;
}

uint64_t forza_file_read( char* filename, char* buf, size_t count ) {
  int      fd        = rev_openat( AT_FDCWD, filename, 0, O_RDONLY );
  uint64_t bytesRead = rev_read( fd, buf, count );
  rev_close( fd );
  return bytesRead;
}

typedef struct sparsemat_t_ {
  int64_t
    local;  //!< 0/1 flag specifies whether this is a local or distributed matrix
  int64_t numrows;   //!< the total number of rows in the matrix
  int64_t lnumrows;  //!< the number of rows on this PE
                     // note lnumrows = (numrows / NPES) + {0 or 1}
                     //    depending on numrows % NPES
  int64_t numcols;   //!< the nonzeros have values between 0 and numcols
  int64_t nnz;       //!< total number of nonzeros in the matrix
  int64_t lnnz;      //!< the number of nonzeros on this PE
  //int64_t * offset;      //!< the row offsets into the array of nonzeros
  int64_t loffset
    [PKT_QUEUE_SIZE];  //!< the row offsets for the row with affinity to this PE
  //int64_t * nonzero;     //!< the global array of column indices of nonzeros
  int64_t lnonzero
    [PKT_QUEUE_SIZE];  //!< local array of column indices (for rows on this PE).
  //double * value;        //!< the global array of values of nonzeros. Optional.
  double lvalue
    [PKT_QUEUE_SIZE];  //!< local array of values (values for rows on this PE)
} sparsemat_t;

typedef struct TrianglePkt {
  int64_t w;
  int64_t vj;
} TrianglePkt;

typedef struct JaccardPkt {
  int64_t sender_pe;
  int64_t x;
  int64_t pos_row;
  int64_t pos_col;
  int64_t index_u;
} JaccardPkt;

typedef struct DegreePkt {
  int64_t sender_pe;
  int64_t i;
  int64_t src_idx;
} DegreePkt;

typedef struct visitmsg {
  int64_t vloc;
  int64_t vfrom;
} visitmsg;

typedef struct ForzaPkt_ {
  volatile int valid;
  int          mb_type;
  void*        pkt;
} ForzaPkt;

typedef struct mailbox_t {
  ForzaPkt          pkt[PKT_QUEUE_SIZE];
  volatile uint64_t mbdone;
  uint64_t          send_count;
  uint64_t          recv_count;
  uint64_t          expected_resp;
} mailbox;

enum MailBoxType { REQUEST, RESPONSE, FORZA_DONE, FORZA_BARRIER };

mailbox**    mb_request;
sparsemat_t* mat;
sparsemat_t* kmer_mat;
int64_t*     cnt;

int64_t**    frontier;
int64_t**    nextFrontier;

int64_t*     nvisited;
int64_t*     visited_size;
int64_t*     frontierTail;
int64_t*     nextFrontierTail;
bool*        visited[2];

void         generate_graph( sparsemat_t* mmfile,
                             bool         iskmer,
                             bool         isbfs,
                             int          mynode ) {
  char buffer1[BUF_SIZE];
  char buffer2[BUF_SIZE][10];
  char path[40];

  if( iskmer ) {
    switch( mynode ) {
    case 0: strcpy( path, "kmer0.dat" ); break;
    case 1: strcpy( path, "kmer1.dat" ); break;
    case 2: strcpy( path, "kmer2.dat" ); break;
    case 3: strcpy( path, "kmer3.dat" ); break;
    default: break;
    }
  } else {

    switch( mynode ) {
    case 0: strcpy( path, "node0.dat" ); break;
    case 1: strcpy( path, "node1.dat" ); break;
    case 2: strcpy( path, "node2.dat" ); break;
    case 3: strcpy( path, "node3.dat" ); break;
    default: break;
    }
  }


  uint64_t bytesRead = forza_file_read( path, buffer1, 50000 );
  buffer1[bytesRead] = '\0';

  uint64_t j         = 0;
  uint64_t k         = 0;

  for( uint64_t i = 0; i < bytesRead; i++ ) {
    if( buffer1[i] == '\n' ) {
      buffer2[k][j] = '\0';
      k++;
      j = 0;
      continue;
    }

    buffer2[k][j] = buffer1[i];
    j++;
  }

  mmfile[mynode].local =
    atoi( buffer2[0] );  //my_atoi should be replaced with sscanf
  mmfile[mynode].numrows  = atoi( buffer2[1] );
  mmfile[mynode].lnumrows = atoi( buffer2[2] );
  mmfile[mynode].numcols  = atoi( buffer2[3] );
  mmfile[mynode].nnz      = atoi( buffer2[4] );
  mmfile[mynode].lnnz     = atoi( buffer2[5] );

  for( int lrows = 0; lrows < mmfile[mynode].lnumrows + 1; lrows++ ) {
    mmfile[mynode].loffset[lrows] = atoi( buffer2[6 + lrows] );
  }

  int zero_offset = 6 + mmfile[mynode].lnumrows + 1;

  for( int lzeros = 0; lzeros < mmfile[mynode].lnnz; lzeros++ ) {
    mmfile[mynode].lnonzero[lzeros] = atoi( buffer2[zero_offset + lzeros] );
  }

  for( int lval = 0; lval < mat[mynode].lnnz; lval++ ) {
    mmfile[mynode].lvalue[lval] = 0;
  }

  if( isbfs ) {
    visited_size[mynode] = mat[mynode].lnumrows;
    visited[mynode] =
      (bool*) forza_malloc( visited_size[mynode] * sizeof( bool ) );
    for( int tt = 0; tt < visited_size[mynode]; tt++ ) {
      visited[mynode][tt] = 0;
    }
  }

  return;
}

void forza_send( mailbox** mb, ForzaPkt pkt, int dest, int src ) {
  uint64_t sendcnt                   = mb[dest][src].send_count;
  mb[dest][src].pkt[sendcnt].pkt     = pkt.pkt;
  mb[dest][src].pkt[sendcnt].mb_type = pkt.mb_type;
  mb[dest][src].pkt[sendcnt].valid   = 1;
  mb[dest][src].send_count++;
}

void forza_barrier( uint64_t TID ) {
  ForzaPkt pkt;
  pkt.mb_type = FORZA_BARRIER;
  for( int i = 0; i < THREADS; i++ ) {
    forza_send( mb_request, pkt, i, TID );
  }

  fbarriers[TID]++;

  while( 1 ) {
    uint64_t a  = fbarriers[TID];
    int      ok = 1;
    if( a < TOTAL_THREADS ) {
      ok = 0;
    }

    if( ok ) {
      fbarriers[TID] = 0;
      break;
    }
  }
}

void forza_recv_process( void* pkt, int AID ) {
  TrianglePkt* pkg       = (TrianglePkt*) pkt;
  int64_t      tempCount = 0;

  for( int64_t k = mat[AID].loffset[pkg->vj]; k < mat[AID].loffset[pkg->vj + 1];
       k++ ) {
    if( pkg->w == mat[AID].lnonzero[k] ) {
      tempCount++;
      break;
    }
    if( pkg->w <
        mat[AID].lnonzero[k] ) {  // requires that nonzeros are increasing
      break;
    }
  }
  // update the share variable
  cnt[AID] += tempCount;
  forza_free( pkt, sizeof( TrianglePkt ) );
}

void* forza_poll_thread( int* mytid ) {
  int mythread = *mytid;

  if( mythread == 0 ) {
    print_args[0] = (void*) &mythread;
    forza_fprintf( 1, "Actor[%d]: Start recv handler\n", print_args );
  }

  volatile int done_flag = 1;

  while( 1 ) {
    done_flag = 1;
    for( int i = 0; i < THREADS; i++ ) {
      if( mb_request[mythread][i].mbdone == 0 ) {
        done_flag = 0;
        break;
      }
    }

    for( int i = 0; i < THREADS; i++ ) {
      uint64_t recvcnt = mb_request[mythread][i].recv_count;

      while( mb_request[mythread][i].pkt[recvcnt].valid ) {
        switch( mb_request[mythread][i].pkt[recvcnt].mb_type ) {
        case REQUEST:
          forza_recv_process( mb_request[mythread][i].pkt[recvcnt].pkt,
                              mythread );
          break;
        case RESPONSE: break;
        case FORZA_DONE: mb_request[mythread][i].mbdone = 1; break;
        case FORZA_BARRIER: fbarriers[mythread]++; break;
        }
        mb_request[mythread][i].pkt[recvcnt].valid = 0;
        mb_request[mythread][i].recv_count++;
        recvcnt++;
      }
    }

    if( done_flag ) {
      if( mythread == 0 ) {
        print_args[0] = (void*) &mythread;
        forza_fprintf( 1, "Actor[%d]: Done recv handler\n", print_args );
      }
      break;
    }
  }

  return NULL;
}

void jaccard_phase1_recv( void* pkt, int AID ) {
  DegreePkt* pkg2 = (DegreePkt*) pkt;
  DegreePkt* dpkt = (DegreePkt*) forza_malloc( 1 * sizeof( DegreePkt ) );
  dpkt->src_idx   = pkg2->src_idx;
  dpkt->i = kmer_mat[AID].loffset[pkg2->i + 1] - kmer_mat[AID].loffset[pkg2->i];
  ForzaPkt fpkt;
  fpkt.pkt     = (void*) dpkt;
  fpkt.mb_type = RESPONSE;
  mb_request[pkg2->sender_pe][AID].expected_resp++;
  forza_send( mb_request, fpkt, pkg2->sender_pe, AID );
  forza_free( pkt, sizeof( DegreePkt ) );
}

void jaccard_phase1_resp( void* pkt, int AID ) {
  DegreePkt* pkg                  = (DegreePkt*) pkt;
  mat[AID].lnonzero[pkg->src_idx] = pkg->i;
}

void* jaccard_phase1_poll( int* mytid ) {
  int mythread = *mytid;
  if( mythread == 0 ) {
    print_args[0] = (void*) &mythread;
    forza_fprintf( 1, "Actor[%d]: Start phase1 recv handler\n", print_args );
  }

  volatile int done_flag = 1;

  while( 1 ) {
    done_flag = 1;
    for( int i = 0; i < THREADS; i++ ) {
      if( mb_request[mythread][i].mbdone != 2 ) {
        done_flag = 0;
        break;
      }
    }

    for( int i = 0; i < THREADS; i++ ) {
      uint64_t recvcnt = mb_request[mythread][i].recv_count;

      while( mb_request[mythread][i].pkt[recvcnt].valid ) {
        switch( mb_request[mythread][i].pkt[recvcnt].mb_type ) {
        case REQUEST:
          jaccard_phase1_recv( mb_request[mythread][i].pkt[recvcnt].pkt,
                               mythread );
          break;
        case RESPONSE:
          jaccard_phase1_resp( mb_request[mythread][i].pkt[recvcnt].pkt,
                               mythread );
          mb_request[mythread][i].expected_resp--;
          if( mb_request[mythread][i].expected_resp == 0 ) {
            ForzaPkt fpkt;
            fpkt.pkt     = NULL;
            fpkt.mb_type = FORZA_DONE;
            if( mythread == 0 ) {
              print_args[0] = (void*) &mythread;
              print_args[1] = (void*) &i;
              forza_fprintf( 1, "[%d][%d] sending done\n", print_args );
            }
            forza_send( mb_request, fpkt, i, mythread );
          }
          break;
        case FORZA_DONE: mb_request[mythread][i].mbdone++; break;
        case FORZA_BARRIER: fbarriers[mythread]++; break;
        }
        mb_request[mythread][i].pkt[recvcnt].valid = 0;
        mb_request[mythread][i].recv_count++;
        recvcnt++;
      }
    }

    if( done_flag ) {
      if( mythread == 0 ) {
        print_args[0] = (void*) &mythread;
        forza_fprintf( 1, "Actor[%d]: Done recv handler\n", print_args );
      }
      break;
    }
  }

  return NULL;
}

int64_t binary_search( int64_t      l_,
                       int64_t      r_,
                       int64_t      val_,
                       sparsemat_t* mat_,
                       int64_t      flag_ ) {  // custom binary search function
  while( l_ <= r_ ) {
    int     m_ = l_ + ( r_ - l_ ) / 2;
    int64_t comp_var_;
    if( flag_ ) {
      comp_var_ = m_ - l_;
    } else {
      comp_var_ = mat_->lnonzero[m_];
    }
    if( comp_var_ == val_ ) {
      return m_;
    }
    if( comp_var_ < val_ ) {
      l_ = m_ + 1;
    } else {
      r_ = m_ - 1;
    }
  }
  return -1;
}

void jaccard_phase2_recv( void* pkt, int AID ) {
  JaccardPkt*  pkg2              = (JaccardPkt*) pkt;
  sparsemat_t* mat_              = &kmer_mat[AID];
  sparsemat_t* intersection_mat_ = &mat[AID];
  int64_t      search_           = binary_search( mat_->loffset[pkg2->index_u],
                                   mat_->loffset[pkg2->index_u + 1] - 1,
                                   pkg2->x,
                                   mat_,
                                   0 );
  if( search_ != -1 ) {
    pkg2->pos_row = pkg2->pos_row / THREADS;
    int64_t search__ =
      binary_search( intersection_mat_->loffset[pkg2->pos_row],
                     intersection_mat_->loffset[pkg2->pos_row + 1] - 1,
                     pkg2->pos_col,
                     mat_,
                     1 );
    if( search__ != -1 ) {
      intersection_mat_->lvalue[search__]++;
    }
  }
  forza_free( pkt, sizeof( JaccardPkt ) );
}

void* jaccard_phase2_poll( int* mytid ) {
  int mythread = *mytid;
  if( mythread == 0 ) {
    print_args[0] = (void*) &mythread;
    forza_fprintf( 1, "Actor[%d]: Start phase2 recv handler\n", print_args );
  }

  volatile int done_flag = 1;

  while( 1 ) {
    done_flag = 1;
    for( int i = 0; i < THREADS; i++ ) {
      if( mb_request[mythread][i].mbdone == 0 ) {
        done_flag = 0;
        break;
      }
    }

    for( int i = 0; i < THREADS; i++ ) {
      uint64_t recvcnt = mb_request[mythread][i].recv_count;

      while( mb_request[mythread][i].pkt[recvcnt].valid ) {
        switch( mb_request[mythread][i].pkt[recvcnt].mb_type ) {
        case REQUEST:
          jaccard_phase2_recv( mb_request[mythread][i].pkt[recvcnt].pkt,
                               mythread );
          break;
        case RESPONSE: break;
        case FORZA_DONE: mb_request[mythread][i].mbdone++; break;
        case FORZA_BARRIER: fbarriers[mythread]++; break;
        }
        mb_request[mythread][i].pkt[recvcnt].valid = 0;
        mb_request[mythread][i].recv_count++;
        recvcnt++;
      }
    }

    if( done_flag ) {
      if( mythread == 0 ) {
        print_args[0] = (void*) &mythread;
        forza_fprintf( 1, "Actor[%d]: Done recv handler\n", print_args );
      }
      break;
    }
  }

  return NULL;
}

void bfs_recv_process( void* pkt, int AID ) {
  visitmsg* m = (visitmsg*) pkt;
  if( !visited[AID][m->vloc] ) {
    visited[AID][m->vloc]                    = true;
    nextFrontier[AID][nextFrontierTail[AID]] = m->vloc;
    nextFrontierTail[AID]++;
  }

  forza_free( pkt, sizeof( TrianglePkt ) );
}

void* bfs_poll_thread( int* mytid ) {
  int mythread = *mytid;

  if( mythread == 0 ) {
    print_args[0] = (void*) &mythread;
    forza_fprintf( 1, "Actor[%d]: Start recv handler\n", print_args );
  }

  volatile int done_flag = 1;

  while( 1 ) {
    done_flag = 1;
    for( int i = 0; i < THREADS; i++ ) {
      if( mb_request[mythread][i].mbdone == 0 ) {
        done_flag = 0;
        break;
      }
    }

    for( int i = 0; i < THREADS; i++ ) {
      uint64_t recvcnt = mb_request[mythread][i].recv_count;

      while( mb_request[mythread][i].pkt[recvcnt].valid ) {
        switch( mb_request[mythread][i].pkt[recvcnt].mb_type ) {
        case REQUEST:
          bfs_recv_process( mb_request[mythread][i].pkt[recvcnt].pkt,
                            mythread );
          break;
        case RESPONSE: break;
        case FORZA_DONE: mb_request[mythread][i].mbdone = 1; break;
        case FORZA_BARRIER: fbarriers[mythread]++; break;
        }
        mb_request[mythread][i].pkt[recvcnt].valid = 0;
        mb_request[mythread][i].recv_count++;
        recvcnt++;
      }
    }

    if( done_flag ) {
      if( mythread == 0 ) {
        print_args[0] = (void*) &mythread;
        forza_fprintf( 1, "Actor[%d]: Done recv handler\n", print_args );
      }
      break;
    }
  }

  return NULL;
}
