//
// _zip_cc_
//

#include <sst/core/sst_config.h>
#include "zip.h"

using namespace SST::Forza;

ZIP::ZIP(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 0);
  output.init("ZIP[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZIP>(this, &ZIP::clock));
  output.output("ZIP[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  // register with SST
  registerAsPrimaryComponent();

  // load the backing memory controller
  Ctrl = loadUserSubComponent<ZIPMemCtrl>("memory");
  if( !Ctrl ){
    output.fatal(CALL_INFO, -1,
                 "Error : failed to initialize the memory controller subcomponent\n");
  }
}

ZIP::~ZIP(){
}

bool ZIP::clock(Cycle_t cycle){
  return false;
}

// EOF
