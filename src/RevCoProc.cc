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
RevCoProc::RevCoProc( ComponentId_t id, Params &params, RevProc *parent ) :
  SubComponent( id ), output( nullptr ), parent( parent ) {

  uint32_t verbosity = params.find< uint32_t >( "verbose" );
  output             = new SST::Output( "[RevCoProc @t]: ", verbosity, 0, SST::Output::STDOUT );
}

RevCoProc::~RevCoProc() {
  delete output;
}

bool RevCoProc::sendSuccessResp( Forza::zopAPI   *zNic,
                                Forza::zopEvent *zev,
                                 uint16_t         SrcHart ) {
  if( !zNic )
    return false;
  if( !zev )
    return false;

  output->verbose(
    CALL_INFO,
    9,
    0,
                  "[FORZA][RZA][MZOP]: Building MZOP success response for WRITE @ ID=%d\n",
    (unsigned) ( zev->getID() ) );

  // create a new event
  SST::Forza::zopEvent *rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
  rsp_zev->setNB( 0 );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setCredit( 0 );
  rsp_zev->setOpc( SST::Forza::zopOpc::Z_RESP_SACK );
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
  std::vector< uint64_t > payload;
  payload.push_back( 0x00ull );  // ACS
  rsp_zev->setPayload( payload );

  // inject the packet
  zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );

  return true;
}

bool RevCoProc::sendSuccessResp( Forza::zopAPI   *zNic,
                                Forza::zopEvent *zev,
                                uint16_t SrcHart,
                                 uint64_t         Data ) {
  if( zNic == nullptr )
    return false;
  if( zev == nullptr )
    return false;

  output->verbose(
    CALL_INFO,
    9,
    0,
                  "[FORZA][RZA][]: Building LOAD or HZOP response for MSG @ ID=%d\n",
    (unsigned) ( zev->getID() ) );

  uint64_t Addr = 0x00ull;
  zev->getFLIT( Z_FLIT_ADDR, &Addr );

  // create a new event
  SST::Forza::zopEvent *rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
  rsp_zev->setNB( 0 );
  rsp_zev->setID( zev->getID() );
  rsp_zev->setCredit( 0 );
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
  std::vector< uint64_t > payload;
  payload.push_back( 0x00ull );  // ACS
  payload.push_back( Data );     // load response data
  rsp_zev->setPayload( payload );

  // inject the packet
  zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );

  return true;
}

// ---------------------------------------------------------------
// RevSimpleCoProc
// ---------------------------------------------------------------
RevSimpleCoProc::RevSimpleCoProc( ComponentId_t id,
                                  Params       &params,
                                  RevProc      *parent ) :
  RevCoProc( id, params, parent ),
  num_instRetired( 0 ) {

  std::string ClockFreq = params.find< std::string >( "clock", "1Ghz" );
  cycleCount            = 0;

  registerStats();

  //This would be used ot register the clock with SST Core
  /*registerClock( ClockFreq,
    new Clock::Handler<RevSimpleCoProc>(this, &RevSimpleCoProc::ClockTick));
    output->output("Registering subcomponent RevSimpleCoProc with frequency=%s\n", ClockFreq.c_str());*/
}

RevSimpleCoProc::~RevSimpleCoProc(){

};

bool RevSimpleCoProc::IssueInst( RevFeature *F,
                                 RevRegFile *R,
                                 RevMem     *M,
                                 uint32_t    Inst ) {
  RevCoProcInst inst = RevCoProcInst( Inst, F, R, M );
  std::cout << "CoProc instruction issued: " << std::hex << Inst << std::dec
            << std::endl;
  //parent->ExternalDepSet(CreatePasskey(), F->GetHartToExecID(), 7, false);
  InstQ.push( inst );
  return true;
}

