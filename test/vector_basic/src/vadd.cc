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

void add_scalar( uint64_t a[], uint64_t b[], uint64_t c[] ) {
  for( int i = 0; i < 16; i++ ) {
    c[i] = a[i] + b[i];
  }
}

int main( int argc, char** argv ) {
  uint64_t r[16] = { 0 };
  add_scalar( s0, s1, r );
  check( r );
}
