//
// _RevCoProc_cc_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevCoProc.h"

using namespace SST;
using namespace RevCPU;

// ---------------------------------------------------------------
// NOTES:
// - when a memory response comes back from a load, we need to
//   clear the address queue slot on the RevCPU for the ZIQ.
//   This will probably require a callback
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// RevCoProc
// ---------------------------------------------------------------
RevCoProc::RevCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : SubComponent(id), output(nullptr), parent(parent) {

  uint32_t verbosity = params.find<uint32_t>("verbose");
  output = new SST::Output("[RevCoProc @t]: ", verbosity, 0, SST::Output::STDOUT);
}

RevCoProc::~RevCoProc(){
  delete output;
}

bool RevCoProc::sendSuccessResp(Forza::zopAPI *zNic,
                                Forza::zopEvent *zev,
                                uint16_t SrcHart){
  if( !zNic )
    return false;
  if( !zev )
    return false;

  output->verbose(CALL_INFO, 9, 0,
                  "[FORZA][RZA][MZOP]: Building MZOP success response\n");

  // create a new event
  SST::Forza::zopEvent *rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType(SST::Forza::zopMsgT::Z_RESP);
  rsp_zev->setNB(0);
  rsp_zev->setID(zev->getID());
  rsp_zev->setCredit(0);
  rsp_zev->setOpc(SST::Forza::zopOpc::Z_RESP_SACK);
  rsp_zev->setAppID(0);
  rsp_zev->setDestHart(zev->getSrcHart());
  rsp_zev->setDestZCID(zev->getSrcZCID());
  rsp_zev->setDestPCID(zev->getSrcPCID());
  rsp_zev->setDestPrec(zev->getSrcPrec());
  rsp_zev->setSrcHart(SrcHart);
  rsp_zev->setSrcZCID((uint8_t)(zNic->getEndpointType()));
  rsp_zev->setSrcPCID((uint8_t)(zNic->getPCID(zNic->getZoneID())));
  rsp_zev->setSrcPrec((uint8_t)(zNic->getPrecinctID()));

  // set the payload
  std::vector<uint64_t> payload;
  payload.push_back(0x00ull); // ACS
  rsp_zev->setPayload(payload);

  // inject the packet
  zNic->send(rsp_zev, (SST::Forza::zopCompID)(zev->getSrcZCID()));

  return true;
}

bool RevCoProc::sendSuccessResp(Forza::zopAPI *zNic,
                                Forza::zopEvent *zev,
                                uint16_t SrcHart,
                                uint64_t Data){
  if( !zNic )
    return false;
  if( !zev )
    return false;

  // create a new event
  SST::Forza::zopEvent *rsp_zev = new SST::Forza::zopEvent();

  // set all the fields
  rsp_zev->setType(SST::Forza::zopMsgT::Z_RESP);
  rsp_zev->setNB(0);
  rsp_zev->setID(zev->getID());
  rsp_zev->setCredit(0);
  rsp_zev->setOpc(SST::Forza::zopOpc::Z_RESP_LR);
  rsp_zev->setAppID(0);
  rsp_zev->setDestHart(zev->getSrcHart());
  rsp_zev->setDestZCID(zev->getSrcZCID());
  rsp_zev->setDestPCID(zev->getSrcPCID());
  rsp_zev->setDestPrec(zev->getSrcPrec());
  rsp_zev->setSrcHart(SrcHart);
  rsp_zev->setSrcZCID((uint8_t)(zNic->getEndpointType()));
  rsp_zev->setSrcPCID((uint8_t)(zNic->getPCID(zNic->getZoneID())));
  rsp_zev->setSrcPrec((uint8_t)(zNic->getPrecinctID()));

  // set the payload
  std::vector<uint64_t> payload;
  payload.push_back(0x00ull); // ACS
  payload.push_back(Data);    // load response data
  rsp_zev->setPayload(payload);

  // inject the packet
  zNic->send(rsp_zev, (SST::Forza::zopCompID)(zev->getSrcZCID()));

  return true;
}

// ---------------------------------------------------------------
// RevSimpleCoProc
// ---------------------------------------------------------------
RevSimpleCoProc::RevSimpleCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : RevCoProc(id, params, parent), num_instRetired(0) {

  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  cycleCount = 0;

  registerStats();

  //This would be used ot register the clock with SST Core
  /*registerClock( ClockFreq,
    new Clock::Handler<RevSimpleCoProc>(this, &RevSimpleCoProc::ClockTick));
    output->output("Registering subcomponent RevSimpleCoProc with frequency=%s\n", ClockFreq.c_str());*/
}

RevSimpleCoProc::~RevSimpleCoProc(){

};

