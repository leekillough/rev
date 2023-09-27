//
// _zen_memctrl_cc_
//
//

#include "zen_memctrl.h"

using namespace SST;
using namespace SST::Forza;
using namespace SST::Interfaces;

// ---------------------------------------------------------------
// ZENBasicMemCtrl
// ---------------------------------------------------------------
ZENBasicMemCtrl::ZENBasicMemCtrl(ComponentId_t id, const Params& params)
  : ZENMemCtrl(id, params), memIface(nullptr), stdMemHandlers(nullptr),
    LineSize(0), OutstandingReads(0), OutstandingWrites(0){

  // initialize the memory handlers
  stdMemHandlers = new ZENBasicMemCtrl::ZENStdMemHandlers(&output, this);

  // read all the parameters
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  NumRead = params.find<unsigned>("num_read", 16);
  NumWrite = params.find<unsigned>("num_write", 16);
  OpsPerCycle = params.find<unsigned>("ops_per_cycle", 32);

  // initialize the memory interface
  memIface = loadUserSubComponent<Interfaces::StandardMem>(
    "memIface", ComponentInfo::SHARE_NONE,
    getTimeConverter(ClockFreq),
    new StandardMem::Handler<SST::Forza::ZENBasicMemCtrl>(
      this, &ZENBasicMemCtrl::processMemEvent));

  // register the clock
  registerClock(ClockFreq,
                new Clock::Handler<ZENBasicMemCtrl>(this,
                                                    &ZENBasicMemCtrl::clock));
}

ZENBasicMemCtrl::~ZENBasicMemCtrl(){
}

void ZENBasicMemCtrl::init(unsigned int phase){
  // pass the init flag to the memory subsystem
  memIface->init(phase);

  if( phase == 1 ){
    LineSize = memIface->getLineSize();
    output.verbose(CALL_INFO, 5, 0,
                   "Detected cache line size of %u\n", LineSize);
  }
}

void ZENBasicMemCtrl::setup(){
  memIface->setup();
}

void ZENBasicMemCtrl::finish(){
}

bool ZENBasicMemCtrl::sendWRITERequest(uint64_t Addr, uint32_t Size,
                                       std::vector<uint8_t> buf){
  if( Size == 0 ){
    return true;
  }

  ZENMemOp *Op = new ZENMemOp(ZENMemOp::MemOp::ZEN_MemOpWRITE, Addr, Size, buf);
  rqstQ.push_back(Op);
  return true;
}

bool ZENBasicMemCtrl::sendREADRequest(uint64_t Addr, uint32_t Size){
  if( Size == 0 ){
    return true;
  }

  ZENMemOp *Op = new ZENMemOp(ZENMemOp::MemOp::ZEN_MemOpREAD, Addr, Size);
  rqstQ.push_back(Op);
  return true;
}

void ZENBasicMemCtrl::processMemEvent(StandardMem::Request* ev){
  output.verbose(CALL_INFO, 15, 0, "Received memory request event\n");
  if( ev == nullptr ){
    output.fatal(CALL_INFO, -1, "Error : Received null memory event\n");
  }
  ev->handle(stdMemHandlers);
}

void ZENBasicMemCtrl::handleReadResp(StandardMem::ReadResp* ev){
  OutstandingReads--;
}

void ZENBasicMemCtrl::handleWriteResp(StandardMem::WriteResp* ev){
  OutstandingWrites--;
}

void ZENBasicMemCtrl::handleFlushResp(StandardMem::FlushResp* ev){
}

void ZENBasicMemCtrl::handleCustomResp(StandardMem::CustomResp* ev){
}

void ZENBasicMemCtrl::handleInvResp(StandardMem::InvNotify* ev){
}

bool ZENBasicMemCtrl::isOpenSlots(){
  if( (OutstandingReads == NumRead) &&
      (OutstandingWrites == NumWrite) ){
    return false;
  }
  return true;
}

bool ZENBasicMemCtrl::isMemOpAvail(ZENMemOp *Op ){
  switch(Op->getOp()){
  case ZENMemOp::MemOp::ZEN_MemOpREAD:
    if( OutstandingReads < NumRead ){
      return true;
    }
    break;
  case ZENMemOp::MemOp::ZEN_MemOpWRITE:
    if( OutstandingWrites < NumWrite ){
      return true;
    }
    break;
  default:
    output.fatal(CALL_INFO, -1, "Error : unknown memory operation type\n");
    return false;
    break;
  }
  return false;
}

bool ZENBasicMemCtrl::buildStandardMemRqst(ZENMemOp *op,
                                           bool &success){
  if( !op )
    return false;

  Interfaces::StandardMem::Request *rqst = nullptr;

  switch(op->getOp()){
  case ZENMemOp::MemOp::ZEN_MemOpREAD:
    rqst = new Interfaces::StandardMem::Read(op->getAddr(),
                                             static_cast<uint64_t>(op->getSize()),
                                             0);
    requests.push_back(rqst->getID());
    outstanding[rqst->getID()] = op;
    memIface->send(rqst);
    OutstandingReads++;
    success = true;
    return true;
    break;
  case ZENMemOp::MemOp::ZEN_MemOpWRITE:
    rqst = new Interfaces::StandardMem::Write(op->getAddr(),
                                             static_cast<uint64_t>(op->getSize()),
                                             op->getBuf(),
                                             0);
    requests.push_back(rqst->getID());
    outstanding[rqst->getID()] = op;
    memIface->send(rqst);
    OutstandingWrites++;
    success = true;
    return true;
    break;
  default:
    output.fatal(CALL_INFO, -1, "Error : unknown memory operation type\n");
    return false;
    break;
  }
  return false;
}

bool ZENBasicMemCtrl::processNextRqst( unsigned &t_max_ops ){

  bool success = false;

  // retrieve the next candidate request
  for( unsigned i=0; i<rqstQ.size(); i++ ){
    ZENMemOp *op = rqstQ[i];
    if( isMemOpAvail(op) ){
      // found a candidate operation to process
      t_max_ops++;

      // build a StandardMem request
      if( !buildStandardMemRqst(op, success) ){
        output.fatal(CALL_INFO, -1, "Error : failed to build memory request\n");
        return false;
      }

      // sent the request, remove it
      if( success ){
        rqstQ.erase(rqstQ.begin()+i);
      }else{
        // an issue was encountered injecting the request,
        // max out our current outstanding requests
        t_max_ops = OpsPerCycle;
      }
    }
  }

  return false;
}

bool ZENBasicMemCtrl::clock(Cycle_t cycle){

  // check to see if there is anything to process
  if( rqstQ.size() == 0 ){
    return false;
  }

  // check to see if we've saturated the number of outstanding requests
  if( !isOpenSlots() ){
    return false;
  }

  // try and initiate an event process this cycle
  bool done = false;
  unsigned t_max_ops = 0;

  while( !done ){
    if( !processNextRqst(t_max_ops) ){
      done = true;
    }
    if( t_max_ops == OpsPerCycle ){
      done = true;
    }
  }

  return false;
}

// EOF
