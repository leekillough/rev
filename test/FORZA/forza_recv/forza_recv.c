#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main() {
// THIS TEST IS DISABLED BECAUSE WE HAVE NOT YET CHANGED THE PYTHON CONFIG FILE TO ACTUALLY LET SEND WORK
#if 0
  uint64_t qaddr = 0;
  uint64_t tail_ptr = 1;
  uint64_t mbox_id = 0;
  forza_zen_setup(qaddr, (uint64_t)512, tail_ptr, mbox_id);

  uint64_t head_ptr = qaddr;
  while(head_ptr != tail_ptr){
    head_ptr++;
    forza_zen_credit_release(1);
  }
#endif
  return 0;
}
