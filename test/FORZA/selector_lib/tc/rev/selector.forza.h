// #include <stdio.h>
#include <functional>

#include "forzalib.h"

namespace hclib {

    template <typename T>
    inline void launch(const char **deps, int ndeps, T &&lambda) {
        lambda();
    }

    template <typename T>
    inline void finish (T &&lambda) {
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
        Mailbox<T> mbx[1];
        void start(int tid) 
        { 

        }

        bool send(int mb_id, T pkt, int rank, int ActorID) 
        { 
            switch(mb_id)
            {
                case REQUEST:
                    TrianglePkt tpkt;
                    tpkt.w = pkt.w;
                    tpkt.vj = pkt.vj;
                    forza_send(mb_request, tpkt, rank, ActorID);
                    break;
                default:
                    // send with approproate mail box id
                    break;
            }

            return true;
        }

        void done(int mb_id, int ActorID) 
        {            
            switch(mb_id)
            {
                case REQUEST:
                    forza_done(mb_request, ActorID);
                    break;
                default:
                    // call done for the respective mailboxes
                    break;
            } 
        }
    };
}


// void lgp_barrier(uint64_t TID) { 
//     forza_barrier(TID);
// }

