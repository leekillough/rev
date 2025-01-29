//
// _RevCoProc_cc_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
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

bool RevCoProc::sendSuccessResp( zopAPI* zNic, zopEvent* zev, uint16_t SrcHart ) {
  if( !zNic || !zev )
    return false;

  output->verbose(
    CALL_INFO, 9, 0, "[FORZA][RZA][MZOP]: Building MZOP success response for WRITE @ ID=%" PRIu16 "\n", zev->getID()
  );

  // create a new event
  zopEvent* rsp_zev = new zopEvent();

  // set all the fields
  rsp_zev->setType( zopMsgT::Z_RESP );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setOpc( zopOpc::Z_RESP_SACK );
  rsp_zev->setAppID( zev->getAppID() );
  rsp_zev->setResZero( zev->getResZero() );
  rsp_zev->setDestHart( zev->getSrcHart() );
  rsp_zev->setDestZCID( zev->getSrcZCID() );
  rsp_zev->setDestPCID( zev->getSrcPCID() );
  rsp_zev->setDestPrec( zev->getSrcPrec() );
  rsp_zev->setSrcHart( SrcHart );
  rsp_zev->setSrcZCID( (uint8_t) ( zNic->getEndpointType() ) );
  rsp_zev->setSrcPCID( (uint8_t) ( zNic->getPCID( zNic->getZoneID() ) ) );
  rsp_zev->setSrcPrec( (uint8_t) ( zNic->getPrecinctID() ) );
  rsp_zev->setResZero( zev->getResZero() );
  rsp_zev->setMboxID( zev->getMbxID() );

  // no payload
  //if (zev->getOpc() == zopOpc::Z_MZOP_SDMA) {
  //output->verbose(CALL_INFO, 5, 0, "Received SDMA from %s to %s\n", zev->getSrcString().c_str(), zev->getDestString().c_str() );
  //output->verbose(CALL_INFO, 5, 0, "Sending RESP_SACK from %s to %s\n", rsp_zev->getSrcString().c_str(), rsp_zev->getDestString().c_str() );
  //}

  // inject the packet
  zNic->send( rsp_zev, zopCompID( zev->getSrcZCID() ) );

  return true;
}

bool RevCoProc::sendSuccessResp( zopAPI* zNic, zopEvent* zev, uint16_t SrcHart, uint64_t Data ) {
  if( !zNic || !zev )
    return false;

  output->verbose( CALL_INFO, 9, 0, "[FORZA][RZA][]: Building LOAD or HZOP response for MSG @ ID=%" PRIu16 "\n", zev->getID() );

  uint64_t Addr = 0;
  zev->getFLIT( Z_FLIT_ADDR, &Addr );

  // create a new event
  zopEvent* rsp_zev = new zopEvent();

  // set all the fields
  rsp_zev->setType( zopMsgT::Z_RESP );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setOpc( zopOpc::Z_RESP_LR );
  rsp_zev->setAppID( 0 );
  rsp_zev->setDestHart( zev->getSrcHart() );
  rsp_zev->setDestZCID( zev->getSrcZCID() );
  rsp_zev->setDestPCID( zev->getSrcPCID() );
  rsp_zev->setDestPrec( zev->getSrcPrec() );
  rsp_zev->setSrcHart( SrcHart );
  rsp_zev->setSrcZCID( (uint8_t) ( zNic->getEndpointType() ) );
  rsp_zev->setSrcPCID( (uint8_t) ( zNic->getPCID( zNic->getZoneID() ) ) );
  rsp_zev->setSrcPrec( (uint8_t) ( zNic->getPrecinctID() ) );

  // set the payload
  std::vector<uint64_t> payload;
  payload.push_back( Data );  // load response data
  rsp_zev->setPayload( payload );
  rsp_zev->encodeEvent();

  // inject the packet
  zNic->send( rsp_zev, zopCompID( zev->getSrcZCID() ) );

  return true;
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

bool RevSimpleCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
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
RZALSCoProc::RZALSCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), Mem( nullptr ), zNic( nullptr ) {

  std::string ClockFreq = params.find<std::string>( "clock", "1Ghz" );
  registerClock( ClockFreq, new Clock::Handler<RZALSCoProc>( this, &RZALSCoProc::ClockTick ) );
  output->output( "Registering RZALSCoProc with frequency=%s\n", ClockFreq.c_str() );
  MarkLoadCompleteFunc = [=]( const MemReq& req ) { this->MarkLoadComplete( req ); };

  // register the stats
  registerStats();
}

RZALSCoProc::~RZALSCoProc() {
  for( auto& [zev, rs2] : LoadQ )
    delete zev;
}

void RZALSCoProc::registerStats() {
  for( auto* stat : {
         "MZOP_LB",
         "MZOP_LH",
         "MZOP_LW",
         "MZOP_LD",
         "MZOP_LSB",
         "MZOP_LSH",
         "MZOP_LSW",
         "MZOP_LDMA",
         "MZOP_SB",
         "MZOP_SH",
         "MZOP_SW",
         "MZOP_SD",
         "MZOP_SSB",
         "MZOP_SSH",
         "MZOP_SSW",
         "MZOP_SDMA",
       } ) {
    stats.push_back( registerStatistic<uint64_t>( stat ) );
  }
}

void RZALSCoProc::recordStat( mzopStats Stat, uint64_t Data ) {
  if( Stat < mzopStats::MZOP_END ) {
    stats[size_t( Stat )]->addData( Data );
  }
}

bool RZALSCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  return true;
}

bool RZALSCoProc::Reset() {
  return true;
}

bool RZALSCoProc::IsDone() {
  return false;
}

void RZALSCoProc::CheckLSQueue() {
  // Walk the LoadQ and look for hazards that have been
  // cleared in the Proc's LSQueue
  //
  // If a load has been cleared, then prepare a response
  // packet with the appropriate data

  for( auto it = LoadQ.begin(); it != LoadQ.end(); ++it ) {
    auto& [zev, rs2] = *it;

    if( Alloc.getState( rs2 ) == Hazard::DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Z_MZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO, -1, "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%" PRIu16 "\n", zev->getID()
        );
      }
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0;
      if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
        output->fatal( CALL_INFO, -1, "[FORZA][RZA] Erroneous packet contents for ZOP in CheckLSQueue\n" );
      }
      Mem->clearZRqst( Addr );
      delete zev;
      LoadQ.erase( it );
      return;  // this forces us to respond with one response per cycle
    }
  }
}

bool RZALSCoProc::ClockTick( SST::Cycle_t cycle ) {
  CheckLSQueue();
  return true;
}

