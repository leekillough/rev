#include <stdint.h>
#include <stdlib.h>

uint64_t atom64 = 0;
uint32_t atom32 = 0;
uint64_t A[8192];
uint64_t B[8192];
uint64_t R[8192];

int      main() {
  uint64_t i = 0;
  uint64_t j = 0;
  int      r = 0;
  for( i = 0; i < 8192; i++ ) {
    R[i] = A[i] + B[i] * i;
  }

  return 0;
}
