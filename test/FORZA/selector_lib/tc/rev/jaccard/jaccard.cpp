#include "stdlib.h"
#include "../libs/selector.forza.h"

class JaccardSelector: public hclib::Selector<JaccardPkt> {
public:
    JaccardSelector(sparsemat_t* mat, sparsemat_t* intersection_mat) : mat_(mat), intersection_mat_(intersection_mat) {
	    mbx[0].process = [this] (JaccardPkt pkt, int sender_rank) { 
	        this->req_process(pkt, sender_rank);
        };
    }

private:
    //shared variables
    sparsemat_t* mat_;
    sparsemat_t* intersection_mat_;

    void req_process(JaccardPkt pkg, int sender_rank) {
        // JaccardPkt pkg2;
        int64_t search_ = binary_search(mat_->loffset[pkg.index_u], mat_->loffset[pkg.index_u + 1] - 1, pkg.x, mat_, 0);
        if (search_ != -1) {
            pkg.pos_row = pkg.pos_row / THREADS;
            int64_t search__ = binary_search(intersection_mat_->loffset[pkg.pos_row], intersection_mat_->loffset[pkg.pos_row + 1] - 1, pkg.pos_col, mat_, 1);
            if (search__ != -1) { intersection_mat_->lvalue[search__]++; }
        }
    }
};

class DegreeSelector: public hclib::Selector<DegreePkt> {
public:
    DegreeSelector(sparsemat_t* mat, sparsemat_t* kmer_mat) : mat_(mat), kmer_mat_(kmer_mat) {
	    mbx[0].process = [this] (DegreePkt pkt, int sender_rank) { 
	        this->req_process(pkt, sender_rank);
        };
        mbx[1].process = [this] (DegreePkt pkt, int sender_rank) { 
            this->resp_process(pkt, sender_rank);
        };
    }

private:
    //shared variables
    sparsemat_t* mat_;
    sparsemat_t* kmer_mat_;

    void req_process(DegreePkt pkg, int sender_rank) {
        DegreePkt pkg2;
        pkg2.src_idx = pkg.src_idx;
        pkg2.i = kmer_mat_->loffset[pkg.i + 1] - kmer_mat_->loffset[pkg.i];
        send(RESPONSE, &pkg2, sender_rank, 0); // dummy send
    }

    void resp_process(DegreePkt pkg, int sender_rank) {
        mat_->lnonzero[pkg.src_idx] = pkg.i;
    }

};

void *get_edge_degrees(int *mytid) {
    int ActorID = *(mytid);

    DegreeSelector* degreeSelector = new DegreeSelector(&mat[ActorID], &kmer_mat[ActorID]);

    hclib::finish([=]() {
        degreeSelector->start(ActorID);
        DegreePkt degPKG;

        sparsemat_t *L = &mat[ActorID];
    
        for (int64_t y = 0; y < L->lnumrows; y++) {
            for (int64_t k = L->loffset[y]; k < L->loffset[y+1]; k++) {
                degPKG.i = L->lnonzero[k] / THREADS;
                degPKG.src_idx = k;

                DegreePkt *dpkt = (DegreePkt *) forza_malloc(1*sizeof(DegreePkt));
                dpkt->i = degPKG.i;
                dpkt->src_idx = degPKG.src_idx;
                dpkt->sender_pe = ActorID;
                degreeSelector->send(REQUEST, dpkt, L->lnonzero[k] % THREADS, ActorID); 
            }
        }

        degreeSelector->done(REQUEST, ActorID); 
    });

    return NULL;
}

void *jaccard_selector(int *mytid) {
    int ActorID = *(mytid);

    JaccardSelector* jacSelector = new JaccardSelector(&kmer_mat[ActorID], &mat[ActorID]);

    // get_edge_degrees(mat[ActorID], kmer_mat[ActorID]);
    sparsemat_t *A2 = &kmer_mat[ActorID];

    hclib::finish([=]() {
        jacSelector->start(ActorID);

        JaccardPkt pkg;

        for (int64_t v = 0; v < A2->lnumrows; v++) {         	         //vertex v (local)
            for (int64_t k = A2->loffset[v]; k < A2->loffset[v+1]; k++) {
                int64_t v_nonzero = A2->lnonzero[k];                     //vertex u (possibly remote)
                int64_t row_num = v * THREADS + ActorID;

                for (int64_t i_rows = row_num; i_rows < A2->numrows; i_rows++) {
                    // calculate intersection
                    pkg.index_u = i_rows / THREADS;
                    pkg.x = v_nonzero;
                    pkg.pos_row = i_rows;
                    pkg.pos_col = row_num;

                    JaccardPkt *jpkt = (JaccardPkt *) forza_malloc(1*sizeof(JaccardPkt));
                    jpkt->index_u = pkg.index_u;
                    jpkt->x = pkg.x;
                    jpkt->pos_row = pkg.pos_row;
                    jpkt->pos_col = pkg.pos_col;
                    jacSelector->send(REQUEST, jpkt, i_rows % THREADS, ActorID);
                }
            }
        }
        jacSelector->done(REQUEST, ActorID);
    });

    // lgp_barrier();
    sparsemat_t *Jaccard_mat = &mat[ActorID];

    for (int64_t v = 0; v < Jaccard_mat->lnumrows; v++) {
        int64_t deg_v = A2->loffset[v+1] - A2->loffset[v];
        for (int64_t k = Jaccard_mat->loffset[v]; k < Jaccard_mat->loffset[v+1]; k++) {
            Jaccard_mat->lvalue[k] = (double)Jaccard_mat->lvalue[k] / ((double)Jaccard_mat->lnonzero[k] + (double)deg_v - (double)Jaccard_mat->lvalue[k]);
        }
    }

    return NULL;
}



int main(int argc, char* argv[]) {

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
    kmer_mat = (sparsemat_t *) forza_malloc(TOTAL_PEs*sizeof(sparsemat_t));

    for(int i = 0; i < THREADS; i++)
    {
        for(int j = 0; j < THREADS; j++)
        {
            mb_request[i][j].send_count = 0;
            mb_request[i][j].expected_resp = 0;
            mb_request[i][j].recv_count = 0;
            mb_request[i][j].mbdone = 0;
            for(int k = 0; k < PKT_QUEUE_SIZE; k++)
            {
                mb_request[i][j].pkt[k].valid = 0;
            }
        }
    }
    for(int i = 0; i < THREADS; i++)
    {
        generate_graph(mat, false, false, i);
        generate_graph(kmer_mat, true, false, i);
    }

    forza_fprintf(1, "FORZA initilize done.......\n", print_args);

    const char *deps[] = { "system", "bale_actor" };
    hclib::launch(deps, 2, [=] {
		
        forza_thread_t pt[4*THREADS];

        int tid[THREADS];
        for(int i = 0; i < THREADS; i++)
        {
            tid[i] = i;
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i], (void*)get_edge_degrees, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i+THREADS], (void*)jaccard_phase1_poll, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i+THREADS]);
        }

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
        forza_fprintf(1, "Phase 2\n", print_args);

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i+(2*THREADS)], (void*)jaccard_selector, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i+(2*THREADS)]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_create(&pt[i+(3*THREADS)], (void*)jaccard_phase2_poll, &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            forza_thread_join(pt[i+(3*THREADS)]);
        }

    });

    return 0;
}