void RZALSCoProc::MarkLoadComplete( const MemReq& req ) {
  Alloc.setDirty( (uint32_t) ( req.getDestReg() ) );
}

// clang-format off
decltype(RZALSCoProc::mzopTable) RZALSCoProc::mzopTable = {
  // unsigned loads
  { zopOpc::Z_MZOP_LB,   {  mzopKind::load, 1, mzopStats::MZOP_LB,   RevFlag::F_ZEXT64 } },
  { zopOpc::Z_MZOP_LH,   {  mzopKind::load, 2, mzopStats::MZOP_LH,   RevFlag::F_ZEXT64 } },
  { zopOpc::Z_MZOP_LW,   {  mzopKind::load, 4, mzopStats::MZOP_LW,   RevFlag::F_ZEXT64 } },
  { zopOpc::Z_MZOP_LD,   {  mzopKind::load, 8, mzopStats::MZOP_LD,   RevFlag::F_NONE   } },

  // signed loads
  { zopOpc::Z_MZOP_LSB,  {  mzopKind::load, 1, mzopStats::MZOP_LSB,  RevFlag::F_SEXT64 } },
  { zopOpc::Z_MZOP_LSH,  {  mzopKind::load, 2, mzopStats::MZOP_LSH,  RevFlag::F_SEXT64 } },
  { zopOpc::Z_MZOP_LSW,  {  mzopKind::load, 4, mzopStats::MZOP_LSW,  RevFlag::F_SEXT64 } },

  // stores
  { zopOpc::Z_MZOP_SB,   { mzopKind::store, 1, mzopStats::MZOP_SB,   RevFlag::F_NONE   } },
  { zopOpc::Z_MZOP_SH,   { mzopKind::store, 2, mzopStats::MZOP_SH,   RevFlag::F_NONE   } },
  { zopOpc::Z_MZOP_SW,   { mzopKind::store, 4, mzopStats::MZOP_SW,   RevFlag::F_NONE   } },
  { zopOpc::Z_MZOP_SD,   { mzopKind::store, 8, mzopStats::MZOP_SD,   RevFlag::F_NONE   } },

  // ?SIGNED STORES? !!! We should not need these!!! Stores store N bytes regardless of signedness
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  { zopOpc::Z_MZOP_SSB,  { mzopKind::store, 1, mzopStats::MZOP_SSB,  RevFlag::F_NONE   } },
  { zopOpc::Z_MZOP_SSH,  { mzopKind::store, 2, mzopStats::MZOP_SSH,  RevFlag::F_NONE   } },
  { zopOpc::Z_MZOP_SSW,  { mzopKind::store, 4, mzopStats::MZOP_SSW,  RevFlag::F_NONE   } },
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // dma stores
  { zopOpc::Z_MZOP_SDMA, { mzopKind::sdma,  0, mzopStats::MZOP_SDMA, RevFlag::F_NONE   } },
};

// clang-format on

bool RZALSCoProc::handleMZOP( zopEvent* zev, bool& flag ) {
  uint32_t Rs1         = UNDEF_REG;
  uint32_t Rs2         = UNDEF_REG;
  uint64_t Addr        = 0;  // -- Forza::Z_FLIT_ADDR: FLIT 2

  // this is the actual number of data flits
  // these variables are only used for the DMA store operations
  uint32_t RealFlitLen = uint32_t( zev->getLength() );

  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // used only for load operations
  MemReq req{ Addr, uint16_t( Rs2 ), RevRegClass::RegGPR, Z_MZOP_PIPE_HART, MemOp::MemOpREAD, true, MarkLoadCompleteFunc };

  // set the address
  Alloc.SetX( Rs1, Addr );

  // Look up the opcode
  auto opc = zev->getOpc();
  auto it  = mzopTable.find( opc );
  if( it == mzopTable.end() ) {
    output->verbose( CALL_INFO, 9, 0, "[FORZA][RZA][MZOP]: Erroneous MZOP opcode=%" PRIu8 "\n", safe_static_cast<uint8_t>( opc ) );
    return false;
  }

  // Extract the fields
  const auto& [kind, size, stat, flags] = it->second;

  // Execute the code corresponding to the entry
  switch( kind ) {
  case mzopKind::store: {  // Stores
    uint64_t Data = 0;
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    if( !Mem->WriteMem( Z_MZOP_PIPE_HART, Addr, size, &Data ) )
      output->fatal( CALL_INFO, -1, "Error: could not write memory\n" );
    flag = true;
    break;
  }

  case mzopKind::sdma: {  // DMA Stores
    // build a bulk write
    auto   Buf = std::make_unique<uint8_t[]>( RealFlitLen * 8 );
    size_t cur = 0;

    for( uint32_t i = 0; i < RealFlitLen; i++ ) {
      uint64_t Data = 0;
      if( !zev->getFLIT( Z_FLIT_DATA + i, &Data ) ) {
        output->fatal(
          CALL_INFO,
          -1,
          "[FORZA][RZA][MZOP]: MZOP packet has no DMA data FLIT: "
          "Type=%s, ID=%" PRIu16 "\n",
          zNic->msgTToStr( zev->getType() ).c_str(),
          zev->getID()
        );
      }
      for( uint32_t i = 0; i < 8; i++ ) {
        Buf[cur++] = uint8_t( Data & 0xff );
        Data >>= 8;
      }
    }

    // write buffer
    if( !Mem->WriteMem( Z_MZOP_PIPE_HART, Addr, RealFlitLen * 8, Buf.get() ) )
      output->fatal( CALL_INFO, -1, "Error: could not write memory\n" );
    flag = true;
    break;
  }

  case mzopKind::load:  // Loads
    Alloc.SetX( Rs2, 0 );
    Mem->ReadMem( Z_MZOP_PIPE_HART, Addr, size, Alloc.getRegAddr( Rs2 ), req, flags );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    break;
  }

  recordStat( stat );

  if( flag ) {
    // this was a write, signal a success response
    if( !sendSuccessResp( zNic, zev, Z_MZOP_PIPE_HART ) ) {
      output->fatal( CALL_INFO, -1, "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%" PRIu16 "\n", zev->getID() );
    }
    // go ahead and clear the RS2 as we dont need it for WRITE requests
    Alloc.clearReg( Rs2 );
    delete zev;
  }

  // consider this clear the dep on RS1 for all requests
  Alloc.clearReg( Rs1 );

  return true;
}

bool RZALSCoProc::InjectZOP( zopEvent* zev, bool& flag ) {
  if( zev->getType() != zopMsgT::Z_MZOP ) {
    // wrong ZOP type injected
    output->fatal(
      CALL_INFO, -1, "[FORZA][RZA][MZOP]: Cannot handle ZOP message of type: %s\n", zNic->msgTToStr( zev->getType() ).c_str()
    );
  }

  return handleMZOP( zev, flag );
}

