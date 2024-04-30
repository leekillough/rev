#include "../../../common/syscalls/forza.h"
#include <stdint.h>
#include <stdlib.h>

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main() {
  uint64_t hart_id = forza_get_hart_id();
  assert( hart_id == 0 );
  return 0;
}