void RevSimpleCoProc::registerStats() {
  num_instRetired = registerStatistic< uint64_t >( "InstRetired" );
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
    std::cout << "CoProcessor to execute instruction: " << std::hex << inst
              << std::endl;
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
RZALSCoProc::RZALSCoProc( ComponentId_t id, Params &params, RevProc *parent ) :
  RevCoProc( id, params, parent ), Mem( nullptr ), zNic( nullptr ) {

  std::string ClockFreq = params.find< std::string >( "clock", "1Ghz" );
  registerClock(
    ClockFreq,
    new Clock::Handler< RZALSCoProc >( this, &RZALSCoProc::ClockTick ) );
  output->output( "Registering RZALSCoProc with frequency=%s\n",
                  ClockFreq.c_str() );
  MarkLoadCompleteFunc = [=]( const MemReq &req ) {
    this->MarkLoadComplete( req );
  };

  // register the stats
  registerStats();
}

RZALSCoProc::~RZALSCoProc() {
  for( auto it = LoadQ.begin(); it != LoadQ.end(); ++it ) {
    auto z = std::get< LOADQ_ZEV >( *it );
    delete z;
  }
  LoadQ.clear();
}

void RZALSCoProc::registerStats() {
  for( auto *stat : {
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
    stats.push_back( registerStatistic< uint64_t >( stat ) );
  }
}

void RZALSCoProc::recordStat( RZALSCoProc::mzopStats Stat, uint64_t Data ) {
  if( Stat < RZALSCoProc::MZOP_END ) {
    stats[Stat]->addData( Data );
  }
}

bool RZALSCoProc::IssueInst( RevFeature *F,
                            RevRegFile *R,
                            RevMem *M,
                             uint32_t    Inst ) {
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
    auto zev = it->first;
    auto rs2 = it->second;

    if( Alloc.getState( rs2 ) == _H_DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Z_MZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO,
          -1,
                      "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%d\n",
          zev->getID() );
      }
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0x00ull;
      if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
        output->fatal(
          CALL_INFO,
          -1,
          "[FORZA][RZA] Erroneous packet contents for ZOP in CheckLSQueue\n" );
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

void RZALSCoProc::MarkLoadComplete( const MemReq &req ) {
  Alloc.setDirty( (unsigned) ( req.getDestReg() ) );
}

bool RZALSCoProc::handleMZOP( Forza::zopEvent *zev, bool &flag ) {
  unsigned Rs1  = _UNDEF_REG;
  unsigned Rs2  = _UNDEF_REG;
  uint64_t Addr = 0x00ull;    // -- FLIT 3
  uint64_t Data = 0x00ull;    // -- FLIT 4

  // this is the actual number of data flits
  // this does not include the ACS field (flit=0) and the address
  // these variables are only used for the DMA store operations
  unsigned RealFlitLen = (unsigned) ( zev->getLength() - 2 );
  uint8_t *Buf  = nullptr;
  unsigned i, j, cur = 0;

  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
                  "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%d\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID() );
  }

  // used only for load operations
  MemReq req{ Addr,
              (uint16_t) ( Rs2 ),
              RevRegClass::RegGPR,
              Z_MZOP_PIPE_HART,
              MemOp::MemOpREAD,
              true,
              MarkLoadCompleteFunc };

  // set the address
  Alloc.SetX( Rs1, Addr );

  switch( zev->getOpc() ) {
  // unsigned loads
  case Forza::zopOpc::Z_MZOP_LB:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< uint8_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LH:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< uint16_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LW:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LW, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LD:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal(
      Z_MZOP_PIPE_HART, Addr, Alloc.getRegAddr( Rs2 ), req, RevFlag::F_NONE );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LD, 1 );
    break;
  // signed loads
  case Forza::zopOpc::Z_MZOP_LSB:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< int8_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LSB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LSH:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< int16_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LSH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_LSW:
    Alloc.SetX( Rs2, 0x00ull );
    Mem->ReadVal( Z_MZOP_PIPE_HART,
                  Addr,
                  reinterpret_cast< int32_t * >( Alloc.getRegAddr( Rs2 ) ),
                  req,
                  RevFlag::F_SEXT64 );
    zev->setMemReq( req );
    LoadQ.push_back( std::make_pair( zev, Rs2 ) );
    flag = false;
    recordStat( MZOP_LSW, 1 );
    break;

  // unsigned & signed stores
  case Forza::zopOpc::Z_MZOP_SB:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< uint8_t >( Data ) );
    flag = true;
    recordStat( MZOP_SB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SH:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< uint16_t >( Data ) );
    flag = true;
    recordStat( MZOP_SH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SW:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< uint32_t >( Data ) );
    flag = true;
    recordStat( MZOP_SW, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SD:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, Data );
    flag = true;
    recordStat( MZOP_SD, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSB:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< int8_t >( Data ) );
    flag = true;
    recordStat( MZOP_SSB, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSH:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< int16_t >( Data ) );
    flag = true;
    recordStat( MZOP_SSH, 1 );
    break;
  case Forza::zopOpc::Z_MZOP_SSW:
    if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID() );
    }
    Mem->Write( Z_MZOP_PIPE_HART, Addr, static_cast< int32_t >( Data ) );
    flag = true;
    recordStat( MZOP_SSW, 1 );
    break;

  // dma stores
  case Forza::zopOpc::Z_MZOP_SDMA:
    // build a bulk write
    Buf = new uint8_t[RealFlitLen * 8];

    for( i = 0; i < RealFlitLen; i++ ) {
      Data = 0x00ull;
      if( !zev->getFLIT( ( Z_FLIT_DATA ) + i, &Data ) ) {
        output->fatal( CALL_INFO,
                       -1,
                       "[FORZA][RZA][MZOP]: MZOP packet has no DMA data FLIT: "
                       "Type=%s, ID=%d\n",
                       zNic->msgTToStr( zev->getType() ).c_str(),
                       zev->getID() );
      }

      for( j = 0; j < 8; j++ ) {
        Buf[cur] = ( ( Data >> ( j * 8 ) ) & 0b11111111 );
        cur++;
      }
    }

    // write buffer
    Mem->WriteMem( Z_MZOP_PIPE_HART, Addr, RealFlitLen * 8, Buf );
    delete[] Buf;
    flag = true;
    recordStat( MZOP_SDMA, 1 );
    break;
  default:
    // not an MZOP
    output->verbose( CALL_INFO,
                     9,
                     0,
                    "[FORZA][RZA][MZOP]: Erroneous MZOP opcode=%d\n",
                     (unsigned) ( zev->getOpc() ) );
    return false;
    break;
  }

  if( flag ) {
    // this was a write, signal a success response
    if( !sendSuccessResp( zNic, zev, Z_MZOP_PIPE_HART ) ) {
      output->fatal(
        CALL_INFO,
        -1,
                    "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%d\n",
        zev->getID() );
    }
    // go ahead and clear the RS2 as we dont need it for WRITE requests
    Alloc.clearReg( Rs2 );
    delete zev;
  }

  // consider this clear the dep on RS1 for all requests
  Alloc.clearReg( Rs1 );

  return true;
}

