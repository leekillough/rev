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

reg_vstart_t vstart;
uint64_t     vxsat = 0xbadbeefUL;
uint64_t     vxrm  = 0xbadbeefUL;
uint64_t     vcsr  = 0xbadbeefUL;
reg_vl_t     vl;
reg_vtype_t  vtype;
uint64_t     vlenb = 0xbadbeefUL;

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

  printf( "init\n" );
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
  printf( "\nsetting vl: avl=32, e8, m1, ta, ma\n" );
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

  // vsetivli rd, uimm5, vtypei # rd=new vl, uimm=AVL, vtypei=new vtype setting
  printf( "\nsetting vl: avl=8, e32, m8, tu, ma\n" );
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

  // vsetvl  rd, rs1, rs2      # rd=new vl, rs1=AVL, rs2=new vtype value
  printf( "\nsetting vl: avl=10, sew=e64, mf4, ta, mu\n" );
  avl = 10;
  reg_vtype_t vt2;
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

  printf( "\nsetvli !x0 x0 ...\n" );
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
  vt2.f.vsew  = 3;
  vt2.f.vlmul = 1;
  vt2.f.vta   = 1;
  vt2.f.vma   = 0;
  vt2.f.vii   = 0;
  VSETVL( newvl, avl, vt2 );

  printf( "\nsetvli x0 x0 ...\n" );
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

  // TODO vxsat, vxrm, vcsr testing

  printf( "\ncsr.128.64 completed normally\n" );
}
