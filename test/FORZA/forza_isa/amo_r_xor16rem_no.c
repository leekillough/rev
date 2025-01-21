#include <stdlib.h>

#define ORIG 0x1234
#define UPD  0x1

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main( int argc, char** argv ) {
  short          VAL = ORIG;
  short          MOD = UPD;
  volatile short RTN = __forza_amo_r_xor64rem_no( &VAL, MOD );

  assert( RTN == ORIG );
  assert( VAL == ( ORIG ^ UPD ) );

  return 0;
}
