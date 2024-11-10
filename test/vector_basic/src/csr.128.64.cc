/*
 * csr.128.64.cc
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

// local copies of CSRs
reg_vstart_t vstart;
uint64_t     vxsat = 0xbadbeefUL;
uint64_t     vxrm  = 0xbadbeefUL;
uint64_t     vcsr  = 0xbadbeefUL;
reg_vl_t     vl;
reg_vtype_t  vtype;
uint64_t     vlenb = 0xbadbeefUL;

// initial vector data
uint16_t s0[32]    = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                       0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20 };

uint16_t s1[32]    = { 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00,
                       0x0c00, 0x0d00, 0x0e00, 0x0f00, 0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600,
                       0x1700, 0x1800, 0x1900, 0x1a00, 0x1b00, 0x1c00, 0x1d00, 0x1e00, 0x1f00, 0x2000 };

void read_and_dump_registers() {
  // URW
  CSR_READ( vstart, csr_vstart );
  CSR_READ( vxsat, csr_vxsat );
  CSR_READ( vxrm, csr_vxrm );
  CSR_READ( vcsr, csr_vcsr );
  // URO
  CSR_READ( vl, csr_vl );
  CSR_READ( vtype, csr_vtype );
  CSR_READ( vlenb, csr_vlenb );

  vstart.dump();
  printf( "vxsat  (0x%016lx)\n", vxsat );
  printf( "vxrm   (0x%016lx)\n", vxrm );
  printf( "vcsr   (0x%016lx)\n", vcsr );
  vl.dump();
  vtype.dump();
  printf( "vlenb  (0x%016lx)\n", vlenb );
}

int main( int argc, char** argv ) {

  // 33.3.11. State of Vector Extension at Reset
  // The vector extension must have a consistent state at reset. In particular,
  // vtype and vl must have values that can be read and then restored with a
  // single vsetvl instruction.
  // It is recommended that at reset, vtype.vill is set, the remaining bits in
  // vtype are zero, and vl is set to zero.
  // The vstart, vxrm, vxsat CSRs can have arbitrary values at reset.
  // Most uses of the vector unit will require an initial vset{i}vl{i}, which
  // will reset vstart. The vxrm and vxsat fields should be reset explicitly
  // in software before use.
  // The vector registers can have arbitrary values at reset.

  printf( "initial csr values\n" );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0UL );
  CHECKU64( vtype.v, 0x8000000000000000UL );
  CHECKU64( vlenb, 16UL );

  // Part 1: Check vsetvli, vsetivli, vsetvl instructions

  // vsetvli rd, rs1, vtypei   # rd=new vl, rs1=avl, imm_sew, lmul, vta, vma
  printf( "\nvsetvli: avl=128, e8, m1, ta, ma\n" );
  uint64_t newvl = 0;
  uint64_t avl   = 128;
  VSETVLI( newvl, avl, e8, m1, ta, ma );
  CHECKU64( newvl, 16UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x10UL );
  CHECKU64( vtype.v, 0xc0UL );
  CHECKU64( vlenb, 16UL );

  printf( "\nvsetivli: avl=8, e32, m8, tu, ma\n" );
  VSETIVLI( newvl, 8, e32, m8, tu, ma );
  CHECKU64( newvl, 8UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x8UL );
  CHECKU64( vtype.v, 0x93UL );
  CHECKU64( vlenb, 16UL );

  printf( "\nvsetvl: avl=10, sew=e64, mf4, ta, mu\n" );
  avl = 10;
  reg_vtype_t vt2;
  vt2.f.vlmul = 1;
  vt2.f.vsew  = 3;
  vt2.f.vlmul = 1;
  vt2.f.vta   = 1;
  vt2.f.vma   = 0;
  vt2.f.vii   = 0;
  VSETVL( newvl, avl, vt2 );
  CHECKU64( newvl, 0x4UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x4UL );
  CHECKU64( vtype.v, 0x59UL );
  CHECKU64( vlenb, 16UL );

  // Part 2
  // rd  rs1     AVL        vl
  // -   !x0    x[rs1]    Normal
  // !x0  x0    ~x0       Set vl to VLMAX
  // x0   x0     vl       existing vl (vtype can change)

  printf( "\vsetvl !x0 x0 ...\n" );
  vt2.f.vsew  = 0UL;
  vt2.f.vlmul = 3;
  asm volatile( "vsetvl %0, x0, %1\n\t" : "=r"( newvl ) : "r"( vt2 ) );
  CHECKU64( newvl, 0x80UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x80UL );
  CHECKU64( vtype.v, 0x43UL );
  CHECKU64( vlenb, 16UL );

  // put back to previous value
  avl         = 10;
  vt2.f.vlmul = 1;
  vt2.f.vsew  = 3;
  vt2.f.vta   = 1;
  vt2.f.vma   = 0;
  vt2.f.vii   = 0;
  VSETVL( newvl, avl, vt2 );

  printf( "\vsetvl x0 x0 ...\n" );
  asm volatile( "vsetvl x0, x0, %0\n\t" : : "r"( vt2 ) );
  CHECKU64( newvl, 0x4UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x4UL );
  CHECKU64( vtype.v, 0x59UL );
  CHECKU64( vlenb, 16UL );

  // Part 3: check CSR's after vector add operations
  avl = 32;
  printf( "\nvsetvli: avl=32, e16, m1, ta, ma\n" );
  VSETVLI( newvl, avl, e16, m1, ta, ma );
  CHECKU64( newvl, 0x8UL );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x8UL );
  CHECKU64( vtype.v, 0xc8UL );
  CHECKU64( vlenb, 16UL );
  printf( "\nperforming vector add\n" );
  VL( e16.v, v0, s0 );
  VL( e16.v, v2, s1 );
  VADD(.vv, v4, v0, v2 );
  read_and_dump_registers();
  CHECKU64( vstart.v, 0UL );
  CHECKU64( vxsat, 0UL );
  CHECKU64( vxrm, 0UL );
  CHECKU64( vcsr, 0UL );
  CHECKU64( vl.v, 0x8UL );
  CHECKU64( vtype.v, 0xc8UL );
  CHECKU64( vlenb, 16UL );

  printf( "\ncsr.128.64 completed normally\n" );
}
