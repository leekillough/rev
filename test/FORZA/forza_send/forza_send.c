#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert(cond)                                                           \
  if (!(cond)) {                                                               \
    abort();                                                                   \
  }

int main() {
  uint64_t qaddr;
  uint64_t tail_ptr;
  forza_zen_setup(qaddr, (uint64_t)512, tail_ptr);
  uint64_t *pkt = (uint64_t *)forza_scratchpad_alloc(1 * sizeof(uint32_t));
  uint64_t size = sizeof(uint32_t);
  uint64_t dst = 1;
  forza_send(dst, pkt, size);
  forza_scratchpad_free(pkt, 1 * sizeof(uint32_t));
  return 0;
}
