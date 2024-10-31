//
// _RevVectorCoProc_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevVectorCoProc.h"

namespace SST::RevCPU {

// ---------------------------------------------------------------
// RevVectorCoProc
// ---------------------------------------------------------------
RevVectorCoProc::RevVectorCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), num_instRetired( 0 ) {

  std::string ClockFreq = params.find<std::string>( "clock", "1Ghz" );
  cycleCount            = 0;

  registerStats();

  //This would be used ot register the clock with SST Core
  /*registerClock( ClockFreq,
    new Clock::Handler<RevVectorCoProc>(this, &RevVectorCoProc::ClockTick));
    output->output("Registering subcomponent RevVectorCoProc with frequency=%s\n", ClockFreq.c_str());*/
}

bool RevVectorCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  RevCoProcInst inst = RevCoProcInst( Inst, F, R, M );
  //std::cout << "Vector CoProc instruction issued: " << std::hex << Inst << std::dec << std::endl;
  //parent->ExternalDepSet(CreatePasskey(), F->GetHartToExecID(), 7, false);
  InstQ.push( inst );
  return true;
}

void RevVectorCoProc::registerStats() {
  num_instRetired = registerStatistic<uint64_t>( "InstRetired" );
}

bool RevVectorCoProc::Reset() {
  InstQ = {};
  return true;
}

bool RevVectorCoProc::ClockTick( SST::Cycle_t cycle ) {
  if( !InstQ.empty() ) {
    RevCoProcInst rec = InstQ.front();
    //parent->ExternalDepClear(CreatePasskey(), InstQ.front().Feature->GetHartToExecID(), 7, false);
    num_instRetired->addData( 1 );
    parent->ExternalStallHart( CreatePasskey(), 0 );
    FauxExec( rec );
    InstQ.pop();
    cycleCount = cycle;
  }
  if( ( cycle - cycleCount ) > 1 ) {
    parent->ExternalReleaseHart( CreatePasskey(), 0 );
  }
  return true;
}

void RevVectorCoProc::FauxExec( RevCoProcInst rec ) {

  // For speedy prototyping, I'm only concerned with these fine folks
  // 0d0572d7    vsetvli t0, a0, e32, m1, ta, ma
  // 0205e007    vle32.v v0, (a1)
  // 02066087    vle32.v v1, (a2)
  // 02008157    vadd.vv v2, v0, v1
  // 0206e127    vse32.v v2, (a3)

  // VLEN=128, ELEN=64
  // 32-bit elements require 4 parallel loads, adds, or stores.
  // MemHierarchy must be disabled.

  std::cout << "Vector CoProcessor to execute instruction: " << std::hex << rec.Inst << std::endl;
  switch( rec.Inst ) {
  case 0x0d0572d7:  // vsetvli t0, a0, e32, m1, ta, ma
    // returns new vl. Always 4 for our test case
    rec.RegFile->SetX( RevReg::t0, 0x4 );
    break;
  case 0x0205e007:  // vle32.v v0, (a1)
  {
    uint64_t a1 = rec.RegFile->GetX<uint64_t>( RevReg::a1 );
    MemReq   req0( a1, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
    rec.Mem->ReadVal<uint64_t>( 0, a1, &vreg[0][0], req0, RevFlag::F_NONE );
    MemReq req1( a1 + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
    rec.Mem->ReadVal<uint64_t>( 0, a1 + 8, &vreg[0][1], req1, RevFlag::F_NONE );
    output->verbose( CALL_INFO, 5, 0, "*V v[0][0] <- 0x%" PRIx64 " v[0][1] <- 0x%" PRIx64 "\n", vreg[0][0], vreg[0][1] );
    break;
  }
  case 0x02066087:  // vle32.v v1, (a2)
  {
    uint64_t a2 = rec.RegFile->GetX<uint64_t>( RevReg::a2 );
    MemReq   req0( a2, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
    rec.Mem->ReadVal<uint64_t>( 0, a2, &vreg[1][0], req0, RevFlag::F_NONE );
    MemReq req1( a2 + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
    rec.Mem->ReadVal<uint64_t>( 0, a2 + 8, &vreg[1][1], req1, RevFlag::F_NONE );
    output->verbose( CALL_INFO, 5, 0, "*V v[1][0] <- 0x%" PRIx64 " v[1][1] <- 0x%" PRIx64 "\n", vreg[1][0], vreg[1][1] );
    break;
  }
  case 0x02008157:  // vadd.vv v2, v0, v1
    // this does 64 bits at a time. Overflow will not occur for this test
    vreg[2][0] = vreg[0][0] + vreg[1][0];
    vreg[2][1] = vreg[0][1] + vreg[1][1];
    output->verbose(
      CALL_INFO,
      5,
      0,
      "*V"
      " 0x%" PRIx64 " <- v[0][0] 0x%" PRIx64 " <- v[0][1]"
      " 0x%" PRIx64 " <- v[1][0] 0x%" PRIx64 " <- v[1][1]"
      " v[2][0] <- 0x%" PRIx64 " v[2][1] <- 0x%" PRIx64 "\n",
      vreg[0][0],
      vreg[0][1],
      vreg[1][0],
      vreg[1][1],
      vreg[2][0],
      vreg[2][1]
    );
    break;
  case 0x0206e127:  // vse32.v v2, (a3)
  {
    uint64_t a3 = rec.RegFile->GetX<uint64_t>( RevReg::a3 );
    rec.Mem->WriteMem( 0, a3, 8, &vreg[2][0] );
    rec.Mem->WriteMem( 0, a3 + 8, 8, &vreg[2][1] );
    output->verbose(
      CALL_INFO,
      5,
      0,
      "*V"
      " 0x%" PRIx64 " <- v[2][0] 0x%" PRIx64 " <- v[2][1]"
      " M[0x%" PRIx64 "] <- 0x%" PRIx64 " M[0x%" PRIx64 "] <- 0x%" PRIx64 "\n",
      vreg[2][0],
      vreg[2][1],
      a3,
      vreg[2][0],
      a3 + 8,
      vreg[2][1]
    );
    break;
  }
  default: output->fatal( CALL_INFO, -1, "faux vector coprocessor cannot digest instruction 0x%" PRIx32 "\n", rec.Inst ); break;
  }
}

}  // namespace SST::RevCPU
