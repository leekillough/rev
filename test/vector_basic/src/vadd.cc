/*
 * apptest.cpp
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// Standard includes
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// REV
#include "rev.h"

uint64_t s0[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
uint64_t s1[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600 };

void check( uint64_t result[] ) {
  for( int i = 0; i < 16; i++ ) {
    assert( result[i] == ( s0[i] + s1[i] ) );
  }
}

size_t add_scalar( uint64_t a[], uint64_t b[], uint64_t c[] ) {
  size_t time0, time1;
  REV_TIME( time0 );
  for( int i = 0; i < 16; i++ ) {
    c[i] = a[i] + b[i];
  }
  REV_TIME( time1 );
  return time1 - time0;
}

int main( int argc, char** argv ) {
  size_t   time_scalar  = 0;
  size_t   time_vector  = 0;
  uint64_t r_scalar[16] = { 0 };
  time_scalar           = add_scalar( s0, s1, r_scalar );
  check( r_scalar );
  printf( "Cycle counts: scalar=%ld vector=%ld\n", time_scalar, time_vector );
}