bool RZALSCoProc::InjectZOP( Forza::zopEvent *zev, bool &flag ) {
  if( zev->getType() != Forza::zopMsgT::Z_MZOP ) {
    // wrong ZOP type injected
    output->fatal(
      CALL_INFO,
      -1,
                  "[FORZA][RZA][MZOP]: Cannot handle ZOP message of type: %s\n",
      zNic->msgTToStr( zev->getType() ).c_str() );
  }

  return handleMZOP( zev, flag );
}

// ---------------------------------------------------------------
// RZAAMOCoproc
// ---------------------------------------------------------------
RZAAMOCoProc::RZAAMOCoProc( ComponentId_t id,
                            Params       &params,
                            RevProc      *parent ) :
  RevCoProc( id, params, parent ),
  Mem( nullptr ), zNic( nullptr ) {
  std::string ClockFreq = params.find< std::string >( "clock", "1Ghz" );
  registerClock(
    ClockFreq,
    new Clock::Handler< RZAAMOCoProc >( this, &RZAAMOCoProc::ClockTick ) );
  output->output( "Registering RZAAMOCoProc with frequency=%s\n",
                  ClockFreq.c_str() );
  MarkLoadCompleteFunc = [=]( const MemReq &req ) {
    this->MarkLoadComplete( req );
  };

  // register the stats
  registerStats();
}

