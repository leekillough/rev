#include "../../../../common/syscalls/forza.h"
#include "../../../../common/syscalls/syscalls.h"
#define assert(x)                                                              \
  if (!(x)) {                                                                  \
    asm(".byte 0x00");                                                         \
    asm(".byte 0x00");                                                         \
    asm(".byte 0x00");                                                         \
    asm(".byte 0x00");                                                         \
  }
#define SCRATCHPAD_SIZE 524288
int main(int argc, char **argv) {
  uint64_t *addr;
#define N 10
  addr = (uint64_t *)forza_scratchpad_alloc(SCRATCHPAD_SIZE + 1);

  // This should
  if (addr == NULL) {
    const char msg[41] = "alloc failed (as it should have)\n";
    uint8_t bytesWritten = rev_write(STDOUT_FILENO, &msg[0], sizeof(msg));
    return 0;
  } else {
    const char msg[50] = "scratchpad alloc succeeded but it shouldn't have\n";
    rev_write(STDOUT_FILENO, msg, sizeof(msg));
  }

  for (uint32_t i = 0; i < N; i++) {
    addr[i] = i;
  }

  for (uint32_t i = 0; i < N; i++) {
    assert(addr[i] == i);
  }

  forza_scratchpad_free(addr, SCRATCHPAD_SIZE + 1);

  return 0;
}
