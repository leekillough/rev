#include "stdint.h"
#include "selector.forza.h"

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


void *triangle_selector(void *mytid) {
    int ActorID = *(int *) mytid;
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

                    triSelector->send(REQUEST, pkg, pe, ActorID);
                }
            }
        }
        // Indicate that we are done with sending messages to the REQUEST mailbox
        triSelector->done(REQUEST, ActorID);
    });

    return NULL;
}


int main(int argc, char* argv[]) {

    const char *deps[] = { "system", "bale_actor" };
    hclib::launch(deps, 2, [=] {
        
        for(int i = 0; i < THREADS; i++)
        {
            generate_graph(mat, i);
        }

        printf("L has %ld rows/cols and %ld nonzeros.\n", mat[0].numrows, mat[0].nnz);
    
        printf("Run triangle counting ...\n");

        pthread_t pt[THREADS];
        // pthread_t t5, t6, t7, t8;
        int tid[THREADS];
        for(int i = 0; i < THREADS; i++)
        {
            tid[i] = i;
        }

        for(int i = 0; i < THREADS; i++)
        {
            pthread_create(&pt[i], NULL, triangle_selector, (void *) &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            pthread_join(pt[i], NULL);
        }

        for(int i = 0; i < THREADS; i++)
        {
            pthread_create(&pt[i], NULL, forza_poll_thread, (void *) &tid[i]);
        }

        for(int i = 0; i < THREADS; i++)
        {
            pthread_join(pt[i], NULL);
        }

    });

    uint64_t total_tri_cnt = 0;

    for(int i = 0; i < THREADS; i++)
    {
        total_tri_cnt += cnt[i];
        // printf("Triangles[%d]-%ld\n", i, cnt[i]);
    }
    printf(" %16ld triangles\n", total_tri_cnt);
    return 0;
}
