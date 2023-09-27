//
// _zen_h_
//


#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "zen_memctrl.h"
#include <string>

namespace SST::Forza{
  class ZEN : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZEN,                                    // component class
      "ForzaZEN",                             // component libary
      "ZEN",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZEN: Forza ZEN component",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "clockFreq",  "ZEN core clock frequency", "1GHz" },
      { "verbose",    "Sets the output verbsoity", 0 }
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS()

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"memory", "Backend ZEN memory infrastruture", "SST::Forza::ZENMemCtrl"}
    )

    // public class members

    /// ZEN: constructor
    ZEN(SST::ComponentId_t id, SST::Params& params);

    /// ZEN: destructor
    ~ZEN();

  private:
    // private class members

    /// ZEN: clock handler
    bool clock(SST::Cycle_t cycle);

    // private data members
    SST::Output output;             ///< ZEN: SST output handler
    ZENMemCtrl *Ctrl;               ///< ZEN: memory controller
  }; // class SST::ZEN
} // namespace SST::Forza

#endif // _ZEN_H_

// EOF
