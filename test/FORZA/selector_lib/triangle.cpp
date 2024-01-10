
// #ifndef FORZA
// #include <shmem.h>
// extern "C" {
// #include <spmat.h>
// }
// #include "selector.h"

// #define THREADS shmem_n_pes()
// #define MYTHREAD shmem_my_pe()

// #else
// int64_t
#include "stdint.h"
// selector
#include "selector.forza.h"

// #include <pthread.h>

// #define THREADS forza_n_pes()
#define MYTHREAD forza_my_pe()
// #endif

class TestActor: public hclib::Actor<int64_t> {
  int64_t *lval;

  void process(int64_t pkt, int sender_rank) {
      printf("PROCESS [Sender rank: %d], [Pkt: %ld]\n", sender_rank, pkt);
      *lval = pkt;
  }

  public:
    TestActor(int64_t *lval_) : lval(lval_) {
        mb[0].process = [this](int64_t pkt, int sender_rank) { this->process(pkt, sender_rank);};
    }
};

int main(int argc, char * argv[]) 
{
  int tid = MYTHREAD;

  const char *deps[] = { "system", "bale_actor" };

  hclib::launch(deps, 2, [=] {
      pthread_t thread_id;
      int64_t val = 0;
      TestActor *actor = new TestActor(&val);
      printf("INIT [PE%d] val = %ld\n", MYTHREAD, val);
      hclib::finish([=, &thread_id]() {
          actor->start(thread_id, tid);

          int64_t pkt = 99;
          int64_t dst = (MYTHREAD + 1) % THREADS;
          actor->send(0, pkt, dst);

          pkt = 98;
          dst = (MYTHREAD + 1) % THREADS;
          actor->send(0, pkt, dst);

          pkt = 97;
          dst = (MYTHREAD + 1) % THREADS;
          actor->send(0, pkt, dst);

          pkt = 96;
          dst = (MYTHREAD + 1) % THREADS;
          actor->send(0, pkt, dst);
          
          actor->done(0);
      });
      lgp_barrier();
      printf("DONE [PE%d] val = %ld\n", MYTHREAD, val);
  });


  pthread_exit(NULL);

//   forza_poll_thread(MYTHREAD);

  return 0;
}