RZAAMOCoProc::~RZAAMOCoProc() {
  for( auto it = AMOQ.begin(); it != AMOQ.end(); ++it ) {
    auto z = std::get< AMOQ_ZEV >( *it );
    delete z;
  }
  AMOQ.clear();
}

void RZAAMOCoProc::registerStats() {
  for( auto *stat : {
         "HZOP_32_BASE_ADD",   "HZOP_32_BASE_AND",   "HZOP_32_BASE_OR",
         "HZOP_32_BASE_XOR",   "HZOP_32_BASE_SMAX",  "HZOP_32_BASE_MAX",
         "HZOP_32_BASE_SMIN",  "HZOP_32_BASE_MIN",   "HZOP_32_BASE_SWAP",
         "HZOP_32_BASE_CAS",   "HZOP_32_BASE_FADD",  "HZOP_32_BASE_FSUB",
         "HZOP_32_BASE_FRSUB", "HZOP_64_BASE_ADD",   "HZOP_64_BASE_AND",
         "HZOP_64_BASE_OR",    "HZOP_64_BASE_XOR",   "HZOP_64_BASE_SMAX",
         "HZOP_64_BASE_MAX",   "HZOP_64_BASE_SMIN",  "HZOP_64_BASE_MIN",
         "HZOP_64_BASE_SWAP",  "HZOP_64_BASE_CAS",   "HZOP_64_BASE_FADD",
         "HZOP_64_BASE_FSUB",  "HZOP_64_BASE_FRSUB", "HZOP_32_M_ADD",
         "HZOP_32_M_AND",      "HZOP_32_M_OR",       "HZOP_32_M_XOR",
         "HZOP_32_M_SMAX",     "HZOP_32_M_MAX",      "HZOP_32_M_SMIN",
         "HZOP_32_M_MIN",      "HZOP_32_M_SWAP",     "HZOP_32_M_CAS",
         "HZOP_32_M_FADD",     "HZOP_32_M_FSUB",     "HZOP_32_M_FRSUB",
         "HZOP_64_M_ADD",      "HZOP_64_M_AND",      "HZOP_64_M_OR",
         "HZOP_64_M_XOR",      "HZOP_64_M_SMAX",     "HZOP_64_M_MAX",
         "HZOP_64_M_SMIN",     "HZOP_64_M_MIN",      "HZOP_64_M_SWAP",
         "HZOP_64_M_CAS",      "HZOP_64_M_FADD",     "HZOP_64_M_FSUB",
         "HZOP_64_M_FRSUB",    "HZOP_32_S_ADD",      "HZOP_32_S_AND",
         "HZOP_32_S_OR",       "HZOP_32_S_XOR",      "HZOP_32_S_SMAX",
         "HZOP_32_S_MAX",      "HZOP_32_S_SMIN",     "HZOP_32_S_MIN",
         "HZOP_32_S_SWAP",     "HZOP_32_S_CAS",      "HZOP_32_S_FADD",
         "HZOP_32_S_FSUB",     "HZOP_32_S_FRSUB",    "HZOP_64_S_ADD",
         "HZOP_64_S_AND",      "HZOP_64_S_OR",       "HZOP_64_S_XOR",
         "HZOP_64_S_SMAX",     "HZOP_64_S_MAX",      "HZOP_64_S_SMIN",
         "HZOP_64_S_MIN",      "HZOP_64_S_SWAP",     "HZOP_64_S_CAS",
         "HZOP_64_S_FADD",     "HZOP_64_S_FSUB",     "HZOP_64_S_FRSUB",
         "HZOP_32_MS_ADD",     "HZOP_32_MS_AND",     "HZOP_32_MS_OR",
         "HZOP_32_MS_XOR",     "HZOP_32_MS_SMAX",    "HZOP_32_MS_MAX",
         "HZOP_32_MS_SMIN",    "HZOP_32_MS_MIN",     "HZOP_32_MS_SWAP",
         "HZOP_32_MS_CAS",     "HZOP_32_MS_FADD",    "HZOP_32_MS_FSUB",
         "HZOP_32_MS_FRSUB",   "HZOP_64_MS_ADD",     "HZOP_64_MS_AND",
         "HZOP_64_MS_OR",      "HZOP_64_MS_XOR",     "HZOP_64_MS_SMAX",
         "HZOP_64_MS_MAX",     "HZOP_64_MS_SMIN",    "HZOP_64_MS_MIN",
         "HZOP_64_MS_SWAP",    "HZOP_64_MS_CAS",     "HZOP_64_MS_FADD",
         "HZOP_64_MS_FSUB",    "HZOP_64_MS_FRSUB",
       } ) {
    stats.push_back( registerStatistic< uint64_t >( stat ) );
  }
}

