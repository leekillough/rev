#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert(cond)                                                           \
  if (!(cond)) {                                                               \
    abort();                                                                   \
  }

int main() {
  uint64_t *addr;
  forza_zen_init(addr, (uint64_t)512);
  addr = (uint64_t *)forza_scratchpad_alloc(1 * sizeof(uint32_t));
  uint64_t size = sizeof(uint32_t);
  uint64_t dst = 1;
  forza_send(addr, size, dst);
  forza_scratchpad_free(addr, 1 * sizeof(uint32_t));
  return 0;
}
