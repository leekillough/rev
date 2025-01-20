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

bool RevCoProc::sendSuccessResp( Forza::zopAPI* zNic, Forza::zopEvent* zev, uint16_t SrcHart ) {
  if( !zNic )
    return false;
  if( !zev )
    return false;

  output->verbose(
    CALL_INFO, 9, 0, "[FORZA][RZA][MZOP]: Building MZOP success response for WRITE @ ID=%" PRIu16 "\n", zev->getID()
  );

  // create a new event
  SST::Forza::zopEvent* rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setOpc( SST::Forza::zopOpc::Z_RESP_SACK );
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
  //if (zev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SDMA) {
  //output->verbose(CALL_INFO, 5, 0, "Received SDMA from %s to %s\n", zev->getSrcString().c_str(), zev->getDestString().c_str() );
  //output->verbose(CALL_INFO, 5, 0, "Sending RESP_SACK from %s to %s\n", rsp_zev->getSrcString().c_str(), rsp_zev->getDestString().c_str() );
  //}

  // inject the packet
  zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );

  return true;
}

bool RevCoProc::sendSuccessResp( Forza::zopAPI* zNic, Forza::zopEvent* zev, uint16_t SrcHart, uint64_t Data ) {
  if( zNic == nullptr )
    return false;
  if( zev == nullptr )
    return false;

  output->verbose( CALL_INFO, 9, 0, "[FORZA][RZA][]: Building LOAD or HZOP response for MSG @ ID=%" PRIu16 "\n", zev->getID() );

  uint64_t Addr = 0;
  zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr );

  // create a new event
  SST::Forza::zopEvent* rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setOpc( SST::Forza::zopOpc::Z_RESP_LR );
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
  zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );

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

