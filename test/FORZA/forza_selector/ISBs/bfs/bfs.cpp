#include "stdlib.h"
#include "../../libs/selector.forza.h"

class BFSSelector: public hclib::Selector<visitmsg> {
    std::vector<int64_t> *nextFrontier;
    int64_t *pred_glob;
    void process(visitmsg m, int sender_rank) {
        // code ported to forzalib and not keeping a version here for ease of
        // compilation.
    }

public:
BFSSelector() {
        mbx[0].process = [this](visitmsg pkt, int sender_rank) { this->process(pkt, sender_rank); };
    }
};

void *bfs_selector(int *mytid) {
    int ActorID = *(mytid);

    int root = ActorID;

    sparsemat_t *A2 = &mat[ActorID];

    if(root % THREADS == ActorID) {
        visited[ActorID][root/THREADS] = true;
        frontier[ActorID][frontierTail[ActorID]] = root/THREADS;
        frontierTail[ActorID]++;
        nvisited[ActorID]++;
    }

    while (nvisited[ActorID] < visited_size[ActorID]) {
        BFSSelector *bfss_ptr = new BFSSelector();
        hclib::finish([=]() {
                bfss_ptr->start(ActorID);
                for(int64_t i=0;i<frontierTail[ActorID];i++) {
                    for(int j=A2->loffset[frontier[ActorID][i]];j<A2->loffset[frontier[ActorID][i]+1];j++) {
                        int column = A2->lnonzero[j];
                        int dest = column % THREADS;

                        visitmsg *m = (visitmsg *) forza_malloc(1*sizeof(visitmsg));
                        m->vloc = column / THREADS;
                        m->vfrom = frontier[ActorID][i];



                        bfss_ptr->send(REQUEST, m, dest, ActorID);
                    }
                }
                bfss_ptr->done(REQUEST, ActorID);
                
            });

        int lsum = nextFrontierTail[ActorID] + 1;
        nvisited[ActorID]+=lsum;

        for(int tf = 0; tf < nextFrontierTail[ActorID]; tf++)
        {
            frontier[ActorID][tf] = nextFrontier[ActorID][tf];
            nextFrontier[ActorID][tf] = 0;
        }

        frontierTail[ActorID] = nextFrontierTail[ActorID];
        nextFrontierTail[ActorID] = 0;


    }
    
    return NULL;
}

int main(int argc, char* argv[]) {

    forza_fprintf(1, "Initialize FORZA.......\n", print_args);

    void *ptr0 = (void *) argv[0];
    print_args[0] = &ptr0;
    forza_fprintf(1, "argv[0] address - %p\n", print_args);


    void *ptr = (void *) argv[1];
    print_args[0] = &ptr;
    forza_fprintf(1, "argv[1] address - %p\n", print_args);

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
    
    frontier = (int64_t **) forza_malloc(TOTAL_PEs*sizeof(int64_t *));
    nextFrontier = (int64_t **) forza_malloc(TOTAL_PEs*sizeof(int64_t *));
    for(int i = 0; i < TOTAL_PEs; i++)
    {
        frontier[i] = (int64_t *) forza_malloc(BFS_FRONTIER_SIZE*sizeof(int64_t));
        nextFrontier[i] = (int64_t *) forza_malloc(BFS_FRONTIER_SIZE*sizeof(int64_t));
    }
    nvisited = (int64_t *) forza_malloc(TOTAL_PEs*sizeof(int64_t));
    visited_size = (int64_t *) forza_malloc(TOTAL_PEs*sizeof(int64_t));
    frontierTail = (int64_t *) forza_malloc(TOTAL_PEs*sizeof(int64_t));
    nextFrontierTail = (int64_t *) forza_malloc(TOTAL_PEs*sizeof(int64_t));

    for(int i = 0; i < TOTAL_PEs; i++)
    {
        nvisited[i] = 0;
        visited_size[i] = 0;
        frontierTail[i] = 0;
        nextFrontierTail[i] = 0;
    }

    // for(int i = 0; i < THREADS; i++)
    // {
    //     for(int j = 0; j < THREADS; j++)
    //     {
    //         mb_request[i][j].send_count = 0;
    //         mb_request[i][j].expected_resp = 0;
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
        generate_graph(mat, false, true, i);
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
            forza_thread_create(&pt[i], (void*)bfs_selector, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i + THREADS], (void *)bfs_poll_thread, &tid[i]);
        }


        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i+ THREADS]);
        }

        int test = 0;
        bool testvisited = true;
        for (int i = 0; i < visited_size[test]; i++) 
        {
            if (visited[test][i] != true) 
            {
                print_args[0] = (void *) &i;
                print_args[1] = (void *) &test;
                forza_fprintf(1, "%d is not visited on PE%d\n", print_args);
                testvisited = false;
            }
        }

        if(testvisited)
        {
            forza_fprintf(1, "All nodes visited\n", print_args);
        }

    
    });

    return 0;
}