bool RevSimpleCoProc::IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst){
  RevCoProcInst inst = RevCoProcInst(Inst, F, R, M);
  std::cout << "CoProc instruction issued: " << std::hex << Inst << std::dec << std::endl;
  //parent->ExternalDepSet(CreatePasskey(), F->GetHartToExecID(), 7, false);
  InstQ.push(inst);
  return true;
}

void RevSimpleCoProc::registerStats(){
  num_instRetired = registerStatistic<uint64_t>("InstRetired");
}

bool RevSimpleCoProc::Reset(){
  InstQ = {};
  return true;
}

bool RevSimpleCoProc::ClockTick(SST::Cycle_t cycle){
  if(!InstQ.empty()){
    uint32_t inst = InstQ.front().Inst;
    //parent->ExternalDepClear(CreatePasskey(), InstQ.front().Feature->GetHartToExecID(), 7, false);
    num_instRetired->addData(1);
    parent->ExternalStallHart(CreatePasskey(), 0);
    InstQ.pop();
    std::cout << "CoProcessor to execute instruction: " << std::hex << inst << std::endl;
    cycleCount = cycle;
  }

  if((cycle - cycleCount) > 500){
    parent->ExternalReleaseHart(CreatePasskey(), 0);
  }
  return true;
}

// ---------------------------------------------------------------
// RZALSCoproc
// ---------------------------------------------------------------
RZALSCoProc::RZALSCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : RevCoProc(id, params, parent), Mem(nullptr), zNic(nullptr),
    Feature(nullptr), RegFile(nullptr){
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  registerClock( ClockFreq,
                 new Clock::Handler<RZALSCoProc>(this, &RZALSCoProc::ClockTick) );
  output->output("Registering RZALSCoProc with frequency=%s\n",
                 ClockFreq.c_str());

  // setup the Feature object
  Feature = new RevFeature("RV64IMAFD", output, 0, 1, Z_MZOP_PIPE_HART);

  // setup the register file
  MarkLoadCompleteFunc = [=](const MemReq& req){parent->MarkLoadComplete(req); };
  RegFile = new RevRegFile(Feature);
  RegFile->SetMarkLoadComplete(MarkLoadCompleteFunc);

  // retrieve the LS queue from the parent
  LSQueue = parent->GetLSQueue();

  // set the load/store queue
  RegFile->SetLSQueue(LSQueue);
}

