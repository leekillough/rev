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

// Test helpers
#define xstr( s ) str( s )
#define str( s )  #s

#define CHECKU64( VAR, EXPECTED )                                   \
  do {                                                              \
    if( EXPECTED != VAR ) {                                         \
      printf( "Error: " #VAR " E=0x%lx A=0x%lx\n", EXPECTED, VAR ); \
      assert( EXPECTED == VAR );                                    \
    }                                                               \
  } while( 0 )

#define CHECKU16( VAR, EXPECTED )                                 \
  do {                                                            \
    if( EXPECTED != VAR ) {                                       \
      printf( "Error: " #VAR " E=0x%x A=0x%x\n", EXPECTED, VAR ); \
      assert( EXPECTED == VAR );                                  \
    }                                                             \
  } while( 0 )

// Assembly helpers

// Vector CSR URW
#define csr_vstart  0x008
#define csr_vxsat   0x009
#define csr_vxrm    0x00a
#define csr_vcsr    0x00f
// Counters
#define csr_cycle   0xc00
#define csr_time    0xc01
#define csr_instret 0xc02
// Vector CSR URO
#define csr_vl      0xc20
#define csr_vtype   0xc21
#define csr_vlenb   0xc22

// rdtime works on REV but just returns 0 on spike
#define RDTIME( X )                           \
  do {                                        \
    asm volatile( " rdtime %0" : "=r"( X ) ); \
  } while( 0 )

#define RDINSTRET( X )                           \
  do {                                           \
    asm volatile( " rdinstret %0" : "=r"( X ) ); \
  } while( 0 )

//"vsetvli t0,  a0,     e8,     m1,  ta, ma  \n\t"
#define VSETVLI( RD, RS1, SEW, LMUL, VTA, VMA )                                                                               \
  do {                                                                                                                        \
    asm volatile( " vsetvli %0, %1, " str( SEW ) ", " str( LMUL ) ", " str( VTA ) "," str( VMA ) : "=r"( RD ) : "r"( RS1 ) ); \
  } while( 0 )

// vsetivli rd, uimm, vtypei # rd=new vl, uimm=AVL, vtypei=new vtype setting
#define VSETIVLI( RD, UIMM5, SEW, LMUL, VTA, VMA )                                                                             \
  do {                                                                                                                         \
    asm volatile( " vsetivli %0," str( UIMM5 ) ", " str( SEW ) ", " str( LMUL ) ", " str( VTA ) "," str( VMA ) : "=r"( RD ) ); \
  } while( 0 )

// vsetvl rd, rs1, rs2    # rd=new vl, rs1=AVL, rs2=new vtype value
#define VSETVL( RD, RS1, RS2 )                                                   \
  do {                                                                           \
    asm volatile( " vsetvl %0, %1, %2 " : "=r"( RD ) : "r"( RS1 ), "r"( RS2 ) ); \
  } while( 0 )

// CSR Access: CSRRW CSRRS CSRRC CSRRWI CSRRSI CSRRCI

#define CSRRW( RD, CSR, RS1 )                                                  \
  do {                                                                         \
    asm volatile( " csrrw %0, " str( CSR ) ", %1" : "=r"( RD ) : "r"( RS1 ) ); \
  } while( 0 )

#define CSR_READ( RD, CSR )                                       \
  do {                                                            \
    asm volatile( " csrrsi %0, " str( CSR ) ", 0" : "=r"( RD ) ); \
  } while( 0 )

#define CSR_WRITE( RS1, CSR )                                       \
  do {                                                              \
    asm volatile( " csrrw zero, " str( CSR ) ", %1" : "r"( RS1 ) ); \
  } while( 0 )

// CSR Types
enum vsew { e8 = 0, e16 = 1, e32 = 2, e64 = 3 };

enum vlmul { m1 = 0, m2 = 1, m4 = 2, m8 = 3, mf8 = 5, mf4 = 6, mf2 = 7 };

union reg_vtype_t {
  uint64_t v = 0x0UL;

  struct {
    uint64_t vlmul : 3;   // [2:0] 0:mf1 1:mf2 2:m4 3:mf8 4:x 5:mf8 6:mf4 7:mf2
    uint64_t vsew  : 3;   // [5:3] 0:e8 1:e16 2:e32 3:e64
    uint64_t vta   : 1;   // [6]
    uint64_t vma   : 1;   // [7]
    uint64_t zero  : 55;  // [62:8]
    uint64_t vii   : 1;   // 63
  } f;

  void dump() {
    printf( "vtype  (0x%016lx) vii=0x%x vma=0x%x vta=0x%x vsew=0x%x vlmul=0x%x\n", v, f.vii, f.vma, f.vta, f.vsew, f.vlmul );
  }
};

union reg_vl_t {
  uint64_t v = 0UL;

  struct {
    uint64_t l : 7;  // [6:0]
  } f;

  void dump() { printf( "vl     (0x%016lx) l=0x%x\n", v, f.l ); }
};

// #define csr_vstart  0x008
// #define csr_vxsat   0x009
// #define csr_vxrm    0x00a
// #define csr_vcsr    0x00f
// // Counters
// #define csr_cycle   0xc00
// #define csr_time    0xc01
// #define csr_instret 0xc02
// // Vector CSR URO
// #define csr_vl      0xc20
// #define csr_vlenb   0xc22

// Vector Load/Store (VLE16.v/VSE16.v ...)
#define VL( VTYPE, VD, RS1 )                                                  \
  do {                                                                        \
    asm volatile( " vl" str( VTYPE ) " " str( VD ) ", (%0)" : : "r"( RS1 ) ); \
  } while( 0 )

#define VS( VTYPE, SUFFIX, VS3, RS1 )                                          \
  do {                                                                         \
    asm volatile( " vs" str( VTYPE ) " " str( VS3 ) ", (%0)" : : "r"( RS1 ) ); \
  } while( 0 )

// Vector Integer operations
#define VADD( SUFFIX, VD, VS2, VS1 )                                                    \
  do {                                                                                  \
    asm volatile( " vadd" str( SUFFIX ) " " str( VD ) "," str( VS2 ) ", " str( VS1 ) ); \
  } while( 0 )

union reg_vstart_t {
  uint64_t v = 0x0UL;

  struct {
    uint64_t eindex : 7;  // [6:0] VLEN=128, 2**7=128
  } f;

  void dump() { printf( "vstart (0x%016lx) eindex=0x%x\n", v, f.eindex ); }
};
