#ifndef BSRLIB_HEADER
#define BSRLIB_HEADER

#define NTHREAD 1

// use need to define NTHREAD by themselves
// #ifndef NTHREAD
// #define NTHREAD 8
// #endif

// users need to define msg_t struct by themselves
// typedef struct msg_struct {
//  uint64_t local_sum;
// } msg_t;

#include <inttypes.h>

typedef struct barrier_struct {
  volatile uint64_t t[NTHREAD];
} barrier_t;

barrier_t DEFAULT_BARRIER_INSTANCE = { .t = { 0ul } };

// void barrier(volatile barrier_t *p, int mythread) {
void barrier( uint64_t mythread ) {
  volatile barrier_t* p = &DEFAULT_BARRIER_INSTANCE;

  p->t[mythread]++;
  uint64_t a = p->t[mythread];
  while( 1 ) {
    int ok = 1;
    for( int i = 0; i < NTHREAD; i++ ) {
      if( p->t[i] < a ) {
        ok = 0;
      }
    }
    if( ok ) {
      break;
    }
  }
}

// typedef struct mailbox_struct {
//  int busy;
//  msg_t m;
// } mailbox_t;

// mailbox_t DEFAULT_MBOX_INSTANCE[NTHREAD * NTHREAD];

// // void send(mailbox_t *mbox_base, int send_id, int recv_id, const msg_t *m) {
// void send(uint64_t send_id, uint64_t recv_id, volatile msg_t *m) {
//     volatile mailbox_t *mbox_addr = &(DEFAULT_MBOX_INSTANCE[0]) + (recv_id * NTHREAD + send_id);
//     while (mbox_addr->busy);
//     mbox_addr->m = *m;
//     mbox_addr->busy = 1;
// }

// // void recv(mailbox_t *mbox_base, int send_id, int recv_id, msg_t *m) {
// void recv(uint64_t send_id, uint64_t recv_id, volatile msg_t *m) {
//     volatile mailbox_t *mbox_addr = &(DEFAULT_MBOX_INSTANCE[0]) + (recv_id * NTHREAD + send_id);
//     while (!mbox_addr->busy);
//     *m = mbox_addr->m;
//     mbox_addr->busy = 0;
// }


#endif