RZALSCoProc::~RZALSCoProc(){
  for( auto it = LoadQ.begin(); it != LoadQ.end(); ++it ){
    auto z = std::get<LOADQ_ZEV>(*it);
    delete z;
  }
  LoadQ.clear();
  delete Feature;
  delete RegFile;
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

void RZALSCoProc::CheckLSQueue(){
  // Walk the LoadQ and look for hazards that have been
  // cleared in the Proc's LSQueue
  //
  // If a load has been cleared, then prepare a response
  // packet with the appropriate data
  //std::cout << "LoadQ.size() = 0x" << std::hex << LoadQ.size() << std::dec << std::endl;
  for( auto it = LoadQ.begin(); it != LoadQ.end(); ++it ){
    auto zev = std::get<LOADQ_ZEV>(*it);    // ZEV object
    auto rs1 = std::get<LOADQ_RS1>(*it);    // address for a load
    auto rs2 = std::get<LOADQ_RS2>(*it);    // target for a load

    std::cout << "CheckLSQueue: rs1 = " << rs1 << std::endl;
    std::cout << "CheckLSQueue: rs2 = " << rs2 << std::endl;

    if( !parent->ExternalDepCheck(CreatePasskey(), 0, rs2, false) ){
      // dependency has been cleared, the load is complete
      // time to issue a response
      if( !sendSuccessResp(zNic,
                           zev,
                           Z_MZOP_PIPE_HART,
                           RegFile->GetX<uint64_t>(rs2))){
        output->fatal(CALL_INFO, -1,
                      "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%d\n",
                      zev->getID());

      }
      Alloc.clearReg(rs1);
      Alloc.clearReg(rs2);
      delete zev;

      // erase the entry
      std::cout << "erasing an entry" << std::endl;
      LoadQ.erase(it);
    }
  }
}

bool RZALSCoProc::ClockTick(SST::Cycle_t cycle){
  CheckLSQueue();
  return true;
}

bool RZALSCoProc::handleMZOP(Forza::zopEvent *zev, bool &flag){
  unsigned Rs1  = _UNDEF_REG;
  unsigned Rs2  = _UNDEF_REG;
  uint64_t Addr = 0x00ull;    // -- FLIT 3
  uint64_t Data = 0x00ull;    // -- FLIT 4

  // this is the actual number of data flits
  // this does not include the ACS field (flit=0) and the address
  // these variables are only used for the DMA store operations
  unsigned RealFlitLen = (unsigned)(zev->getLength()-2);
  uint8_t *Buf  = nullptr;
  unsigned i,j,cur = 0;

  if( !Alloc.getRegs(Rs1) ){
    std::cout << "<<<<<<<<<<<<<<<< RAN OUT OF REGISTERS" << std::endl;
    return false;
  }

  // preload the address
  if( !zev->getFLIT(Z_FLIT_ADDR, &Addr) ){
    output->fatal(CALL_INFO, -1,
                  "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%d\n",
                  zNic->msgTToStr(zev->getType()).c_str(),
                  zev->getID());
  }

  // set the address
  Alloc.SetX(Rs1, Addr);

  switch( zev->getOpcode() ){
  // unsigned loads
  case Forza::zopOpc::Z_MZOP_LB:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  case Forza::zopOpc::Z_MZOP_LH:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  case Forza::zopOpc::Z_MZOP_LW:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  case Forza::zopOpc::Z_MZOP_LD:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  // signed loads
  case Forza::zopOpc::Z_MZOP_LSB:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  case Forza::zopOpc::Z_MZOP_LSH:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;
  case Forza::zopOpc::Z_MZOP_LSW:
    if( !Alloc.getRegs(Rs2) ){
      return false;
    }
    flag = false;
    break;

  // unsigned & signed stores
  case Forza::zopOpc::Z_MZOP_SB:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<uint8_t>(Data));
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SH:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<uint16_t>(Data));
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SW:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<uint32_t>(Data));
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SD:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, Data);
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSB:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<int8_t>(Data));
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSH:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<int16_t>(Data));
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSW:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Mem->Write(Z_MZOP_PIPE_HART, Addr, static_cast<int32_t>(Data));
    flag = true;
    break;

  // dma stores
  case Forza::zopOpc::Z_MZOP_SDMA:
    // build a bulk write
    Buf = new uint8_t(RealFlitLen*8);
    for( i=0; i<RealFlitLen; i++ ){
      Data = 0x00ull;
      if( !zev->getFLIT((Z_FLIT_ADDR+1)+i, &Data) ){
        output->fatal(CALL_INFO, -1,
                      "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%d\n",
                      zNic->msgTToStr(zev->getType()).c_str(),
                      zev->getID());
      }
      for( j=0; j<8; j++ ){
        Buf[cur] = ((Data >> (j*8)) & 0b11111111);
        cur++;
      }
    }

    // write buffer
    Mem->WriteMem(Z_MZOP_PIPE_HART, Addr, RealFlitLen*8, Buf);
    delete [] Buf;
    flag = true;
    break;
  default:
    // not an MZOP
    output->verbose(CALL_INFO, 9, 0,
                    "[FORZA][RZA][MZOP]: Erroneous MZOP opcode=%d\n",
                    (unsigned)(zev->getOpcode()));
    return false;
    break;
  }

  if( flag ){
    // this was a write, signal a success response
    if( !sendSuccessResp(zNic, zev, Z_MZOP_PIPE_HART) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%d\n",
                    zev->getID());
    }
    Alloc.clearReg(Rs1);
    delete zev;
  }

  return true;
}

