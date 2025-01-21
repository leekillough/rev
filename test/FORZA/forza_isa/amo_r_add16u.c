#include <stdlib.h>

#define ORIG 0x1234
#define UPD  0x1

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main( int argc, char** argv ) {
  short VAL = ORIG;
  short MOD = UPD;
  __forza_amo_r_add64u( &VAL, MOD );

  assert( VAL == ( ORIG + UPD ) );

  return 0;
}