void RZAAMOCoProc::recordStat( RZAAMOCoProc::hzopStats Stat, uint64_t Data ) {
  if( Stat < RZAAMOCoProc::HZOP_END ) {
    stats[Stat]->addData( Data );
  }
}

bool RZAAMOCoProc::IssueInst( RevFeature *F,
                            RevRegFile *R,
                            RevMem *M,
                              uint32_t    Inst ) {
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

bool RZAAMOCoProc::handleHZOP( Forza::zopEvent *zev, bool &flag ) {
  flag = false; // these are handled as READ requests; eg they hazard
  unsigned Rs1  = _UNDEF_REG;
  unsigned Rs2  = _UNDEF_REG;
  uint64_t Addr = 0x00ull;    // -- FLIT 3
  uint64_t Data = 0x00ull;    // -- FLIT 4

  // get some registers
  if( !Alloc.getRegs( Rs1, Rs2 ) ) {
    return false;
  }

  // preload the address
  if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
    output->fatal(
      CALL_INFO,
      -1,
                  "[FORZA][RZA][HZOP]: HZOP packet has no address FLIT: Type=%s, ID=%d\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID() );
  }

  // preload the data
  if( !zev->getFLIT( Z_FLIT_DATA, &Data ) ) {
    output->fatal(
      CALL_INFO,
      -1,
                  "[FORZA][RZA][HZOP]: HZOP packet has no data FLIT: Type=%s, ID=%d\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID() );
  }

  // setup the MemReq
  MemReq req{ Addr,
              (uint16_t) ( Rs2 ),
              RevRegClass::RegGPR,
              Z_MZOP_PIPE_HART,
              MemOp::MemOpAMO,
              true,
              MarkLoadCompleteFunc };

  // set the registers
  Alloc.SetX( Rs1, Data );
  Alloc.SetX( Rs2, 0x00ull );
  zev->setMemReq( req );

  switch( zev->getOpc() ) {
  // 32bit base
  case Forza::zopOpc::Z_HAC_32_BASE_ADD:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOADD );
    recordStat( HZOP_32_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_AND:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOAND );
    recordStat( HZOP_32_BASE_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_OR:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOOR );
    recordStat( HZOP_32_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_XOR:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOXOR );
    recordStat( HZOP_32_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SMAX:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOMAX );
    recordStat( HZOP_32_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_MAX:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOMAXU );
    recordStat( HZOP_32_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SMIN:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOMIN );
    recordStat( HZOP_32_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_MIN:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOMINU );
    recordStat( HZOP_32_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_32_BASE_SWAP:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs1 ) ),
                 reinterpret_cast< uint32_t * >( Alloc.getRegAddr( Rs2 ) ),
                 req,
                 RevFlag::F_AMOSWAP );
    recordStat( HZOP_32_BASE_SWAP, 1 );
    break;
  // 64bit base
  case Forza::zopOpc::Z_HAC_64_BASE_ADD:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOADD );
    recordStat( HZOP_64_BASE_ADD, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_AND:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOAND );
    recordStat( HZOP_64_BASE_AND, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_OR:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOOR );
    recordStat( HZOP_64_BASE_OR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_XOR:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOXOR );
    recordStat( HZOP_64_BASE_XOR, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SMAX:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOMAX );
    recordStat( HZOP_64_BASE_SMAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_MAX:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOMAXU );
    recordStat( HZOP_64_BASE_MAX, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SMIN:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOMIN );
    recordStat( HZOP_64_BASE_SMIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_MIN:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOMINU );
    recordStat( HZOP_64_BASE_MIN, 1 );
    break;
  case Forza::zopOpc::Z_HAC_64_BASE_SWAP:
    Mem->AMOVal( Z_HZOP_PIPE_HART,
                 Addr,
                 Alloc.getRegAddr( Rs1 ),
                 Alloc.getRegAddr( Rs2 ),
                 req,
                 RevFlag::F_AMOSWAP );
    recordStat( HZOP_64_BASE_SWAP, 1 );
    break;
  default:
    output->verbose( CALL_INFO,
                     9,
                     0,
                    "[FORZA][RZA][HZOP]: Erroneous HZOP opcode=%d\n",
                     (unsigned) ( zev->getOpc() ) );
    return false;
    break;
  }

  // add the request to the AMOQ
  AMOQ.push_back( std::make_tuple( zev, Rs1, Rs2 ) );

  return true;
}