void RZALSCoProc::recordStat( RZALSCoProc::mzopStats Stat, uint64_t Data ) {
  if( Stat < RZALSCoProc::MZOP_END ) {
    stats[Stat]->addData( Data );
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

    if( Alloc.getState( rs2 ) == _H_DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Forza::Z_MZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO, -1, "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%" PRIu16 "\n", zev->getID()
        );
      }
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0;
      if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr ) ) {
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

bool RZALSCoProc::handleMZOP( Forza::zopEvent* zev, bool& flag ) {
  uint32_t Rs1         = _UNDEF_REG;
  uint32_t Rs2         = _UNDEF_REG;
  uint64_t Addr        = 0;  // -- Forza::Z_FLIT_ADDR: FLIT 2
  uint64_t Data        = 0;  // -- Forza::Z_FLIT_DATA: FLIT 3

  // this is the actual number of data flits
  // these variables are only used for the DMA store operations
  uint32_t RealFlitLen = (uint32_t) ( zev->getLength() );
  uint32_t i, j, cur = 0;

  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // used only for load operations
  MemReq req{
    Addr, (uint16_t) ( Rs2 ), RevRegClass::RegGPR, Forza::Z_MZOP_PIPE_HART, MemOp::MemOpREAD, true, MarkLoadCompleteFunc };

  // set the address
  Alloc.SetX( Rs1, Addr );

  switch( zev->getOpc() ) {
  // uint32_t loads
  case Forza::zopOpc::Z_MZOP_LB:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<uint8_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LH:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<uint16_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LW:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LW, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LD:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs2 ), req, RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LD, 1 );
    break;
  // signed loads
  case Forza::zopOpc::Z_MZOP_LSB:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<int8_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LSB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LSH:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<int16_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LSH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LSW:
    Alloc.SetX( Rs2, 0 );
    Mem->ReadVal( Forza::Z_MZOP_PIPE_HART, Addr, reinterpret_cast<int32_t*>( Alloc.getRegAddr( Rs2 ) ), req, RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.emplace_back( zev, Rs2 );
    flag = false;
    recordStat( MZOP_LSW, 1 );
    break;

  // uint32_t & signed stores
  case Forza::zopOpc::Z_MZOP_SB:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<uint8_t>( Data ) );
    flag = true;
    recordStat( MZOP_SB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SH:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<uint16_t>( Data ) );
    flag = true;
    recordStat( MZOP_SH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SW:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<uint32_t>( Data ) );
    flag = true;
    recordStat( MZOP_SW, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SD:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, Data );
    flag = true;
    recordStat( MZOP_SD, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSB:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<int8_t>( Data ) );
    flag = true;
    recordStat( MZOP_SSB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSH:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<int16_t>( Data ) );
    flag = true;
    recordStat( MZOP_SSH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSW:
    if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( Forza::Z_MZOP_PIPE_HART, Addr, static_cast<int32_t>( Data ) );
    flag = true;
    recordStat( MZOP_SSW, 1 );
    break;

  // dma stores
  case Forza::zopOpc::Z_MZOP_SDMA: {
    // build a bulk write
    auto Buf = std::make_unique<uint8_t[]>( RealFlitLen * 8 );

    for( i = 0; i < RealFlitLen; i++ ) {
      Data = 0;
      if( !zev->getFLIT( ( Forza::Z_FLIT_DATA ) + i, &Data ) ) {
        output->fatal(
          CALL_INFO,
          -1,
          "[FORZA][RZA][MZOP]: MZOP packet has no DMA data FLIT: "
          "Type=%s, ID=%" PRIu16 "\n",
          zNic->msgTToStr( zev->getType() ).c_str(),
          zev->getID()
        );
      }

      for( j = 0; j < 8; j++ ) {
        Buf[cur] = ( ( Data >> ( j * 8 ) ) & 0b11111111 );
        cur++;
      }
    }

    // write buffer
    Mem->WriteMem( Forza::Z_MZOP_PIPE_HART, Addr, RealFlitLen * 8, Buf.get() );
    flag = true;
    recordStat( MZOP_SDMA, 1 );
  } break;
  default:
    // not an MZOP
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][MZOP]: Erroneous MZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  }

  if( flag ) {
    // this was a write, signal a success response
    if( !sendSuccessResp( zNic, zev, Forza::Z_MZOP_PIPE_HART ) ) {
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

bool RZALSCoProc::InjectZOP( Forza::zopEvent* zev, bool& flag ) {
  if( zev->getType() != Forza::zopMsgT::Z_MZOP ) {
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
         "HZOP_8_BASE_ADD",    "HZOP_8_BASE_AND",    "HZOP_8_BASE_OR",    "HZOP_8_BASE_XOR",    "HZOP_8_BASE_SMAX",
         "HZOP_8_BASE_MAX",    "HZOP_8_BASE_SMIN",   "HZOP_8_BASE_MIN",   "HZOP_8_BASE_SWAP",   "HZOP_8_BASE_CAS",
         "HZOP_8_BASE_FADD",   "HZOP_8_BASE_FSUB",   "HZOP_8_BASE_FRSUB", "HZOP_16_BASE_ADD",   "HZOP_16_BASE_AND",
         "HZOP_16_BASE_OR",    "HZOP_16_BASE_XOR",   "HZOP_16_BASE_SMAX", "HZOP_16_BASE_MAX",   "HZOP_16_BASE_SMIN",
         "HZOP_16_BASE_MIN",   "HZOP_16_BASE_SWAP",  "HZOP_16_BASE_CAS",  "HZOP_16_BASE_FADD",  "HZOP_16_BASE_FSUB",
         "HZOP_16_BASE_FRSUB", "HZOP_32_BASE_ADD",   "HZOP_32_BASE_AND",  "HZOP_32_BASE_OR",    "HZOP_32_BASE_XOR",
         "HZOP_32_BASE_SMAX",  "HZOP_32_BASE_MAX",   "HZOP_32_BASE_SMIN", "HZOP_32_BASE_MIN",   "HZOP_32_BASE_SWAP",
         "HZOP_32_BASE_CAS",   "HZOP_32_BASE_FADD",  "HZOP_32_BASE_FSUB", "HZOP_32_BASE_FRSUB", "HZOP_64_BASE_ADD",
         "HZOP_64_BASE_AND",   "HZOP_64_BASE_OR",    "HZOP_64_BASE_XOR",  "HZOP_64_BASE_SMAX",  "HZOP_64_BASE_MAX",
         "HZOP_64_BASE_SMIN",  "HZOP_64_BASE_MIN",   "HZOP_64_BASE_SWAP", "HZOP_64_BASE_CAS",   "HZOP_64_BASE_FADD",
         "HZOP_64_BASE_FSUB",  "HZOP_64_BASE_FRSUB", "HZOP_8_M_ADD",      "HZOP_8_M_AND",       "HZOP_8_M_OR",
         "HZOP_8_M_XOR",       "HZOP_8_M_SMAX",      "HZOP_8_M_MAX",      "HZOP_8_M_SMIN",      "HZOP_8_M_MIN",
         "HZOP_8_M_SWAP",      "HZOP_8_M_CAS",       "HZOP_8_M_FADD",     "HZOP_8_M_FSUB",      "HZOP_8_M_FRSUB",
         "HZOP_16_M_ADD",      "HZOP_16_M_AND",      "HZOP_16_M_OR",      "HZOP_16_M_XOR",      "HZOP_16_M_SMAX",
         "HZOP_16_M_MAX",      "HZOP_16_M_SMIN",     "HZOP_16_M_MIN",     "HZOP_16_M_SWAP",     "HZOP_16_M_CAS",
         "HZOP_16_M_FADD",     "HZOP_16_M_FSUB",     "HZOP_16_M_FRSUB",   "HZOP_32_M_ADD",      "HZOP_32_M_AND",
         "HZOP_32_M_OR",       "HZOP_32_M_XOR",      "HZOP_32_M_SMAX",    "HZOP_32_M_MAX",      "HZOP_32_M_SMIN",
         "HZOP_32_M_MIN",      "HZOP_32_M_SWAP",     "HZOP_32_M_CAS",     "HZOP_32_M_FADD",     "HZOP_32_M_FSUB",
         "HZOP_32_M_FRSUB",    "HZOP_64_M_ADD",      "HZOP_64_M_AND",     "HZOP_64_M_OR",       "HZOP_64_M_XOR",
         "HZOP_64_M_SMAX",     "HZOP_64_M_MAX",      "HZOP_64_M_SMIN",    "HZOP_64_M_MIN",      "HZOP_64_M_SWAP",
         "HZOP_64_M_CAS",      "HZOP_64_M_FADD",     "HZOP_64_M_FSUB",    "HZOP_64_M_FRSUB",    "HZOP_8_S_ADD",
         "HZOP_8_S_AND",       "HZOP_8_S_OR",        "HZOP_8_S_XOR",      "HZOP_8_S_SMAX",      "HZOP_8_S_MAX",
         "HZOP_8_S_SMIN",      "HZOP_8_S_MIN",       "HZOP_8_S_SWAP",     "HZOP_8_S_CAS",       "HZOP_8_S_FADD",
         "HZOP_8_S_FSUB",      "HZOP_8_S_FRSUB",     "HZOP_16_S_ADD",     "HZOP_16_S_AND",      "HZOP_16_S_OR",
         "HZOP_16_S_XOR",      "HZOP_16_S_SMAX",     "HZOP_16_S_MAX",     "HZOP_16_S_SMIN",     "HZOP_16_S_MIN",
         "HZOP_16_S_SWAP",     "HZOP_16_S_CAS",      "HZOP_16_S_FADD",    "HZOP_16_S_FSUB",     "HZOP_16_S_FRSUB",
         "HZOP_32_S_ADD",      "HZOP_32_S_AND",      "HZOP_32_S_OR",      "HZOP_32_S_XOR",      "HZOP_32_S_SMAX",
         "HZOP_32_S_MAX",      "HZOP_32_S_SMIN",     "HZOP_32_S_MIN",     "HZOP_32_S_SWAP",     "HZOP_32_S_CAS",
         "HZOP_32_S_FADD",     "HZOP_32_S_FSUB",     "HZOP_32_S_FRSUB",   "HZOP_64_S_ADD",      "HZOP_64_S_AND",
         "HZOP_64_S_OR",       "HZOP_64_S_XOR",      "HZOP_64_S_SMAX",    "HZOP_64_S_MAX",      "HZOP_64_S_SMIN",
         "HZOP_64_S_MIN",      "HZOP_64_S_SWAP",     "HZOP_64_S_CAS",     "HZOP_64_S_FADD",     "HZOP_64_S_FSUB",
         "HZOP_64_S_FRSUB",    "HZOP_8_MS_ADD",      "HZOP_8_MS_AND",     "HZOP_8_MS_OR",       "HZOP_8_MS_XOR",
         "HZOP_8_MS_SMAX",     "HZOP_8_MS_MAX",      "HZOP_8_MS_SMIN",    "HZOP_8_MS_MIN",      "HZOP_8_MS_SWAP",
         "HZOP_8_MS_CAS",      "HZOP_8_MS_FADD",     "HZOP_8_MS_FSUB",    "HZOP_8_MS_FRSUB",    "HZOP_16_MS_ADD",
         "HZOP_16_MS_AND",     "HZOP_16_MS_OR",      "HZOP_16_MS_XOR",    "HZOP_16_MS_SMAX",    "HZOP_16_MS_MAX",
         "HZOP_16_MS_SMIN",    "HZOP_16_MS_MIN",     "HZOP_16_MS_SWAP",   "HZOP_16_MS_CAS",     "HZOP_16_MS_FADD",
         "HZOP_16_MS_FSUB",    "HZOP_16_MS_FRSUB",   "HZOP_32_MS_ADD",    "HZOP_32_MS_AND",     "HZOP_32_MS_OR",
         "HZOP_32_MS_XOR",     "HZOP_32_MS_SMAX",    "HZOP_32_MS_MAX",    "HZOP_32_MS_SMIN",    "HZOP_32_MS_MIN",
         "HZOP_32_MS_SWAP",    "HZOP_32_MS_CAS",     "HZOP_32_MS_FADD",   "HZOP_32_MS_FSUB",    "HZOP_32_MS_FRSUB",
         "HZOP_64_MS_ADD",     "HZOP_64_MS_AND",     "HZOP_64_MS_OR",     "HZOP_64_MS_XOR",     "HZOP_64_MS_SMAX",
         "HZOP_64_MS_MAX",     "HZOP_64_MS_SMIN",    "HZOP_64_MS_MIN",    "HZOP_64_MS_SWAP",    "HZOP_64_MS_CAS",
         "HZOP_64_MS_FADD",    "HZOP_64_MS_FSUB",    "HZOP_64_MS_FRSUB",
       } ) {
    stats.push_back( registerStatistic<uint64_t>( stat ) );
  }
}

void RZAAMOCoProc::recordStat( RZAAMOCoProc::hzopStats Stat, uint64_t Data ) {
  if( Stat < RZAAMOCoProc::HZOP_END ) {
    stats[Stat]->addData( Data );
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

bool RZAAMOCoProc::handleHZOP( Forza::zopEvent* zev, bool& flag ) {
  flag          = false;  // these are handled as READ requests; eg they hazard
  uint32_t Rs1  = _UNDEF_REG;
  uint32_t Rs2  = _UNDEF_REG;
  uint64_t Addr = 0;  // -- Forza::Z_FLIT_ADDR: FLIT 2
  uint64_t Data = 0;  // -- Forza::Z_FLIT_DATA: FLIT 3

  // get some registers
  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][HZOP]: HZOP packet has no address FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // preload the data
  if( !zev->getFLIT( Forza::Z_FLIT_DATA, &Data ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "[FORZA][RZA][HZOP]: HZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  // setup the MemReq
  MemReq req{ Addr, (uint16_t) ( Rs2 ), RevRegClass::RegGPR, Forza::Z_MZOP_PIPE_HART, MemOp::MemOpAMO, true, MarkLoadCompleteFunc };

  // set the registers
  Alloc.SetX( Rs1, Data );
  Alloc.SetX( Rs2, 0 );
  zev->setMemReq( req );

  // temporary flag value
  RevFlag flags{ RevFlag::F_NONE };

  switch( zev->getOpc() ) {
  // 8bit base
  case Forza::zopOpc::Z_HAC_8_BASE_ADD:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOADD
    );
    recordStat( HZOP_8_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_AND:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOAND
    );
    recordStat( HZOP_8_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_OR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOOR
    );
    recordStat( HZOP_8_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_XOR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOXOR
    );
    recordStat( HZOP_8_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_SMAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAX
    );
    recordStat( HZOP_8_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_MAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAXU
    );
    recordStat( HZOP_8_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_SMIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMIN
    );
    recordStat( HZOP_8_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_MIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMINU
    );
    recordStat( HZOP_8_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_SWAP:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOSWAP
    );
    recordStat( HZOP_8_BASE_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_BASE_CAS:
  case Forza::zopOpc::Z_HAC_8_BASE_FADD:
  case Forza::zopOpc::Z_HAC_8_BASE_FSUB:
  case Forza::zopOpc::Z_HAC_8_BASE_FRSUB:
  case Forza::zopOpc::Z_HAC_8_BASE_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 16bit base
  case Forza::zopOpc::Z_HAC_16_BASE_ADD:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOADD
    );
    recordStat( HZOP_16_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_AND:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOAND
    );
    recordStat( HZOP_16_BASE_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_OR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOOR
    );
    recordStat( HZOP_16_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_XOR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOXOR
    );
    recordStat( HZOP_16_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_SMAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAX
    );
    recordStat( HZOP_16_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_MAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAXU
    );
    recordStat( HZOP_16_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_SMIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMIN
    );
    recordStat( HZOP_16_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_MIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMINU
    );
    recordStat( HZOP_16_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_SWAP:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOSWAP
    );
    recordStat( HZOP_16_BASE_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_BASE_CAS:
  case Forza::zopOpc::Z_HAC_16_BASE_FADD:
  case Forza::zopOpc::Z_HAC_16_BASE_FSUB:
  case Forza::zopOpc::Z_HAC_16_BASE_FRSUB:
  case Forza::zopOpc::Z_HAC_16_BASE_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 32bit base
  case Forza::zopOpc::Z_HAC_32_BASE_ADD:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOADD
    );
    recordStat( HZOP_32_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_AND:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOAND
    );
    recordStat( HZOP_32_BASE_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_OR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOOR
    );
    recordStat( HZOP_32_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_XOR:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOXOR
    );
    recordStat( HZOP_32_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SMAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAX
    );
    recordStat( HZOP_32_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_MAX:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMAXU
    );
    recordStat( HZOP_32_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SMIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMIN
    );
    recordStat( HZOP_32_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_MIN:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOMINU
    );
    recordStat( HZOP_32_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SWAP:
    Mem->AMOVal(
      Forza::Z_HZOP_PIPE_HART,
      Addr,
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs1 ) ),
      reinterpret_cast<uint32_t*>( Alloc.getRegAddr( Rs2 ) ),
      req,
      RevFlag::F_AMOSWAP
    );
    recordStat( HZOP_32_BASE_SWAP, 1 );
    break;
  // 64bit base
  case Forza::zopOpc::Z_HAC_64_BASE_ADD:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOADD );
    recordStat( HZOP_64_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_AND:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOAND );
    recordStat( HZOP_64_BASE_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_OR:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOOR );
    recordStat( HZOP_64_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_XOR:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOXOR );
    recordStat( HZOP_64_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SMAX:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOMAX );
    recordStat( HZOP_64_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_MAX:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOMAXU );
    recordStat( HZOP_64_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SMIN:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOMIN );
    recordStat( HZOP_64_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_MIN:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOMINU );
    recordStat( HZOP_64_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SWAP:
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, RevFlag::F_AMOSWAP );
    recordStat( HZOP_64_BASE_SWAP, 1 );
    break;
  // 8bit M
  case Forza::zopOpc::Z_HAC_8_M_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_M_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_M_CAS:
  case Forza::zopOpc::Z_HAC_8_M_FADD:
  case Forza::zopOpc::Z_HAC_8_M_FSUB:
  case Forza::zopOpc::Z_HAC_8_M_FRSUB:
  case Forza::zopOpc::Z_HAC_8_M_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 16bit M
  case Forza::zopOpc::Z_HAC_16_M_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_M_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_M_CAS:
  case Forza::zopOpc::Z_HAC_16_M_FADD:
  case Forza::zopOpc::Z_HAC_16_M_FSUB:
  case Forza::zopOpc::Z_HAC_16_M_FRSUB:
  case Forza::zopOpc::Z_HAC_16_M_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 32bit M
  case Forza::zopOpc::Z_HAC_32_M_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_M_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_M_CAS:
  case Forza::zopOpc::Z_HAC_32_M_FADD:
  case Forza::zopOpc::Z_HAC_32_M_FSUB:
  case Forza::zopOpc::Z_HAC_32_M_FRSUB:
  case Forza::zopOpc::Z_HAC_32_M_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 64bit M
  case Forza::zopOpc::Z_HAC_64_M_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONN );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_M_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_M_CAS:
  case Forza::zopOpc::Z_HAC_64_M_FADD:
  case Forza::zopOpc::Z_HAC_64_M_FSUB:
  case Forza::zopOpc::Z_HAC_64_M_FRSUB:
  case Forza::zopOpc::Z_HAC_64_M_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 8bit S
  case Forza::zopOpc::Z_HAC_8_S_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_S_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_S_CAS:
  case Forza::zopOpc::Z_HAC_8_S_FADD:
  case Forza::zopOpc::Z_HAC_8_S_FSUB:
  case Forza::zopOpc::Z_HAC_8_S_FRSUB:
  case Forza::zopOpc::Z_HAC_8_S_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 16bit S
  case Forza::zopOpc::Z_HAC_16_S_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_S_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_S_CAS:
  case Forza::zopOpc::Z_HAC_16_S_FADD:
  case Forza::zopOpc::Z_HAC_16_S_FSUB:
  case Forza::zopOpc::Z_HAC_16_S_FRSUB:
  case Forza::zopOpc::Z_HAC_16_S_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 32bit S
  case Forza::zopOpc::Z_HAC_32_S_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_S_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_S_CAS:
  case Forza::zopOpc::Z_HAC_32_S_FADD:
  case Forza::zopOpc::Z_HAC_32_S_FSUB:
  case Forza::zopOpc::Z_HAC_32_S_FRSUB:
  case Forza::zopOpc::Z_HAC_32_S_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 64bit S
  case Forza::zopOpc::Z_HAC_64_S_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMOON );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_S_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_S_CAS:
  case Forza::zopOpc::Z_HAC_64_S_FADD:
  case Forza::zopOpc::Z_HAC_64_S_FSUB:
  case Forza::zopOpc::Z_HAC_64_S_FRSUB:
  case Forza::zopOpc::Z_HAC_64_S_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 8bit MS
  case Forza::zopOpc::Z_HAC_8_MS_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_8_MS_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_8_MS_CAS:
  case Forza::zopOpc::Z_HAC_8_MS_FADD:
  case Forza::zopOpc::Z_HAC_8_MS_FSUB:
  case Forza::zopOpc::Z_HAC_8_MS_FRSUB:
  case Forza::zopOpc::Z_HAC_8_MS_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 16bit MS
  case Forza::zopOpc::Z_HAC_16_MS_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_16_MS_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_16_MS_CAS:
  case Forza::zopOpc::Z_HAC_16_MS_FADD:
  case Forza::zopOpc::Z_HAC_16_MS_FSUB:
  case Forza::zopOpc::Z_HAC_16_MS_FRSUB:
  case Forza::zopOpc::Z_HAC_16_MS_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 32bit MS
  case Forza::zopOpc::Z_HAC_32_MS_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_32_MS_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_MS_CAS:
  case Forza::zopOpc::Z_HAC_32_MS_FADD:
  case Forza::zopOpc::Z_HAC_32_MS_FSUB:
  case Forza::zopOpc::Z_HAC_32_MS_FRSUB:
  case Forza::zopOpc::Z_HAC_32_MS_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  // 64bit MS
  case Forza::zopOpc::Z_HAC_64_MS_ADD:
    RevFlagSet( flags, RevFlag::F_AMOADD );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_AND:
    RevFlagSet( flags, RevFlag::F_AMOAND );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_OR:
    RevFlagSet( flags, RevFlag::F_AMOOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_XOR:
    RevFlagSet( flags, RevFlag::F_AMOXOR );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_SMAX:
    RevFlagSet( flags, RevFlag::F_AMOMAX );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_MAX:
    RevFlagSet( flags, RevFlag::F_AMOMAXU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_SMIN:
    RevFlagSet( flags, RevFlag::F_AMOMIN );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_MIN:
    RevFlagSet( flags, RevFlag::F_AMOMINU );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_SWAP:
    RevFlagSet( flags, RevFlag::F_AMOSWAP );
    RevFlagSet( flags, RevFlag::F_AMONO );
    Mem->AMOVal( Forza::Z_HZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs1 ), Alloc.getRegAddr( Rs2 ), req, flags );
    recordStat( HZOP_64_MS_SWAP, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_MS_CAS:
  case Forza::zopOpc::Z_HAC_64_MS_FADD:
  case Forza::zopOpc::Z_HAC_64_MS_FSUB:
  case Forza::zopOpc::Z_HAC_64_MS_FRSUB:
  case Forza::zopOpc::Z_HAC_64_MS_THRESH:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Unimplemented HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  default:
    output->verbose(
      CALL_INFO, 9, 0, "[FORZA][RZA][HZOP]: Erroneous HZOP opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() )
    );
    return false;
    break;
  }

  // add the request to the AMOQ
  AMOQ.emplace_back( zev, Rs1, Rs2 );

  return true;
}

bool RZAAMOCoProc::InjectZOP( Forza::zopEvent* zev, bool& flag ) {
  if( zev->getType() != Forza::zopMsgT::Z_HZOPAC ) {
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

    if( Alloc.getState( rs2 ) == _H_DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Forza::Z_HZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO, -1, "[FORZA][RZA][HZOP]: Failed to send success response for ZOP ID=%" PRIu16 "\n", zev->getID()
        );
      }

      // clear the hazards
      Alloc.clearReg( rs1 );
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0;
      if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr ) ) {
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
