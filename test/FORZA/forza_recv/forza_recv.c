#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert(cond)                                                           \
  if (!(cond)) {                                                               \
    abort();                                                                   \
  }

int main() {
  uint64_t* pkt = (uint64_t *)forza_poll();
  assert(pkt == NULL);
  forza_popq();
  return 0;
}
