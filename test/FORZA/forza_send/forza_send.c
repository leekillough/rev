#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert(cond)                                                           \
  if (!(cond)) {                                                               \
    abort();                                                                   \
  }

int main() {
  uint64_t pkt = 0;
  uint64_t dst = 1;
  forza_send(pkt, dst);
  return 0;
}
