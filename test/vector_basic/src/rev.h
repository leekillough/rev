/*
 * rev.h
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// REV Includes
#include "rev-macros.h"
#include "syscalls.h"

// Enable tracing on assertions
#undef assert
#define assert TRACE_ASSERT

// printing helpers
#ifndef USE_SPIKE
//rev_fast_print limited to a format string, 6 simple numeric parameters, 1024 char max output
#define printf rev_fast_printf
//#define printf(...)
#else
#include <cstdio>
#endif

// Basic intrinsics
// This works on REV but just returns 0 on spike
#define RDTIME( X )                           \
  do {                                        \
    asm volatile( " rdtime %0" : "=r"( X ) ); \
  } while( 0 )

#define RDINSTRET( X )                           \
  do {                                           \
    asm volatile( " rdinstret %0" : "=r"( X ) ); \
  } while( 0 )
