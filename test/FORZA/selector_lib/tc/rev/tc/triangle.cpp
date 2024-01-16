#include "stdlib.h"
#include "../libs/selector.forza.h"

class TriangleSelector: public hclib::Selector<TrianglePkt> {
public:
    TriangleSelector(int64_t* cnts, sparsemat_t* mat) : cnt_(cnts), mat_(mat) {
        mbx[REQUEST].process = [this] (TrianglePkt pkt, int sender_rank) { 
            this->req_process(pkt, sender_rank);
        };
    }

private:
    //shared variables
    int64_t* cnt_;
    sparsemat_t* mat_;

    void req_process(TrianglePkt pkg, int sender_rank) {
        int64_t tempCount = 0;

        for (int64_t k = mat_->loffset[pkg.vj]; k < mat_->loffset[pkg.vj + 1]; k++) {
            if (pkg.w == mat_->lnonzero[k]) {
                tempCount++;
                break;
            }
            if (pkg.w < mat_->lnonzero[k]) { // requires that nonzeros are increasing
                break;
            }
        }

        // update the share variable
        *cnt_ += tempCount;
    }
};


void *triangle_selector(int *mytid) {
    int ActorID = *(mytid);

    // pthread_t thread_id;
    TriangleSelector* triSelector = new TriangleSelector(&cnt[ActorID], &mat[ActorID]);

    hclib::finish([=]() {
        triSelector->start(ActorID);
        int64_t k,kk, pe;
        int64_t l_i, L_j;

        TrianglePkt pkg;

        for (l_i = 0; l_i < mat[ActorID].lnumrows; l_i++) {
            for (k = mat[ActorID].loffset[l_i]; k < mat[ActorID].loffset[l_i + 1]; k++) {
                // L_i = l_i * THREADS + ActorID;
                L_j = mat[ActorID].lnonzero[k];

                pe = L_j % THREADS;
                pkg.vj = L_j / THREADS;
                for (kk = mat[ActorID].loffset[l_i]; kk < mat[ActorID].loffset[l_i + 1]; kk++) {
                    pkg.w = mat[ActorID].lnonzero[kk];

                    if (pkg.w > L_j) {
                        break;
                    }
            
                    TrianglePkt *tpkt = (TrianglePkt *) forza_malloc(1*sizeof(TrianglePkt));
                    tpkt->w = pkg.w;
                    tpkt->vj = pkg.vj;

                    triSelector->send(REQUEST, tpkt, pe, ActorID);
                }
            }
        }
        // Indicate that we are done with sending messages to the REQUEST mailbox
        triSelector->done(REQUEST, ActorID);
    });

    return NULL;
}


int main(int argc, char *argv[]) {

    // print_args[0] = (void *) &argc;
    // forza_fprintf(1, "AJ:Main-%d\n", print_args);

    // assert(atoi(argv[1]) == 99);
    forza_fprintf(1, "Initialize FORZA.......\n", print_args);

    // int total_pe = atoi(argv[1]);
    void *ptr0 = (void *) argv[0];
    print_args[0] = &ptr0;
    forza_fprintf(1, "argv[0] address - %p\n", print_args);
    // forza_fprintf(1, "AJ:got argv[0] string - %s\n", print_args);


    void *ptr = (void *) argv[1];
    print_args[0] = &ptr;
    forza_fprintf(1, "argv[1] address - %p\n", print_args);
    // forza_fprintf(1, "AJ:got argv[1] string - %s\n", print_args);

    int TOTAL_PEs = atoi(argv[1]);
    print_args[0] = (void *) &TOTAL_PEs;
    forza_fprintf(1, "Total Actors per zone-%d\n", print_args);

    uint64_t TOTAL_PE_SYSTEM = atoi(argv[2]);
    print_args[0] = (void *) &TOTAL_PE_SYSTEM;
    forza_fprintf(1, "Total Actors in system-%d\n", print_args);

    THREADS = TOTAL_PEs;
    TOTAL_THREADS = TOTAL_PE_SYSTEM;
    fbarriers = (int *) forza_malloc(TOTAL_PE_SYSTEM*sizeof(int));

    mb_request = (mailbox **) forza_malloc(TOTAL_PEs*sizeof(mailbox*));
    for(int i = 0; i < TOTAL_PEs; i++)
    {
        mb_request[i] = (mailbox *) forza_malloc(TOTAL_PEs*sizeof(mailbox));
    }

    mat = (sparsemat_t *) forza_malloc(TOTAL_PEs*sizeof(sparsemat_t));
    cnt = (int64_t *) forza_malloc(TOTAL_PEs*sizeof(int64_t));

    // int test1 = atoi(argv[2]);
    // print_args[0] = (void *) &test1;
    // forza_fprintf(1, "AJ:Main-%d\n", print_args);

    // int test2 = atoi(argv[3]);
    // print_args[0] = (void *) &test2;
    // forza_fprintf(1, "AJ:Main-%d\n", print_args);


    // for(int i = 0; i < THREADS; i++)
    // {
    //     for(int j = 0; j < THREADS; j++)
    //     {
    //         mb_request[i][j].send_count = 0;
    //         mb_request[i][j].recv_count = 0;
    //         mb_request[i][j].mbdone = 0;
    //         for(int k = 0; k < PKT_QUEUE_SIZE; k++)
    //         {
    //             mb_request[i][j].pkt[k].valid = 0;
    //         }
    //     }
    // }


    for(int i = 0; i < THREADS; i++)
    {
        generate_graph(mat, false, i);
    }
    forza_fprintf(1, "FORZA initilize done.......\n", print_args);
    
    const char *deps[] = { "system", "bale_actor" };

    hclib::launch(deps, 2, [=] {

        
        forza_thread_t pt[2*THREADS];

        int tid[THREADS];
        for(int i = 0; i < THREADS; i++)
        {
            tid[i] = i;
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i], (void*)triangle_selector, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i + THREADS], (void *)forza_poll_thread, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i]);
        }
        // print_args[0] = (void *) &mb_request[0][0].send_count;
        // print_args[1] = (void *) &mb_request[0][1].send_count;
        // print_args[2] = (void *) &mb_request[1][0].send_count;
        // print_args[3] = (void *) &mb_request[1][1].send_count;
        // forza_fprintf(1, "Send - %l, %l %l %l\n", print_args);


        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i+ THREADS]);
        }

    });

    uint64_t total_tri_cnt = 0;

    for(int i = 0; i < THREADS; i++)
    {
        total_tri_cnt += cnt[i];
    }

    print_args[0] = (void *) &total_tri_cnt;
    forza_fprintf(1, "Triangles-%l\n", print_args);

    return 0;
}