bool RZAAMOCoProc::InjectZOP( Forza::zopEvent *zev, bool &flag ) {
  if( ( zev->getType() != Forza::zopMsgT::Z_HZOPAC ) &&
      ( zev->getType() != Forza::zopMsgT::Z_HZOPV ) ) {
    // wrong ZOP type injected
    output->fatal(
      CALL_INFO,
      -1,
                  "[FORZA][RZA][HZOP]: Cannot handle ZOP message of type: %s\n",
      zNic->msgTToStr( zev->getType() ).c_str() );
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
    auto zev = std::get< AMOQ_ZEV >( *it );
    auto rs1 = std::get< AMOQ_RS1 >( *it );
    auto rs2 = std::get< AMOQ_RS2 >( *it );

    if( Alloc.getState( rs2 ) == _H_DIRTY ) {
      // load to register has occurred, time to build a response
      if( !sendSuccessResp( zNic, zev, Z_HZOP_PIPE_HART, Alloc.GetX( rs2 ) ) ) {
        output->fatal(
          CALL_INFO,
          -1,
                      "[FORZA][RZA][HZOP]: Failed to send success response for ZOP ID=%d\n",
          zev->getID() );
      }

      // clear the hazards
      Alloc.clearReg( rs1 );
      Alloc.clearReg( rs2 );

      // clear the request from the ZRqst map
      uint64_t Addr = 0x00ull;
      if( !zev->getFLIT( Z_FLIT_ADDR, &Addr ) ) {
        output->fatal(
          CALL_INFO,
          -1,
          "[FORZA][RZA] Erroneous packet contents for ZOP in CheckLSQueue\n" );
      }
      Mem->clearZRqst( Addr );
      delete zev;
      AMOQ.erase( it );
      return;
    }
  }
}

void RZAAMOCoProc::MarkLoadComplete( const MemReq &req ) {
  Alloc.setDirty( (unsigned) ( req.getDestReg() ) );
}

}  // namespace SST::RevCPU

// EOF
