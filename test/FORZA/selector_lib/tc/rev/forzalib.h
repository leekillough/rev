
#include <stdio.h>
#include <stdlib.h>
#include "forza_utils.h"

int forza_thread_create(forza_thread_t *tid, void *fn, void *arg)
{
    int ret;
    ret = rev_pthread_create(tid, NULL, fn, arg);
    return ret;
}

int forza_thread_join(forza_thread_t tid)
{
    int ret;
    ret = rev_pthread_join(tid);
    return ret;
}

uint64_t forza_file_read(char *filename, char *buf, size_t count)
{
    int fd = rev_openat(AT_FDCWD, filename, 0, O_RDONLY);
    uint64_t bytesRead = rev_read(fd, buf, count);
    rev_close(fd);
    return bytesRead;
}


typedef struct sparsemat_t_ {
  int64_t local;                //!< 0/1 flag specifies whether this is a local or distributed matrix
  int64_t numrows;              //!< the total number of rows in the matrix
  int64_t lnumrows;             //!< the number of rows on this PE 
                                // note lnumrows = (numrows / NPES) + {0 or 1} 
                                //    depending on numrows % NPES  
  int64_t numcols;              //!< the nonzeros have values between 0 and numcols
  int64_t nnz;                  //!< total number of nonzeros in the matrix
  int64_t lnnz;                 //!< the number of nonzeros on this PE
  //int64_t * offset;      //!< the row offsets into the array of nonzeros
  int64_t loffset[1000];            //!< the row offsets for the row with affinity to this PE
  //int64_t * nonzero;     //!< the global array of column indices of nonzeros
  int64_t lnonzero[1000];           //!< local array of column indices (for rows on this PE).
  //double * value;        //!< the global array of values of nonzeros. Optional.
  //double * lvalue;              //!< local array of values (values for rows on this PE)
}sparsemat_t;

typedef struct TrianglePkt {
    int64_t w;
    int64_t vj;
    volatile int64_t valid;
} TrianglePkt;

typedef struct mailbox_t {
    TrianglePkt pkt[1000];
    uint64_t send_count;
    uint64_t recv_count;
    volatile int mbdone;
} mailbox;

enum MailBoxType {REQUEST, RESPONSE};

mailbox mb_request[4][4];
sparsemat_t mat[4];
int64_t cnt[4];

void generate_graph(sparsemat_t *mat, int mynode)
{
    char buffer1[BUF_SIZE];
    char buffer2[BUF_SIZE][10];
    char path[40];

    switch(mynode)
    {
      case 0:
          strcpy(path,"node0.dat");
          break;
      case 1:
          strcpy(path,"node1.dat");
          break;
      case 2:
          strcpy(path,"node2.dat");
          break;
      case 3:
          strcpy(path,"node3.dat");
          break;
      default:
          break;        
    }

    uint64_t bytesRead = forza_file_read(path, buffer1, 50000);
    buffer1[bytesRead] = '\0';

    uint64_t j = 0;
    uint64_t k = 0;

    for(uint64_t i = 0; i < bytesRead; i++)
    {
      if(buffer1[i] == '\n')
      {
        buffer2[k][j] = '\0';
        k++;
        j = 0;
        continue;
      }

      buffer2[k][j] = buffer1[i];
      j++; 
    }

    mat[mynode].local = atoi(buffer2[0]); 		//my_atoi should be replaced with sscanf
    mat[mynode].numrows = atoi(buffer2[1]);
    mat[mynode].lnumrows = atoi(buffer2[2]);
    mat[mynode].numcols = atoi(buffer2[3]);
    mat[mynode].nnz = atoi(buffer2[4]);
    mat[mynode].lnnz = atoi(buffer2[5]);

    for(int lrows = 0; lrows < mat[mynode].lnumrows + 1; lrows++)
    {
        mat[mynode].loffset[lrows] = atoi(buffer2[6+lrows]);
    }

    int zero_offset = 6 + mat[mynode].lnumrows + 1;

    for(int lzeros = 0; lzeros < mat[mynode].lnnz; lzeros++)
    {
        mat[mynode].lnonzero[lzeros] = atoi(buffer2[zero_offset + lzeros]);
    }
    return;
}

void forza_send(mailbox mb[][4], TrianglePkt pkt, int dest, int src)
{
    uint64_t sendcnt = mb[dest][src].send_count;
    mb[dest][src].pkt[sendcnt].w = pkt.w;
    mb[dest][src].pkt[sendcnt].vj = pkt.vj;
    mb[dest][src].pkt[sendcnt].valid = 1;
    mb[dest][src].send_count++;
}

void forza_done(mailbox mb[][4], int src)
{
    for(int i = 0; i < THREADS; i++)
    {
        mb[i][src].mbdone = 1;
    }
}

void forza_recv_process(TrianglePkt pkg, int AID)
{
    int64_t tempCount = 0;

    for (int64_t k = mat[AID].loffset[pkg.vj]; k < mat[AID].loffset[pkg.vj + 1]; k++) {
        if (pkg.w == mat[AID].lnonzero[k]) {
            tempCount++;
            break;
        }
        if (pkg.w < mat[AID].lnonzero[k]) { // requires that nonzeros are increasing
            break;
        }
    }
    // update the share variable
    cnt[AID] += tempCount;
}

void *forza_poll_thread(int *mytid)
{
    int mythread = *mytid;
    volatile int done_flag = 1;
    
    while(1)
    {
        done_flag = 1;
        for(int i = 0; i < THREADS; i++)
        {
            if(mb_request[mythread][i].mbdone == 0)
            {
                done_flag = 0;
                break;
            }
        }

        for(int i = 0; i < THREADS; i++)
        {
            uint64_t recvcnt = mb_request[mythread][i].recv_count;
            print_args[0] = (void *) &recvcnt;

            while(mb_request[mythread][i].pkt[recvcnt].valid)
            {
                forza_recv_process(mb_request[mythread][i].pkt[recvcnt], mythread);
                mb_request[mythread][i].pkt[recvcnt].valid = 0;
                mb_request[mythread][i].recv_count++;
                recvcnt++;
            }
        }

        if(done_flag)
        {
            break;
        }
    }

    return NULL;

}