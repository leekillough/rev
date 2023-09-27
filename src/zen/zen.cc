//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"

using namespace SST::Forza;

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 0);
  output.init("ZEN[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZEN>(this, &ZEN::clock));
  output.output("ZEN[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  // register with SST
  registerAsPrimaryComponent();

  // load the backing memory controller
  Ctrl = loadUserSubComponent<ZENMemCtrl>("memory");
  if( !Ctrl ){
    output.fatal(CALL_INFO, -1,
                 "Error : failed to initialize the memory controller subcomponent\n");
  }
}

ZEN::~ZEN(){
}

bool ZEN::clock(Cycle_t cycle){
  return false;
}

// EOF
