#include <stdlib.h>

#define ORIG 0x1234
#define UPD  0x1

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main( int argc, char** argv ) {
  int          VAL = ORIG;
  int          MOD = UPD;
  volatile int RTN = __forza_amo_r_and64rem_on( &VAL, MOD );

  assert( RTN == ( ORIG & UPD ) );
  assert( VAL == ORIG );

  return 0;
}
