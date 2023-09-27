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
    LineSize(0){

  // initialize the memory handlers
  stdMemHandlers = new ZENBasicMemCtrl::ZENStdMemHandlers(&output, this);

  // read all the parameters
  std::string ClockFreq = params.find<std::string>("clock", "1Ghz");

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

void ZENBasicMemCtrl::processMemEvent(StandardMem::Request* ev){
  output.verbose(CALL_INFO, 15, 0, "Received memory request event\n");
  if( ev == nullptr ){
    output.fatal(CALL_INFO, -1, "Error : Received null memory event\n");
  }
  ev->handle(stdMemHandlers);
}

void ZENBasicMemCtrl::handleReadResp(StandardMem::ReadResp* ev){
}

void ZENBasicMemCtrl::handleWriteResp(StandardMem::WriteResp* ev){
}

void ZENBasicMemCtrl::handleFlushResp(StandardMem::FlushResp* ev){
}

void ZENBasicMemCtrl::handleCustomResp(StandardMem::CustomResp* ev){
}

void ZENBasicMemCtrl::handleInvResp(StandardMem::InvNotify* ev){
}

bool ZENBasicMemCtrl::clock(Cycle_t cycle){
  return false;
}

// EOF
