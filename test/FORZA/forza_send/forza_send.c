#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert(cond)                                                           \
  if (!(cond)) {                                                               \
    abort();                                                                   \
  }

int main() {
// THIS TEST IS DISABLED BECAUSE WE HAVE NOT YET CHANGED THE PYTHON CONFIG FILE TO ACTUALLY LET SEND WORK
#if 0
  uint64_t qaddr;
  uint64_t tail_ptr;
  uint64_t *pkt = (uint64_t *)forza_scratchpad_alloc(16 * sizeof(uint64_t));
  forza_zen_setup(qaddr, 16, tail_ptr);
  forza_zen_setup((uint64_t)pkt, (uint64_t)16, (uint64_t)pkt);
  uint64_t size = sizeof(uint64_t);
  uint64_t dst = 1;
  forza_send(dst, pkt, size);
  forza_scratchpad_free(pkt, 16 * sizeof(uint64_t));
#endif
  return 0;
}
