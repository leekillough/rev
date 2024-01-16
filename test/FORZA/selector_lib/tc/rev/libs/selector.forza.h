// #include <stdio.h>
#include <functional>

#include "forzalib.h"

namespace hclib {

    template <typename T>
    inline void launch(const char **deps, int ndeps, T &&lambda) {
        // forza_fprintf(1, "Actor Launch\n", print_args);
        lambda();
    }

    template <typename T>
    inline void finish (T &&lambda) {
        // forza_fprintf(1, "Actor Finish\n", print_args);
        lambda();
    }

    template<typename T>
    class Mailbox {
    public:
        std::function<void (T, int)> process;
    };

    template<typename T>
    class Selector {
    public:
        Mailbox<T> mbx[10];
        void start(int tid) 
        { 
            if(tid == 0)
            {
                print_args[0] = (void *) &tid;
                forza_fprintf(1, "Actor[%d]: Start Send\n", print_args);
            }
        }

        bool send(int mb_id, T* pkt, int rank, int ActorID) 
        { 
            // switch(mb_id)
            // {
            //     case REQUEST:
                    ForzaPkt fpkt;
                    fpkt.pkt = (void *) pkt;
                    // tpkt.pkt.w = pkt.w;
                    // tpkt.pkt.vj = pkt.vj;
                    fpkt.mb_type = mb_id;
                    forza_send(mb_request, fpkt, rank, ActorID);
                //     break;
                // case RESPONSE:
                //     break;
                // case FORZA_BARRIER:
                //     break;
                // default:
                //     // send with approproate mail box id
                //     break;
            // }

            return true;
        }

        void done(int mb_id, int ActorID) 
        {            
            // switch(mb_id)
            // {
            //     case REQUEST:
                    // forza_done(mb_request, ActorID);
                    ForzaPkt pkt;
                    pkt.pkt = NULL;
                    // pkt.pkt.w = 0;
                    // pkt.pkt.vj = 0;
                    pkt.mb_type = FORZA_DONE;
                    for(int i = 0; i < THREADS; i++)
                    {
                        forza_send(mb_request, pkt, i, ActorID);
                    }
            //         break;
            //     default:
            //         // call done for the respective mailboxes
            //         break;
            // }
            if(ActorID == 0)
            {
                print_args[0] = (void *) &ActorID;
                forza_fprintf(1, "Actor[%d]: Done Send\n", print_args);
            }
        }
    };
}