// ---------------------------------------------------------------
// RZAAMOCoproc
// ---------------------------------------------------------------
RZAAMOCoProc::RZAAMOCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), Mem( nullptr ), zNic( nullptr ) {
  std::string ClockFreq = params.find<std::string>( "clock", "1Ghz" );
  registerClock( ClockFreq, new Clock::Handler<RZAAMOCoProc>( this, &RZAAMOCoProc::ClockTick ) );
  output->output( "Registering RZAAMOCoProc with frequency=%s\n", ClockFreq.c_str() );
  MarkLoadCompleteFunc = [=]( const MemReq& req ) { this->MarkLoadComplete( req ); };

  // register the stats
  registerStats();
}

RZAAMOCoProc::~RZAAMOCoProc() {
  for( auto& [zev, rs1, rs2] : AMOQ )
    delete zev;
}

void RZAAMOCoProc::registerStats() {
  for( auto* stat : {
         "HZOP_8_BASE_ADD",    "HZOP_8_BASE_AND",    "HZOP_8_BASE_OR",      "HZOP_8_BASE_XOR",     "HZOP_8_BASE_SMAX",
         "HZOP_8_BASE_MAX",    "HZOP_8_BASE_SMIN",   "HZOP_8_BASE_MIN",     "HZOP_8_BASE_SWAP",    "HZOP_8_BASE_CAS",
         "HZOP_8_BASE_THRESH", "HZOP_8_BASE_FADD",   "HZOP_8_BASE_FSUB",    "HZOP_8_BASE_FRSUB",   "HZOP_16_BASE_ADD",
         "HZOP_16_BASE_AND",   "HZOP_16_BASE_OR",    "HZOP_16_BASE_XOR",    "HZOP_16_BASE_SMAX",   "HZOP_16_BASE_MAX",
         "HZOP_16_BASE_SMIN",  "HZOP_16_BASE_MIN",   "HZOP_16_BASE_SWAP",   "HZOP_16_BASE_CAS",    "HZOP_16_BASE_THRESH",
         "HZOP_16_BASE_FADD",  "HZOP_16_BASE_FSUB",  "HZOP_16_BASE_FRSUB",  "HZOP_32_BASE_ADD",    "HZOP_32_BASE_AND",
         "HZOP_32_BASE_OR",    "HZOP_32_BASE_XOR",   "HZOP_32_BASE_SMAX",   "HZOP_32_BASE_MAX",    "HZOP_32_BASE_SMIN",
         "HZOP_32_BASE_MIN",   "HZOP_32_BASE_SWAP",  "HZOP_32_BASE_CAS",    "HZOP_32_BASE_THRESH", "HZOP_32_BASE_FADD",
         "HZOP_32_BASE_FSUB",  "HZOP_32_BASE_FRSUB", "HZOP_64_BASE_ADD",    "HZOP_64_BASE_AND",    "HZOP_64_BASE_OR",
         "HZOP_64_BASE_XOR",   "HZOP_64_BASE_SMAX",  "HZOP_64_BASE_MAX",    "HZOP_64_BASE_SMIN",   "HZOP_64_BASE_MIN",
         "HZOP_64_BASE_SWAP",  "HZOP_64_BASE_CAS",   "HZOP_64_BASE_THRESH", "HZOP_64_BASE_FADD",   "HZOP_64_BASE_FSUB",
         "HZOP_64_BASE_FRSUB", "HZOP_8_M_ADD",       "HZOP_8_M_AND",        "HZOP_8_M_OR",         "HZOP_8_M_XOR",
         "HZOP_8_M_SMAX",      "HZOP_8_M_MAX",       "HZOP_8_M_SMIN",       "HZOP_8_M_MIN",        "HZOP_8_M_SWAP",
         "HZOP_8_M_CAS",       "HZOP_8_M_THRESH",    "HZOP_8_M_FADD",       "HZOP_8_M_FSUB",       "HZOP_8_M_FRSUB",
         "HZOP_16_M_ADD",      "HZOP_16_M_AND",      "HZOP_16_M_OR",        "HZOP_16_M_XOR",       "HZOP_16_M_SMAX",
         "HZOP_16_M_MAX",      "HZOP_16_M_SMIN",     "HZOP_16_M_MIN",       "HZOP_16_M_SWAP",      "HZOP_16_M_CAS",
         "HZOP_16_M_THRESH",   "HZOP_16_M_FADD",     "HZOP_16_M_FSUB",      "HZOP_16_M_FRSUB",     "HZOP_32_M_ADD",
         "HZOP_32_M_AND",      "HZOP_32_M_OR",       "HZOP_32_M_XOR",       "HZOP_32_M_SMAX",      "HZOP_32_M_MAX",
         "HZOP_32_M_SMIN",     "HZOP_32_M_MIN",      "HZOP_32_M_SWAP",      "HZOP_32_M_CAS",       "HZOP_32_M_THRESH",
         "HZOP_32_M_FADD",     "HZOP_32_M_FSUB",     "HZOP_32_M_FRSUB",     "HZOP_64_M_ADD",       "HZOP_64_M_AND",
         "HZOP_64_M_OR",       "HZOP_64_M_XOR",      "HZOP_64_M_SMAX",      "HZOP_64_M_MAX",       "HZOP_64_M_SMIN",
         "HZOP_64_M_MIN",      "HZOP_64_M_SWAP",     "HZOP_64_M_CAS",       "HZOP_64_M_THRESH",    "HZOP_64_M_FADD",
         "HZOP_64_M_FSUB",     "HZOP_64_M_FRSUB",    "HZOP_8_S_ADD",        "HZOP_8_S_AND",        "HZOP_8_S_OR",
         "HZOP_8_S_XOR",       "HZOP_8_S_SMAX",      "HZOP_8_S_MAX",        "HZOP_8_S_SMIN",       "HZOP_8_S_MIN",
         "HZOP_8_S_SWAP",      "HZOP_8_S_CAS",       "HZOP_8_S_FADD",       "HZOP_8_S_FSUB",       "HZOP_8_S_FRSUB",
         "HZOP_16_S_ADD",      "HZOP_16_S_AND",      "HZOP_16_S_OR",        "HZOP_16_S_XOR",       "HZOP_16_S_SMAX",
         "HZOP_16_S_MAX",      "HZOP_16_S_SMIN",     "HZOP_16_S_MIN",       "HZOP_16_S_SWAP",      "HZOP_16_S_CAS",
         "HZOP_16_S_THRESH",   "HZOP_16_S_FADD",     "HZOP_16_S_FSUB",      "HZOP_16_S_FRSUB",     "HZOP_32_S_ADD",
         "HZOP_32_S_AND",      "HZOP_32_S_OR",       "HZOP_32_S_XOR",       "HZOP_32_S_SMAX",      "HZOP_32_S_MAX",
         "HZOP_32_S_SMIN",     "HZOP_32_S_MIN",      "HZOP_32_S_SWAP",      "HZOP_32_S_CAS",       "HZOP_32_S_THRESH",
         "HZOP_32_S_FADD",     "HZOP_32_S_FSUB",     "HZOP_32_S_FRSUB",     "HZOP_64_S_ADD",       "HZOP_64_S_AND",
         "HZOP_64_S_OR",       "HZOP_64_S_XOR",      "HZOP_64_S_SMAX",      "HZOP_64_S_MAX",       "HZOP_64_S_SMIN",
         "HZOP_64_S_MIN",      "HZOP_64_S_SWAP",     "HZOP_64_S_CAS",       "HZOP_64_S_THRESH",    "HZOP_64_S_FADD",
         "HZOP_64_S_FSUB",     "HZOP_64_S_FRSUB",    "HZOP_8_MS_ADD",       "HZOP_8_MS_AND",       "HZOP_8_MS_OR",
         "HZOP_8_MS_XOR",      "HZOP_8_MS_SMAX",     "HZOP_8_MS_MAX",       "HZOP_8_MS_SMIN",      "HZOP_8_MS_MIN",
         "HZOP_8_MS_SWAP",     "HZOP_8_MS_CAS",      "HZOP_8_MS_THRESH",    "HZOP_8_MS_FADD",      "HZOP_8_MS_FSUB",
         "HZOP_8_MS_FRSUB",    "HZOP_16_MS_ADD",     "HZOP_16_MS_AND",      "HZOP_16_MS_OR",       "HZOP_16_MS_XOR",
         "HZOP_16_MS_SMAX",    "HZOP_16_MS_MAX",     "HZOP_16_MS_SMIN",     "HZOP_16_MS_MIN",      "HZOP_16_MS_SWAP",
         "HZOP_16_MS_CAS",     "HZOP_16_MS_THRESH",  "HZOP_16_MS_FADD",     "HZOP_16_MS_FSUB",     "HZOP_16_MS_FRSUB",
         "HZOP_32_MS_ADD",     "HZOP_32_MS_AND",     "HZOP_32_MS_OR",       "HZOP_32_MS_XOR",      "HZOP_32_MS_SMAX",
         "HZOP_32_MS_MAX",     "HZOP_32_MS_SMIN",    "HZOP_32_MS_MIN",      "HZOP_32_MS_SWAP",     "HZOP_32_MS_CAS",
         "HZOP_32_MS_THRESH",  "HZOP_32_MS_FADD",    "HZOP_32_MS_FSUB",     "HZOP_32_MS_FRSUB",    "HZOP_64_MS_ADD",
         "HZOP_64_MS_AND",     "HZOP_64_MS_OR",      "HZOP_64_MS_XOR",      "HZOP_64_MS_SMAX",     "HZOP_64_MS_MAX",
         "HZOP_64_MS_SMIN",    "HZOP_64_MS_MIN",     "HZOP_64_MS_SWAP",     "HZOP_64_MS_CAS",      "HZOP_64_MS_THRESH",
         "HZOP_64_MS_FADD",    "HZOP_64_MS_FSUB",    "HZOP_64_MS_FRSUB",
       } ) {
    stats.push_back( registerStatistic<uint64_t>( stat ) );
  }
}