bool RZALSCoProc::InjectZOP(Forza::zopEvent *zev, bool &flag){
  if( zev->getType() != Forza::zopMsgT::Z_MZOP ){
    // wrong ZOP type injected
    output->fatal(CALL_INFO, -1,
                  "[FORZA][RZA][MZOP]: Cannot handle ZOP message of type: %s\n",
                  zNic->msgTToStr(zev->getType()).c_str());
  }

  return handleMZOP(zev,flag);

#if 0
  // inject the request into the memory subsystem
  RevInst Inst;
  flag = false;
  unsigned Rs1  = _UNDEF_REG;
  unsigned Rs2  = _UNDEF_REG;
  uint64_t Addr = 0x00ull;    // -- FLIT 3
  uint64_t Data = 0x00ull;    // -- FLIT 4
  bool (*func)(RevFeature *, RevRegFile *, RevMem *, RevInst);

  // pre clear the immediate field
  Inst.imm = 0x00ull;

  if( !Alloc.getRegs(Rs1, Rs2) ){
    std::cout << "<<<<<<<<<<<<<<<< RAN OUT OF REGISTERS" << std::endl;
    return false;
  }

  // preload the address
  if( !zev->getFLIT(Z_FLIT_ADDR, &Addr) ){
    output->fatal(CALL_INFO, -1,
                  "[FORZA][RZA][MZOP]: MZOP packet has no address FLIT: Type=%s, ID=%d\n",
                  zNic->msgTToStr(zev->getType()).c_str(),
                  zev->getID());
  }

  Inst.rs1 = Rs1;
  RegFile->SetX(Inst.rs1, Addr);

  switch( zev->getOpcode() ){
  case Forza::zopOpc::Z_MZOP_LB:
    Inst.rd = Rs2;
    func = load<uint8_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LH:
    Inst.rd = Rs2;
    func = load<uint16_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LW:
    Inst.rd = Rs2;
    func = load<uint32_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LD:
    Inst.rd = Rs2;
    func = load<uint64_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LSB:
    Inst.rd = Rs2;
    func = load<int8_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LSH:
    Inst.rd = Rs2;
    func = load<int16_t>;
    break;
  case Forza::zopOpc::Z_MZOP_LSW:
    Inst.rd = Rs2;
    func = load<int32_t>;
    break;
  case Forza::zopOpc::Z_MZOP_SB:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<uint8_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SH:
    if( !zev->getFLIT(Z_FLIT_DATA, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<uint16_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SW:
    if( !zev->getFLIT(4, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<uint32_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SD:
    if( !zev->getFLIT(4, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<uint64_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSB:
    if( !zev->getFLIT(4, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<int8_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSH:
    if( !zev->getFLIT(4, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<int16_t>;
    flag = true;
    break;
  case Forza::zopOpc::Z_MZOP_SSW:
    if( !zev->getFLIT(4, &Data) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: MZOP packet has no data FLIT: Type=%s, ID=%d\n",
                    zNic->msgTToStr(zev->getType()).c_str(),
                    zev->getID());
    }
    Inst.rs2 = Rs2;
    RegFile->SetX(Inst.rs2, Data);
    func = store<int32_t>;
    flag = true;
    break;
  default:
    // not an MZOP
    output->verbose(CALL_INFO, 9, 0,
                    "[FORZA][RZA][MZOP]: Erroneous MZOP opcode=%d\n",
                    (unsigned)(zev->getOpcode()));
    return false;
    break;
  }

  output->verbose(CALL_INFO, 9, 0,
                  "[FORZA][RZA][MZOP]: Executing MZOP with opcode=%d\n",
                  (unsigned)(zev->getOpcode()));

  // execute the mzop
  func(Feature, RegFile, Mem, Inst);

  // if the request was a memory write, initiate a response
  // and clear the hazards
  if( flag ){
    if( !sendSuccessResp(zNic, zev, Z_MZOP_PIPE_HART) ){
      output->fatal(CALL_INFO, -1,
                    "[FORZA][RZA][MZOP]: Failed to send success response for ZOP ID=%d\n",
                    zev->getID());
    }
    Alloc.clearReg(Inst.rs1);
    Alloc.clearReg(Inst.rs2);
    delete zev;
  }else{
    // request was a read, save it off for response processing
#if 0
    LoadQ.push_back(std::tuple<Forza::zopEvent *,unsigned, unsigned>(zev,
                                                                     Inst.rs1,
                                                                     Inst.rd));
#endif
    parent->ExternalDepSet(CreatePasskey(), 0, Inst.rd, false);
  }

  return true;
#endif
}

// ---------------------------------------------------------------
// RZAAMOCoproc
// ---------------------------------------------------------------
RZAAMOCoProc::RZAAMOCoProc(ComponentId_t id, Params& params, RevProc* parent)
  : RevCoProc(id, params, parent), Mem(nullptr), zNic(nullptr),
    Feature(nullptr), RegFile(nullptr){
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  registerClock( ClockFreq,
                 new Clock::Handler<RZAAMOCoProc>(this, &RZAAMOCoProc::ClockTick) );
  output->output("Registering RZAAMOCoProc with frequency=%s\n",
                 ClockFreq.c_str());

  // setup the Feature object
  Feature = new RevFeature("RV64IMAFD", output, 0, 1, Z_HZOP_PIPE_HART);

  // setup the register file
  MarkLoadCompleteFunc = [=](const MemReq& req){parent->MarkLoadComplete(req); };
  RegFile = new RevRegFile(Feature);
  RegFile->SetMarkLoadComplete(MarkLoadCompleteFunc);

  // retrieve the LS queue from the parent
  LSQueue = parent->GetLSQueue();

  // set the load/store queue
  RegFile->SetLSQueue(LSQueue);
}

RZAAMOCoProc::~RZAAMOCoProc(){
  delete Feature;
  delete RegFile;
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

bool RZAAMOCoProc::InjectZOP(Forza::zopEvent *zev, bool &flag){
  if( (zev->getType() != Forza::zopMsgT::Z_HZOPAC) &&
      (zev->getType() != Forza::zopMsgT::Z_HZOPV) ){
    // wrong ZOP type injected
    output->fatal(CALL_INFO, -1,
                  "[FORZA][RZA][HZOP]: Cannot handle ZOP message of type: %s\n",
                  zNic->msgTToStr(zev->getType()).c_str());
  }

  // this is not a pure MZOP Write
  flag = false;
  return true;
}

// EOF
