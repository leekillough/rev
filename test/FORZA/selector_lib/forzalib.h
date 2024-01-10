#include <pthread.h>


#define THREADS 1


typedef struct TrianglePkt {
    int64_t w;
    int64_t vj;
    int valid;
} TrianglePkt;

typedef struct mailbox_t {
    TrianglePkt pkt[100000];
    uint64_t send_count;
    uint64_t recv_count;
    volatile int mbdone;
} mailbox;

enum MailBoxType {REQUEST, RESPONSE};

mailbox mb_request[2][2];

void forza_send(mailbox mb[][2], TrianglePkt pkt, int dest, int src)
{
    uint64_t sendcnt = mb[dest][src].send_count;
    mb[dest][src].pkt[sendcnt].w = pkt.w;
    // mb[dest][src].pkt[sendcnt].vj = pkt.vj;
    mb[dest][src].pkt[sendcnt].valid = 1;
    mb[dest][src].send_count++;
}

void forza_done(mailbox mb[][2], int src)
{
    for(int i = 0; i < THREADS; i++)
    {
        // printf("AJAY: marking %d %d as done\n", i ,src);
        mb[i][src].mbdone = 1;
    }
}

void forza_recv_process(TrianglePkt pkt)
{
    printf("Received packet-%ld\n", pkt.w);
    // put the recv handler here
}

void *forza_poll_thread(void *mytid)
{
    pthread_detach(pthread_self());
    int *mythread = (int *) mytid;
    int done_flag = 1;

    while(1)
    {
        for(int i = 0; i < THREADS; i++)
        {
            if(mb_request[*mythread][i].mbdone == 0)
            {
                // printf("AJAY: not marked as done - %d %d\n", *mythread, i);
                done_flag = 0;
                break;
            }
        }

        for(int i = 0; i < THREADS; i++)
        {
            uint64_t recvcnt = mb_request[*mythread][i].recv_count;
            while(mb_request[*mythread][i].pkt[recvcnt].valid)
            {
                forza_recv_process(mb_request[*mythread][i].pkt[recvcnt]);
                mb_request[*mythread][i].pkt[recvcnt].valid = 0;
                mb_request[*mythread][i].recv_count++;
                recvcnt++;
            }
        }

        if(done_flag)
            break;

        // printf("Looping?\n");
    }

    pthread_exit(NULL);
    // return NULL;
}