#include <stdlib.h>

#define ORIG 0x12
#define UPD  0x1

#define assert( cond ) \
  if( !( cond ) ) {    \
    abort();           \
  }

int main( int argc, char** argv ) {
  char VAL = ORIG;
  char MOD = UPD;
  __forza_amo_r_or64u( &VAL, MOD );

  assert( VAL == ( ORIG ^ UPD ) );

  return 0;
}
