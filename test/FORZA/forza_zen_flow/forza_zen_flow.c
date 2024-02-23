
#include "../../../common/syscalls/forza.h"
#include <stdlib.h>
#include <stdint.h>

#define assert(x)                                                              \
  do                                                                           \
    if (!(x)) {                                                                \
      asm(".dword 0x00000000");                                                \
    }                                                                          \
  while (0)

int main(int argc, char **argv)
{
  //int TID = atoi(argv[1]);
  int TID = forza_get_my_zap();
  int qsize = 10;
  uint64_t qaddr[10];
  volatile uint64_t *my_tail_ptr; //address that zen should update
  my_tail_ptr = (uint64_t *)forza_scratchpad_alloc(1*sizeof(uint64_t *));
  *my_tail_ptr = 0xdeadbeef; // initialize contents of tail ptr to dummy data
  uint64_t *my_recv_buffer = &qaddr[0];
  //uint64_t *my_recv_buffer = (uint64_t *)forza_scratchpad_alloc(qsize*sizeof(uint64_t));

  volatile uint64_t *cur_recv_ptr;
  cur_recv_ptr = &qaddr[0];

  forza_get_my_precinct((uint64_t)cur_recv_ptr, (uint64_t)my_tail_ptr, *my_tail_ptr);

  // Buffer for me can be memory or scratchpad; since executing one hart per zone, use
  // memory for now
  forza_zen_setup((uint64_t)my_recv_buffer, qsize*sizeof(uint64_t), (uint64_t)my_tail_ptr);
 
  forza_zone_barrier(2); // two executing harts

  forza_get_my_precinct((uint64_t)cur_recv_ptr, (uint64_t)my_tail_ptr, *my_tail_ptr);

  uint64_t *pkt = (uint64_t *) forza_scratchpad_alloc(1*sizeof(uint64_t));
  *pkt = 690;

  if(TID == 0)
    forza_send(1, (uint64_t)pkt, sizeof(uint64_t));

  uint64_t recv_pkt = 0;

  //return 0;

  if(TID == 1)
  {
    forza_get_my_precinct((uint64_t)cur_recv_ptr, (uint64_t)my_tail_ptr, *my_tail_ptr);
    while((uint64_t)cur_recv_ptr == *my_tail_ptr){
	    forza_get_my_precinct((uint64_t)cur_recv_ptr, (uint64_t)my_tail_ptr, *my_tail_ptr);
    }
    recv_pkt = *cur_recv_ptr;
    assert(recv_pkt == 690);
    cur_recv_ptr++;
    forza_zen_credit_release(sizeof(uint64_t));
  }

  forza_scratchpad_free((uint64_t)my_tail_ptr, sizeof(uint64_t*));
  //forza_scratchpad_free((uint64_t)my_recv_buffer, qsize*sizeof(uint64_t));
  forza_scratchpad_free((uint64_t)pkt, sizeof(uint64_t));

  return 0;
}
