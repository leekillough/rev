//
// _zip_h_
//


#ifndef _ZIP_H_
#define _ZIP_H_

#include "zip_sst.h"
#include "zip_memctrl.h"
#include <string>

namespace SST::Forza{
  class ZIP : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZIP,                                    // component class
      "ForzaZIP",                             // component libary
      "ZIP",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZIP: Forza ZIP component",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "clockFreq",  "ZIP core clock frequency", "1GHz" },
      { "verbose",    "Sets the output verbsoity", 0 }
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS()

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"memory", "Backend ZIP memory infrastruture", "SST::Forza::ZIPMemCtrl"}
    )

    // public class members

    /// ZIP: constructor
    ZIP(SST::ComponentId_t id, SST::Params& params);

    /// ZIP: destructor
    ~ZIP();

  private:
    // private class members

    /// ZIP: clock handler
    bool clock(SST::Cycle_t cycle);

    // private data members
    SST::Output output;             ///< ZIP: SST output handler
    ZIPMemCtrl *Ctrl;               ///< ZIP: memory controller
  }; // class SST::ZIP
} // namespace SST::Forza

#endif // _ZIP_H_

// EOF
