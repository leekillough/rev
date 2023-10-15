//
// _RevCoProc_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevCoProc.h"

namespace SST::RevCPU {

// ---------------------------------------------------------------
// RevCoProc
// ---------------------------------------------------------------
RevCoProc::RevCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : SubComponent( id ), output( nullptr ), parent( parent ) {

  uint32_t verbosity = params.find<uint32_t>( "verbose" );
  output             = new SST::Output( "[RevCoProc @t]: ", verbosity, 0, SST::Output::STDOUT );
}

RevCoProc::~RevCoProc() {
  delete output;
}

// ---------------------------------------------------------------
// RevSimpleCoProc
// ---------------------------------------------------------------
RevSimpleCoProc::RevSimpleCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), num_instRetired( 0 ) {

  std::string ClockFreq = params.find<std::string>( "clock", "1Ghz" );
  cycleCount            = 0;

  registerStats();

  //This would be used ot register the clock with SST Core
  /*registerClock( ClockFreq,
    new Clock::Handler<RevSimpleCoProc>(this, &RevSimpleCoProc::ClockTick));
    output->output("Registering subcomponent RevSimpleCoProc with frequency=%s\n", ClockFreq.c_str());*/
}

bool RevSimpleCoProc::IssueInst( RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  RevCoProcInst inst = RevCoProcInst( Inst, F, R, M );
  std::cout << "CoProc instruction issued: " << std::hex << Inst << std::dec << std::endl;
  //parent->ExternalDepSet(CreatePasskey(), F->GetHartToExecID(), 7, false);
  InstQ.push( inst );
  return true;
}

void RevSimpleCoProc::registerStats() {
  num_instRetired = registerStatistic<uint64_t>( "InstRetired" );
}

bool RevSimpleCoProc::Reset() {
  InstQ = {};
  return true;
}

bool RevSimpleCoProc::ClockTick( SST::Cycle_t cycle ) {
  if( !InstQ.empty() ) {
    uint32_t inst = InstQ.front().Inst;
    //parent->ExternalDepClear(CreatePasskey(), InstQ.front().Feature->GetHartToExecID(), 7, false);
    num_instRetired->addData( 1 );
    parent->ExternalStallHart( CreatePasskey(), 0 );
    InstQ.pop();
    std::cout << "CoProcessor to execute instruction: " << std::hex << inst << std::endl;
    cycleCount = cycle;
  }
  if( ( cycle - cycleCount ) > 500 ) {
    parent->ExternalReleaseHart( CreatePasskey(), 0 );
  }
  return true;
}

// ---------------------------------------------------------------
// RZALSCoproc
// ---------------------------------------------------------------
RZALSCoProc::RZALSCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : RevCoProc(id, params, parent){
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  registerClock( ClockFreq,
                 new Clock::Handler<RZALSCoProc>(this, &RZALSCoProc::ClockTick) );
  output->output("Registering RZALSCoProc with frequency=%s\n",
                 ClockFreq.c_str());
}

RZALSCoProc::~RZALSCoProc(){
}

bool RZALSCoProc::IssueInst(RevFeature *F,
                            RevRegFile *R,
                            RevMem *M,
                            uint32_t Inst){
  return true;
}

bool RZALSCoProc::Reset(){
  return true;
}

bool RZALSCoProc::IsDone(){
  return false;
}

bool RZALSCoProc::ClockTick(SST::Cycle_t cycle){
  return true;
}

// ---------------------------------------------------------------
// RZAAMOCoproc
// ---------------------------------------------------------------
RZAAMOCoProc::RZAAMOCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : RevCoProc(id, params, parent){
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  registerClock( ClockFreq,
                 new Clock::Handler<RZAAMOCoProc>(this, &RZAAMOCoProc::ClockTick) );
  output->output("Registering RZAAMOCoProc with frequency=%s\n",
                 ClockFreq.c_str());
}

RZAAMOCoProc::~RZAAMOCoProc(){
}

bool RZAAMOCoProc::IssueInst(RevFeature *F,
                            RevRegFile *R,
                            RevMem *M,
                            uint32_t Inst){
  return true;
}

bool RZAAMOCoProc::Reset(){
  return true;
}

bool RZAAMOCoProc::IsDone(){
  return false;
}

bool RZAAMOCoProc::ClockTick(SST::Cycle_t cycle){
  return true;
}

}  // namespace SST::RevCPU

// EOF
