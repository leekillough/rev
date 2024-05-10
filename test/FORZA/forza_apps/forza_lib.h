#ifndef _forza_lib_h_
#define _forza_lib_h_

// #include "../../../common/syscalls/forza.h"
// #include "../../../common/syscalls/syscalls.h"
#include "forza_utils.h"
// extern "C" {
// #include <stdlib.h>
// #include <stdio.h>
// }
#define NUM_ACTORS_PER_ZAP   512
#define QUEUE_SIZE_PER_ACTOR 100

static uint64_t LOCAL_ACTORS      = 1;
static uint64_t GLOBAL_ACTORS     = 1;
static uint64_t ACTOR_START_INDEX = 0;
static uint64_t THREADS_PER_ACTOR = 1;

typedef unsigned long int forza_thread_t;
static forza_thread_t     actor_threads[NUM_ACTORS_PER_ZAP];
static uint64_t           ACTOR_ID[NUM_ACTORS_PER_ZAP];
static int64_t*           cnt;

// static uint64_t **barrier_count;

enum MailBoxType { REQUEST, RESPONSE, FORZA_DONE, FORZA_BARRIER };

enum ForzaThreadType { SENDER, RECEIVER };

#define assert( x )               \
  do                              \
    if( !( x ) ) {                \
      asm( ".dword 0x00000000" ); \
    }                             \
  while( 0 )

// typedef struct ForzaPkt_ {
//     int type;
//     void *pkt;
// } ForzaPkt;

typedef struct ForzaPkt_ {
  int type;
  // void *pkt;
  int64_t w;
  int64_t vj;
} ForzaPkt;

typedef struct TrianglePkt {
  int64_t w;
  int64_t vj;
} TrianglePkt;

typedef struct ForzaThreadArgs_ {
  int ThreadType;
  int ActorID;
} ForzaThreadArgs;

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

static sparsemat_t* mat;
extern int          forza_packet_send( int mytid, sparsemat_t* mmat );

// extern void forza_packet_send(int mytid);

static int my_atoi( char* x ) {
  int acc = 0;
  while( *x >= '0' && *x <= '9' ) {
    acc *= 10;
    acc += ( *x - '0' );
    x++;
  }
  return acc;
}

static uint64_t forza_file_read( char* filename, char* buf, size_t count ) {
  uint64_t i         = 0;

  int fd             = rev_openat( AT_FDCWD, filename, 0, O_RDONLY );

  uint64_t bytesRead = 0;
  char     temp_buf[2000];
  while( i < BUF_SIZE ) {
    bytesRead = rev_read( fd, temp_buf, 2000 );
    if( bytesRead == 0 )
      break;

    for( uint64_t j = 0; j < bytesRead; j++ ) {
      buf[i + j] = temp_buf[j];
    }
    i += bytesRead;
  }
  rev_close( fd );
  return i;
}

static void
  generate_graph( sparsemat_t* mmfile, bool iskmer, bool isbfs, int mynode ) {
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
    my_atoi( buffer2[0] );  //my_atoi should be replaced with sscanf
  mmfile[mynode].numrows  = my_atoi( buffer2[1] );
  mmfile[mynode].lnumrows = my_atoi( buffer2[2] );
  mmfile[mynode].numcols  = my_atoi( buffer2[3] );
  mmfile[mynode].nnz      = my_atoi( buffer2[4] );
  mmfile[mynode].lnnz     = my_atoi( buffer2[5] );

  for( int lrows = 0; lrows < mmfile[mynode].lnumrows + 1; lrows++ ) {
    mmfile[mynode].loffset[lrows] = my_atoi( buffer2[6 + lrows] );
  }

  int zero_offset = 6 + mmfile[mynode].lnumrows + 1;

  for( int lzeros = 0; lzeros < mmfile[mynode].lnnz; lzeros++ ) {
    mmfile[mynode].lnonzero[lzeros] = my_atoi( buffer2[zero_offset + lzeros] );
  }

  for( int lval = 0; lval < mat[mynode].lnnz; lval++ ) {
    mmfile[mynode].lvalue[lval] = 0;
  }

  // if(isbfs)
  // {
  //     visited_size[mynode] = mat[mynode].lnumrows;
  //     visited[mynode] = (bool*)forza_malloc(visited_size[mynode] * sizeof(bool));
  //     for(int tt = 0; tt < visited_size[mynode]; tt++)
  //     {
  //         visited[mynode][tt] = 0;
  //     }
  // }

  return;
}

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

// static void *forza_malloc(size_t size)
// {
//     return (void*) rev_mmap(0,
//           size,
//           PROT_READ | PROT_WRITE | PROT_EXEC,
//           MAP_PRIVATE | MAP_ANONYMOUS,
//           -1,
//           0);
// }

// static void *forza_free(void *ptr, size_t size)
// {
//     std::size_t addr = reinterpret_cast<std::size_t>(ptr);
//     return (void *) rev_munmap(addr, size);
// }

// static void *software_barrier(int ActorID)
// {
//     for(int i = 0; i < GLOBAL_ACTORS; i++)
//         barrier_count[i][ActorID] = 1;

//     while(1)
//     {
//         int barrier_flag = 1;
//         for(int i = 0; i < GLOBAL_ACTORS; i++)
//         {
//             if(barrier_count[ActorID][i]==0)
//             {
//                 barrier_flag = 0;
//             }
//         }

//         if(barrier_flag == 1)
//         {
//             for(int i = 0; i < GLOBAL_ACTORS; i++)
//                 barrier_count[ActorID][i] = 0;

//             break;
//         }
//     }
// }

// void* operator new(std::size_t t)
// {
//     void* p = forza_malloc(t);
//     return p;
// }

// void operator delete(void * p)
// {
//     forza_free(p, 2);
// }

static void
  forza_packet_poll( int mytid, uint64_t** tail_ptr, uint64_t* head_ptr ) {
  // int MY_ACTOR = mytid;
  uint64_t poll_done = 0;
  // int qsize = 100;
  // ForzaPkt qaddr[100];
  // uint64_t **tail_ptr = (uint64_t **) forza_scratchpad_alloc(1*sizeof(uint64_t *));
  // *tail_ptr = (uint64_t *) &qaddr;
  // uint64_t *head_ptr = (uint64_t *) &qaddr;
  bool system_done   = false;

  // FIXME: If using the setup function, below add the mailbox_id argument after tail_ptr
  // forza_zen_setup((uint64_t) &qaddr, qsize*sizeof(ForzaPkt), (uint64_t) tail_ptr);

  // software_barrier(MY_ACTOR);
  // forza_zone_barrier(2);

  while( 1 ) {
    while( *tail_ptr == head_ptr ) {
      if( poll_done == ( GLOBAL_ACTORS - 1 ) )
        system_done = true;
    }

    if( system_done ) {
      break;
    }

    ForzaPkt* recv_pkt = (ForzaPkt*) head_ptr;
    switch( recv_pkt->type ) {
    case REQUEST:
      // forza_recv_process(recv_pkt->type, MY_ACTOR);
      break;
    case RESPONSE: break;
    case FORZA_DONE: poll_done++; break;
    case FORZA_BARRIER:
      // fbarriers[mythread]++;
      break;
    }
    head_ptr++;
  }

  // return NULL;
}
#endif  //_forza_lib_h_
