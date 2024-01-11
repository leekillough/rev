#include <stdio.h>
#include <functional>

#include "forzalib.h"

int forza_n_pes() { return 1; }
int forza_my_pe() { return 0; }

namespace hclib {

    template <typename T>
    inline void launch(const char **deps, int ndeps, T &&lambda) {
        printf("hclib::launch\n");
        lambda();
    }

    template <typename T>
    inline void finish (T &&lambda) {
        printf("hclib::finish\n");
        lambda();
    }

    template<typename T>
    class Mailbox {
    public:
        std::function<void (T, int)> process;
    };

    template<typename T>
    class Actor {
    public:
        Mailbox<T> mb[1];
        void start(pthread_t thread_id, int tid) 
        { 
            printf("Actor::start\n"); 
            pthread_create(&thread_id, NULL, forza_poll_thread, (void *) &tid);

        }

        bool send(int mb_id, T pkt, int rank) 
        { 
            switch(mb_id)
            {
                case REQUEST:
                    TrianglePkt tpkt;
                    tpkt.w = pkt;
                    forza_send(mb_request, tpkt, rank, forza_my_pe());
                    break;
                default:
                    // send with approproate mail box id
                    break;
            }
            printf("Actor::send()\n"); 
            return true;
        }

        void done(int mb_id) 
        {            
            switch(mb_id)
            {
                case REQUEST:
                    forza_done(mb_request, forza_my_pe());
                    break;
                default:
                    // call done for the respective mailboxes
                    break;
            } 
            printf("Actor::done()\n");}
    };
}


void lgp_barrier() { }

