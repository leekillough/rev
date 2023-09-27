//
// _zip_memctrl_cc_
//
//

#include "zip_memctrl.h"

using namespace SST;
using namespace SST::Forza;
using namespace SST::Interfaces;

// ---------------------------------------------------------------
// ZIPBasicMemCtrl
// ---------------------------------------------------------------
ZIPBasicMemCtrl::ZIPBasicMemCtrl(ComponentId_t id, const Params& params)
  : ZIPMemCtrl(id, params), memIface(nullptr), stdMemHandlers(nullptr),
    LineSize(0), OutstandingReads(0), OutstandingWrites(0){

  // initialize the memory handlers
  stdMemHandlers = new ZIPBasicMemCtrl::ZIPStdMemHandlers(&output, this);

  // read all the parameters
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  NumRead = params.find<unsigned>("num_read", 16);
  NumWrite = params.find<unsigned>("num_write", 16);
  OpsPerCycle = params.find<unsigned>("ops_per_cycle", 32);

  // initialize the memory interface
  memIface = loadUserSubComponent<Interfaces::StandardMem>(
    "memIface", ComponentInfo::SHARE_NONE,
    getTimeConverter(ClockFreq),
    new StandardMem::Handler<SST::Forza::ZIPBasicMemCtrl>(
      this, &ZIPBasicMemCtrl::processMemEvent));

  // register the clock
  registerClock(ClockFreq,
                new Clock::Handler<ZIPBasicMemCtrl>(this,
                                                    &ZIPBasicMemCtrl::clock));
}

ZIPBasicMemCtrl::~ZIPBasicMemCtrl(){
}

void ZIPBasicMemCtrl::init(unsigned int phase){
  // pass the init flag to the memory subsystem
  memIface->init(phase);

  if( phase == 1 ){
    LineSize = memIface->getLineSize();
    output.verbose(CALL_INFO, 5, 0,
                   "Detected cache line size of %u\n", LineSize);
  }
}

void ZIPBasicMemCtrl::setup(){
  memIface->setup();
}

void ZIPBasicMemCtrl::finish(){
}

bool ZIPBasicMemCtrl::sendWRITERequest(uint64_t Addr, uint32_t Size,
                                       std::vector<uint8_t> buf){
  if( Size == 0 ){
    return true;
  }

  ZIPMemOp *Op = new ZIPMemOp(ZIPMemOp::MemOp::ZIP_MemOpWRITE, Addr, Size, buf);
  rqstQ.push_back(Op);
  return true;
}

bool ZIPBasicMemCtrl::sendREADRequest(uint64_t Addr, uint32_t Size){
  if( Size == 0 ){
    return true;
  }

  ZIPMemOp *Op = new ZIPMemOp(ZIPMemOp::MemOp::ZIP_MemOpREAD, Addr, Size);
  rqstQ.push_back(Op);
  return true;
}

void ZIPBasicMemCtrl::processMemEvent(StandardMem::Request* ev){
  output.verbose(CALL_INFO, 15, 0, "Received memory request event\n");
  if( ev == nullptr ){
    output.fatal(CALL_INFO, -1, "Error : Received null memory event\n");
  }
  ev->handle(stdMemHandlers);
}

void ZIPBasicMemCtrl::handleReadResp(StandardMem::ReadResp* ev){
  OutstandingReads--;
}

void ZIPBasicMemCtrl::handleWriteResp(StandardMem::WriteResp* ev){
  OutstandingWrites--;
}

void ZIPBasicMemCtrl::handleFlushResp(StandardMem::FlushResp* ev){
}

void ZIPBasicMemCtrl::handleCustomResp(StandardMem::CustomResp* ev){
}

void ZIPBasicMemCtrl::handleInvResp(StandardMem::InvNotify* ev){
}

bool ZIPBasicMemCtrl::isOpenSlots(){
  if( (OutstandingReads == NumRead) &&
      (OutstandingWrites == NumWrite) ){
    return false;
  }
  return true;
}

bool ZIPBasicMemCtrl::isMemOpAvail(ZIPMemOp *Op ){
  switch(Op->getOp()){
  case ZIPMemOp::MemOp::ZIP_MemOpREAD:
    if( OutstandingReads < NumRead ){
      return true;
    }
    break;
  case ZIPMemOp::MemOp::ZIP_MemOpWRITE:
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

bool ZIPBasicMemCtrl::buildStandardMemRqst(ZIPMemOp *op,
                                           bool &success){
  if( !op )
    return false;

  Interfaces::StandardMem::Request *rqst = nullptr;

  switch(op->getOp()){
  case ZIPMemOp::MemOp::ZIP_MemOpREAD:
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
  case ZIPMemOp::MemOp::ZIP_MemOpWRITE:
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

bool ZIPBasicMemCtrl::processNextRqst( unsigned &t_max_ops ){

  bool success = false;

  // retrieve the next candidate request
  for( unsigned i=0; i<rqstQ.size(); i++ ){
    ZIPMemOp *op = rqstQ[i];
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

bool ZIPBasicMemCtrl::clock(Cycle_t cycle){

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