void RZAAMOCoProc::recordStat( hzopStats Stat, uint64_t Data ) {
  if( Stat < hzopStats::HZOP_END ) {
    stats[size_t( Stat )]->addData( Data );
  }
}

bool RZAAMOCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  return true;
}

bool RZAAMOCoProc::Reset() {
  return true;
}

bool RZAAMOCoProc::IsDone() {
  return false;
}

bool RZAAMOCoProc::ClockTick( SST::Cycle_t cycle ) {
  CheckLSQueue();
  return true;
}

// clang-format off
decltype(RZAAMOCoProc::zopAMOTable) RZAAMOCoProc::zopAMOTable = {
  // 8bit base
  { zopOpc::Z_HAC_8_BASE_ADD,     { 1, hzopStats::HZOP_8_BASE_ADD,     RevFlag::F_AMOADD,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_AND,     { 1, hzopStats::HZOP_8_BASE_AND,     RevFlag::F_AMOAND,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_OR,      { 1, hzopStats::HZOP_8_BASE_OR,      RevFlag::F_AMOOR,    RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_XOR,     { 1, hzopStats::HZOP_8_BASE_XOR,     RevFlag::F_AMOXOR,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_SMAX,    { 1, hzopStats::HZOP_8_BASE_SMAX,    RevFlag::F_AMOMAX,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_MAX,     { 1, hzopStats::HZOP_8_BASE_MAX,     RevFlag::F_AMOMAXU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_SMIN,    { 1, hzopStats::HZOP_8_BASE_SMIN,    RevFlag::F_AMOMIN,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_MIN,     { 1, hzopStats::HZOP_8_BASE_MIN,     RevFlag::F_AMOMINU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_SWAP,    { 1, hzopStats::HZOP_8_BASE_SWAP,    RevFlag::F_AMOSWAP,  RevFlag::F_NONE  } },
//{ zopOpc::Z_HAC_8_BASE_CAS,     { 1, hzopStats::HZOP_8_BASE_CAS,     RevFlag::F_AMOCAS,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_THRESH,  { 1, hzopStats::HZOP_8_BASE_THRESH,  RevFlag::F_AMOTHRES, RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_FADD,    { 1, hzopStats::HZOP_8_BASE_FADD,    RevFlag::F_AMOFADD,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_FSUB,    { 1, hzopStats::HZOP_8_BASE_FSUB,    RevFlag::F_AMOFSUB,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_8_BASE_FRSUB,   { 1, hzopStats::HZOP_8_BASE_FRSUB,   RevFlag::F_AMOFSUBR, RevFlag::F_NONE  } },

  // 16bit base
  { zopOpc::Z_HAC_16_BASE_ADD,    { 2, hzopStats::HZOP_16_BASE_ADD,    RevFlag::F_AMOADD,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_AND,    { 2, hzopStats::HZOP_16_BASE_AND,    RevFlag::F_AMOAND,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_OR,     { 2, hzopStats::HZOP_16_BASE_OR,     RevFlag::F_AMOOR,    RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_XOR,    { 2, hzopStats::HZOP_16_BASE_XOR,    RevFlag::F_AMOXOR,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_SMAX,   { 2, hzopStats::HZOP_16_BASE_SMAX,   RevFlag::F_AMOMAX,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_MAX,    { 2, hzopStats::HZOP_16_BASE_MAX,    RevFlag::F_AMOMAXU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_SMIN,   { 2, hzopStats::HZOP_16_BASE_SMIN,   RevFlag::F_AMOMIN,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_MIN,    { 2, hzopStats::HZOP_16_BASE_MIN,    RevFlag::F_AMOMINU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_SWAP,   { 2, hzopStats::HZOP_16_BASE_SWAP,   RevFlag::F_AMOSWAP,  RevFlag::F_NONE  } },
//{ zopOpc::Z_HAC_16_BASE_CAS,    { 2, hzopStats::HZOP_16_BASE_CAS,    RevFlag::F_AMOCAS,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_THRESH, { 2, hzopStats::HZOP_16_BASE_THRESH, RevFlag::F_AMOTHRES, RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_FADD,   { 2, hzopStats::HZOP_16_BASE_FADD,   RevFlag::F_AMOFADD,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_FSUB,   { 2, hzopStats::HZOP_16_BASE_FSUB,   RevFlag::F_AMOFSUB,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_16_BASE_FRSUB,  { 2, hzopStats::HZOP_16_BASE_FRSUB,  RevFlag::F_AMOFSUBR, RevFlag::F_NONE  } },

  // 32bit base
  { zopOpc::Z_HAC_32_BASE_ADD,    { 4, hzopStats::HZOP_32_BASE_ADD,    RevFlag::F_AMOADD,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_AND,    { 4, hzopStats::HZOP_32_BASE_AND,    RevFlag::F_AMOAND,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_OR,     { 4, hzopStats::HZOP_32_BASE_OR,     RevFlag::F_AMOOR,    RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_XOR,    { 4, hzopStats::HZOP_32_BASE_XOR,    RevFlag::F_AMOXOR,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_SMAX,   { 4, hzopStats::HZOP_32_BASE_SMAX,   RevFlag::F_AMOMAX,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_MAX,    { 4, hzopStats::HZOP_32_BASE_MAX,    RevFlag::F_AMOMAXU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_SMIN,   { 4, hzopStats::HZOP_32_BASE_SMIN,   RevFlag::F_AMOMIN,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_MIN,    { 4, hzopStats::HZOP_32_BASE_MIN,    RevFlag::F_AMOMINU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_SWAP,   { 4, hzopStats::HZOP_32_BASE_SWAP,   RevFlag::F_AMOSWAP,  RevFlag::F_NONE  } },
//{ zopOpc::Z_HAC_32_BASE_CAS,    { 4, hzopStats::HZOP_32_BASE_CAS,    RevFlag::F_AMOCAS,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_THRESH, { 4, hzopStats::HZOP_32_BASE_THRESH, RevFlag::F_AMOTHRES, RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_FADD,   { 4, hzopStats::HZOP_32_BASE_FADD,   RevFlag::F_AMOFADD,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_FSUB,   { 4, hzopStats::HZOP_32_BASE_FSUB,   RevFlag::F_AMOFSUB,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_32_BASE_FRSUB,  { 4, hzopStats::HZOP_32_BASE_FRSUB,  RevFlag::F_AMOFSUBR, RevFlag::F_NONE  } },

  // 64bit base
  { zopOpc::Z_HAC_64_BASE_ADD,    { 8, hzopStats::HZOP_64_BASE_ADD,    RevFlag::F_AMOADD,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_AND,    { 8, hzopStats::HZOP_64_BASE_AND,    RevFlag::F_AMOAND,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_OR,     { 8, hzopStats::HZOP_64_BASE_OR,     RevFlag::F_AMOOR,    RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_XOR,    { 8, hzopStats::HZOP_64_BASE_XOR,    RevFlag::F_AMOXOR,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_SMAX,   { 8, hzopStats::HZOP_64_BASE_SMAX,   RevFlag::F_AMOMAX,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_MAX,    { 8, hzopStats::HZOP_64_BASE_MAX,    RevFlag::F_AMOMAXU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_SMIN,   { 8, hzopStats::HZOP_64_BASE_SMIN,   RevFlag::F_AMOMIN,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_MIN,    { 8, hzopStats::HZOP_64_BASE_MIN,    RevFlag::F_AMOMINU,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_SWAP,   { 8, hzopStats::HZOP_64_BASE_SWAP,   RevFlag::F_AMOSWAP,  RevFlag::F_NONE  } },
//{ zopOpc::Z_HAC_64_BASE_CAS,    { 8, hzopStats::HZOP_64_BASE_CAS,    RevFlag::F_AMOCAS,   RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_THRESH, { 8, hzopStats::HZOP_64_BASE_THRESH, RevFlag::F_AMOTHRES, RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_FADD,   { 8, hzopStats::HZOP_64_BASE_FADD,   RevFlag::F_AMOFADD,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_FSUB,   { 8, hzopStats::HZOP_64_BASE_FSUB,   RevFlag::F_AMOFSUB,  RevFlag::F_NONE  } },
  { zopOpc::Z_HAC_64_BASE_FRSUB,  { 8, hzopStats::HZOP_64_BASE_FRSUB,  RevFlag::F_AMOFSUBR, RevFlag::F_NONE  } },

  // 8bit M
  { zopOpc::Z_HAC_8_M_ADD,        { 1, hzopStats::HZOP_8_M_ADD,        RevFlag::F_AMOADD,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_AND,        { 1, hzopStats::HZOP_8_M_AND,        RevFlag::F_AMOAND,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_OR,         { 1, hzopStats::HZOP_8_M_OR,         RevFlag::F_AMOOR,    RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_XOR,        { 1, hzopStats::HZOP_8_M_XOR,        RevFlag::F_AMOXOR,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_SMAX,       { 1, hzopStats::HZOP_8_M_SMAX,       RevFlag::F_AMOMAX,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_MAX,        { 1, hzopStats::HZOP_8_M_MAX,        RevFlag::F_AMOMAXU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_SMIN,       { 1, hzopStats::HZOP_8_M_SMIN,       RevFlag::F_AMOMIN,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_MIN,        { 1, hzopStats::HZOP_8_M_MIN,        RevFlag::F_AMOMINU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_SWAP,       { 1, hzopStats::HZOP_8_M_SWAP,       RevFlag::F_AMOSWAP,  RevFlag::F_AMONN } },
//{ zopOpc::Z_HAC_8_M_CAS,        { 1, hzopStats::HZOP_8_M_CAS,        RevFlag::F_AMOCAS,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_THRESH,     { 1, hzopStats::HZOP_8_M_THRESH,     RevFlag::F_AMOTHRES, RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_FADD,       { 1, hzopStats::HZOP_8_M_FADD,       RevFlag::F_AMOFADD,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_FSUB,       { 1, hzopStats::HZOP_8_M_FSUB,       RevFlag::F_AMOFSUB,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_8_M_FRSUB,      { 1, hzopStats::HZOP_8_M_FRSUB,      RevFlag::F_AMOFSUBR, RevFlag::F_AMONN } },

  // 16bit M
  { zopOpc::Z_HAC_16_M_ADD,       { 2, hzopStats::HZOP_16_M_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_AND,       { 2, hzopStats::HZOP_16_M_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_OR,        { 2, hzopStats::HZOP_16_M_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_XOR,       { 2, hzopStats::HZOP_16_M_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_SMAX,      { 2, hzopStats::HZOP_16_M_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_MAX,       { 2, hzopStats::HZOP_16_M_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_SMIN,      { 2, hzopStats::HZOP_16_M_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_MIN,       { 2, hzopStats::HZOP_16_M_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_SWAP,      { 2, hzopStats::HZOP_16_M_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMONN } },
//{ zopOpc::Z_HAC_16_M_CAS,       { 2, hzopStats::HZOP_16_M_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_THRESH,    { 2, hzopStats::HZOP_16_M_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_FADD,      { 2, hzopStats::HZOP_16_M_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_FSUB,      { 2, hzopStats::HZOP_16_M_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_16_M_FRSUB,     { 2, hzopStats::HZOP_16_M_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMONN } },

  // 32bit M
  { zopOpc::Z_HAC_32_M_ADD,       { 4, hzopStats::HZOP_32_M_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_AND,       { 4, hzopStats::HZOP_32_M_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_OR,        { 4, hzopStats::HZOP_32_M_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_XOR,       { 4, hzopStats::HZOP_32_M_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_SMAX,      { 4, hzopStats::HZOP_32_M_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_MAX,       { 4, hzopStats::HZOP_32_M_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_SMIN,      { 4, hzopStats::HZOP_32_M_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_MIN,       { 4, hzopStats::HZOP_32_M_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_SWAP,      { 4, hzopStats::HZOP_32_M_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMONN } },
//{ zopOpc::Z_HAC_32_M_CAS,       { 4, hzopStats::HZOP_32_M_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_THRESH,    { 4, hzopStats::HZOP_32_M_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_FADD,      { 4, hzopStats::HZOP_32_M_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_FSUB,      { 4, hzopStats::HZOP_32_M_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_32_M_FRSUB,     { 4, hzopStats::HZOP_32_M_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMONN } },

  // 64bit M
  { zopOpc::Z_HAC_64_M_ADD,       { 8, hzopStats::HZOP_64_M_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_AND,       { 8, hzopStats::HZOP_64_M_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_OR,        { 8, hzopStats::HZOP_64_M_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_XOR,       { 8, hzopStats::HZOP_64_M_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_SMAX,      { 8, hzopStats::HZOP_64_M_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_MAX,       { 8, hzopStats::HZOP_64_M_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_SMIN,      { 8, hzopStats::HZOP_64_M_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_MIN,       { 8, hzopStats::HZOP_64_M_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_SWAP,      { 8, hzopStats::HZOP_64_M_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMONN } },
//{ zopOpc::Z_HAC_64_M_CAS,       { 8, hzopStats::HZOP_64_M_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_THRESH,    { 8, hzopStats::HZOP_64_M_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_FADD,      { 8, hzopStats::HZOP_64_M_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_FSUB,      { 8, hzopStats::HZOP_64_M_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMONN } },
  { zopOpc::Z_HAC_64_M_FRSUB,     { 8, hzopStats::HZOP_64_M_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMONN } },

  // 8bit S
  { zopOpc::Z_HAC_8_S_ADD,        { 1, hzopStats::HZOP_8_S_ADD,        RevFlag::F_AMOADD,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_AND,        { 1, hzopStats::HZOP_8_S_AND,        RevFlag::F_AMOAND,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_OR,         { 1, hzopStats::HZOP_8_S_OR,         RevFlag::F_AMOOR,    RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_XOR,        { 1, hzopStats::HZOP_8_S_XOR,        RevFlag::F_AMOXOR,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_SMAX,       { 1, hzopStats::HZOP_8_S_SMAX,       RevFlag::F_AMOMAX,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_MAX,        { 1, hzopStats::HZOP_8_S_MAX,        RevFlag::F_AMOMAXU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_SMIN,       { 1, hzopStats::HZOP_8_S_SMIN,       RevFlag::F_AMOMIN,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_MIN,        { 1, hzopStats::HZOP_8_S_MIN,        RevFlag::F_AMOMINU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_SWAP,       { 1, hzopStats::HZOP_8_S_SWAP,       RevFlag::F_AMOSWAP,  RevFlag::F_AMOON } },
//{ zopOpc::Z_HAC_8_S_CAS,        { 1, hzopStats::HZOP_8_S_CAS,        RevFlag::F_AMOCAS,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_THRESH,     { 1, hzopStats::HZOP_8_S_THRESH,     RevFlag::F_AMOTHRES, RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_FADD,       { 1, hzopStats::HZOP_8_S_FADD,       RevFlag::F_AMOFADD,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_FSUB,       { 1, hzopStats::HZOP_8_S_FSUB,       RevFlag::F_AMOFSUB,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_8_S_FRSUB,      { 1, hzopStats::HZOP_8_S_FRSUB,      RevFlag::F_AMOFSUBR, RevFlag::F_AMOON } },

  // 16bit S
  { zopOpc::Z_HAC_16_S_ADD,       { 2, hzopStats::HZOP_16_S_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_AND,       { 2, hzopStats::HZOP_16_S_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_OR,        { 2, hzopStats::HZOP_16_S_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_XOR,       { 2, hzopStats::HZOP_16_S_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_SMAX,      { 2, hzopStats::HZOP_16_S_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_MAX,       { 2, hzopStats::HZOP_16_S_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_SMIN,      { 2, hzopStats::HZOP_16_S_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_MIN,       { 2, hzopStats::HZOP_16_S_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_SWAP,      { 2, hzopStats::HZOP_16_S_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMOON } },
//{ zopOpc::Z_HAC_16_S_CAS,       { 2, hzopStats::HZOP_16_S_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_THRESH,    { 2, hzopStats::HZOP_16_S_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_FADD,      { 2, hzopStats::HZOP_16_S_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_FSUB,      { 2, hzopStats::HZOP_16_S_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_16_S_FRSUB,     { 2, hzopStats::HZOP_16_S_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMOON } },

  // 32bit S
  { zopOpc::Z_HAC_32_S_ADD,       { 4, hzopStats::HZOP_32_S_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_AND,       { 4, hzopStats::HZOP_32_S_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_OR,        { 4, hzopStats::HZOP_32_S_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_XOR,       { 4, hzopStats::HZOP_32_S_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_SMAX,      { 4, hzopStats::HZOP_32_S_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_MAX,       { 4, hzopStats::HZOP_32_S_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_SMIN,      { 4, hzopStats::HZOP_32_S_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_MIN,       { 4, hzopStats::HZOP_32_S_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_SWAP,      { 4, hzopStats::HZOP_32_S_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMOON } },
//{ zopOpc::Z_HAC_32_S_CAS,       { 4, hzopStats::HZOP_32_S_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_THRESH,    { 4, hzopStats::HZOP_32_S_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_FADD,      { 4, hzopStats::HZOP_32_S_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_FSUB,      { 4, hzopStats::HZOP_32_S_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_32_S_FRSUB,     { 4, hzopStats::HZOP_32_S_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMOON } },

  // 64bit S
  { zopOpc::Z_HAC_64_S_ADD,       { 8, hzopStats::HZOP_64_S_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_AND,       { 8, hzopStats::HZOP_64_S_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_OR,        { 8, hzopStats::HZOP_64_S_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_XOR,       { 8, hzopStats::HZOP_64_S_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_SMAX,      { 8, hzopStats::HZOP_64_S_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_MAX,       { 8, hzopStats::HZOP_64_S_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_SMIN,      { 8, hzopStats::HZOP_64_S_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_MIN,       { 8, hzopStats::HZOP_64_S_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_SWAP,      { 8, hzopStats::HZOP_64_S_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMOON } },
//{ zopOpc::Z_HAC_64_S_CAS,       { 8, hzopStats::HZOP_64_S_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_THRESH,    { 8, hzopStats::HZOP_64_S_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_FADD,      { 8, hzopStats::HZOP_64_S_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_FSUB,      { 8, hzopStats::HZOP_64_S_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMOON } },
  { zopOpc::Z_HAC_64_S_FRSUB,     { 8, hzopStats::HZOP_64_S_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMOON } },

  // 8bit MS
  { zopOpc::Z_HAC_8_MS_ADD,       { 1, hzopStats::HZOP_8_MS_ADD,       RevFlag::F_AMOADD,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_AND,       { 1, hzopStats::HZOP_8_MS_AND,       RevFlag::F_AMOAND,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_OR,        { 1, hzopStats::HZOP_8_MS_OR,        RevFlag::F_AMOOR,    RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_XOR,       { 1, hzopStats::HZOP_8_MS_XOR,       RevFlag::F_AMOXOR,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_SMAX,      { 1, hzopStats::HZOP_8_MS_SMAX,      RevFlag::F_AMOMAX,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_MAX,       { 1, hzopStats::HZOP_8_MS_MAX,       RevFlag::F_AMOMAXU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_SMIN,      { 1, hzopStats::HZOP_8_MS_SMIN,      RevFlag::F_AMOMIN,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_MIN,       { 1, hzopStats::HZOP_8_MS_MIN,       RevFlag::F_AMOMINU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_SWAP,      { 1, hzopStats::HZOP_8_MS_SWAP,      RevFlag::F_AMOSWAP,  RevFlag::F_AMONO } },
//{ zopOpc::Z_HAC_8_MS_CAS,       { 1, hzopStats::HZOP_8_MS_CAS,       RevFlag::F_AMOCAS,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_THRESH,    { 1, hzopStats::HZOP_8_MS_THRESH,    RevFlag::F_AMOTHRES, RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_FADD,      { 1, hzopStats::HZOP_8_MS_FADD,      RevFlag::F_AMOFADD,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_FSUB,      { 1, hzopStats::HZOP_8_MS_FSUB,      RevFlag::F_AMOFSUB,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_8_MS_FRSUB,     { 1, hzopStats::HZOP_8_MS_FRSUB,     RevFlag::F_AMOFSUBR, RevFlag::F_AMONO } },

  // 16bit MS
  { zopOpc::Z_HAC_16_MS_ADD,      { 2, hzopStats::HZOP_16_MS_ADD,      RevFlag::F_AMOADD,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_AND,      { 2, hzopStats::HZOP_16_MS_AND,      RevFlag::F_AMOAND,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_OR,       { 2, hzopStats::HZOP_16_MS_OR,       RevFlag::F_AMOOR,    RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_XOR,      { 2, hzopStats::HZOP_16_MS_XOR,      RevFlag::F_AMOXOR,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_SMAX,     { 2, hzopStats::HZOP_16_MS_SMAX,     RevFlag::F_AMOMAX,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_MAX,      { 2, hzopStats::HZOP_16_MS_MAX,      RevFlag::F_AMOMAXU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_SMIN,     { 2, hzopStats::HZOP_16_MS_SMIN,     RevFlag::F_AMOMIN,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_MIN,      { 2, hzopStats::HZOP_16_MS_MIN,      RevFlag::F_AMOMINU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_SWAP,     { 2, hzopStats::HZOP_16_MS_SWAP,     RevFlag::F_AMOSWAP,  RevFlag::F_AMONO } },
//{ zopOpc::Z_HAC_16_MS_CAS,      { 2, hzopStats::HZOP_16_MS_CAS,      RevFlag::F_AMOCAS,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_THRESH,   { 2, hzopStats::HZOP_16_MS_THRESH,   RevFlag::F_AMOTHRES, RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_FADD,     { 2, hzopStats::HZOP_16_MS_FADD,     RevFlag::F_AMOFADD,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_FSUB,     { 2, hzopStats::HZOP_16_MS_FSUB,     RevFlag::F_AMOFSUB,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_16_MS_FRSUB,    { 2, hzopStats::HZOP_16_MS_FRSUB,    RevFlag::F_AMOFSUBR, RevFlag::F_AMONO } },

  // 32bit MS
  { zopOpc::Z_HAC_32_MS_ADD,      { 4, hzopStats::HZOP_32_MS_ADD,      RevFlag::F_AMOADD,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_AND,      { 4, hzopStats::HZOP_32_MS_AND,      RevFlag::F_AMOAND,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_OR,       { 4, hzopStats::HZOP_32_MS_OR,       RevFlag::F_AMOOR,    RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_XOR,      { 4, hzopStats::HZOP_32_MS_XOR,      RevFlag::F_AMOXOR,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_SMAX,     { 4, hzopStats::HZOP_32_MS_SMAX,     RevFlag::F_AMOMAX,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_MAX,      { 4, hzopStats::HZOP_32_MS_MAX,      RevFlag::F_AMOMAXU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_SMIN,     { 4, hzopStats::HZOP_32_MS_SMIN,     RevFlag::F_AMOMIN,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_MIN,      { 4, hzopStats::HZOP_32_MS_MIN,      RevFlag::F_AMOMINU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_SWAP,     { 4, hzopStats::HZOP_32_MS_SWAP,     RevFlag::F_AMOSWAP,  RevFlag::F_AMONO } },
//{ zopOpc::Z_HAC_32_MS_CAS,      { 4, hzopStats::HZOP_32_MS_CAS,      RevFlag::F_AMOCAS,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_THRESH,   { 4, hzopStats::HZOP_32_MS_THRESH,   RevFlag::F_AMOTHRES, RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_FADD,     { 4, hzopStats::HZOP_32_MS_FADD,     RevFlag::F_AMOFADD,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_FSUB,     { 4, hzopStats::HZOP_32_MS_FSUB,     RevFlag::F_AMOFSUB,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_32_MS_FRSUB,    { 4, hzopStats::HZOP_32_MS_FRSUB,    RevFlag::F_AMOFSUBR, RevFlag::F_AMONO } },

  // 64bit MS
  { zopOpc::Z_HAC_64_MS_ADD,      { 8, hzopStats::HZOP_64_MS_ADD,      RevFlag::F_AMOADD,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_AND,      { 8, hzopStats::HZOP_64_MS_AND,      RevFlag::F_AMOAND,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_OR,       { 8, hzopStats::HZOP_64_MS_OR,       RevFlag::F_AMOOR,    RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_XOR,      { 8, hzopStats::HZOP_64_MS_XOR,      RevFlag::F_AMOXOR,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_SMAX,     { 8, hzopStats::HZOP_64_MS_SMAX,     RevFlag::F_AMOMAX,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_MAX,      { 8, hzopStats::HZOP_64_MS_MAX,      RevFlag::F_AMOMAXU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_SMIN,     { 8, hzopStats::HZOP_64_MS_SMIN,     RevFlag::F_AMOMIN,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_MIN,      { 8, hzopStats::HZOP_64_MS_MIN,      RevFlag::F_AMOMINU,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_SWAP,     { 8, hzopStats::HZOP_64_MS_SWAP,     RevFlag::F_AMOSWAP,  RevFlag::F_AMONO } },
//{ zopOpc::Z_HAC_64_MS_CAS,      { 8, hzopStats::HZOP_64_MS_CAS,      RevFlag::F_AMOCAS,   RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_THRESH,   { 8, hzopStats::HZOP_64_MS_THRESH,   RevFlag::F_AMOTHRES, RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_FADD,     { 8, hzopStats::HZOP_64_MS_FADD,     RevFlag::F_AMOFADD,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_FSUB,     { 8, hzopStats::HZOP_64_MS_FSUB,     RevFlag::F_AMOFSUB,  RevFlag::F_AMONO } },
  { zopOpc::Z_HAC_64_MS_FRSUB,    { 8, hzopStats::HZOP_64_MS_FRSUB,    RevFlag::F_AMOFSUBR, RevFlag::F_AMONO } },
};

// clang-format on

bool RZAAMOCoProc::handleHZOP( zopEvent* zev, bool& flag ) {
  flag          = false;  // these are handled as READ requests; eg they hazard
  uint32_t Rs1  = UNDEF_REG;
  uint32_t Rs2  = UNDEF_REG;
  uint64_t Addr = 0;  // -- Z_FLIT_ADDR: FLIT 2
  uint64_t Data = 0;  // -- Z_FLIT_DATA: FLIT 3

  // get some registers
  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][HZOP]: HZOP packet has no address FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // preload the data
  if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][HZOP]: HZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // setup the MemReq
  MemReq req{ Addr, uint16_t( Rs2 ), RevRegClass::RegGPR, Z_MZOP_PIPE_HART, MemOp::MemOpAMO, true, MarkLoadCompleteFunc };

  // set the registers
  Alloc.SetX( Rs1, Data );
  Alloc.SetX( Rs2, 0 );
  zev->setMemReq( req );

  // Look up the opcode
  zopOpc opc = zev->getOpc();
  auto   it  = zopAMOTable.find( opc );
  if( it == zopAMOTable.end() ) {
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", safe_static_cast<uint8_t>( opc )
    );
    return false;
  }

  // Extract the fields
  const auto& [size, stat, amoOp, amoRtn] = it->second;

  auto flags{ amoOp };
  RevFlagSet( flags, amoRtn );
  Mem->AMOMem( Z_HZOP_PIPE_HART, Addr, size, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
  recordStat( stat );

  // add the request to the AMOQ
  AMOQ.emplace_back( zev, Rs1, Rs2 );

  return true;
}

bool RZAAMOCoProc::InjectZOP( zopEvent* zev, bool& flag ) {
  if( zev->getType() != zopMsgT::Z_HZOPAC ) {
    // wrong ZOP type injected
    output->fatal(
      CALL_INFO, -1, "[FORZA][RZA][HZOP]: Cannot handle ZOP message of type: %s\n", zNic->msgTToStr( zev->getType() ).c_str()
    );
  }

  return handleHZOP( zev, flag );
}

void RZAAMOCoProc::CheckLSQueue() {
  // Walk the AMOQ and look for hazards that have been
  // cleared in the Proc's LSQueue
  //
  // If a load has been cleared, then prepare a response
  // packet with the appropriate data

  for( auto it = AMOQ.begin(); it != AMOQ.end(); ++it ) {
    auto& [zev, rs1, rs2] = *it;

    if( Alloc.getState( rs2 ) == Hazard::DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Z_HZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO, -1, "[FORZA][RZA][HZOP]: Failed to send success response for ZOP ID=%" PRIu16 "\n", zev->getID()
        );
      }

      // clear the hazards
      Alloc.clearReg( rs1 );
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0;
      if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
        output->fatal( CALL_INFO, -1, "[FORZA][RZA] Erroneous packet contents for ZOP in CheckLSQueue\n" );
      }
      Mem->clearZRqst( Addr );
      delete zev;
      AMOQ.erase( it );
      return;
    }
  }
}

void RZAAMOCoProc::MarkLoadComplete( const MemReq& req ) {
  Alloc.setDirty( (uint32_t) ( req.getDestReg() ) );
}

}  // namespace SST::RevCPU

// EOF
