#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>

namespace SST::Forza{
  class ZOPGen_prec : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZOPGen_prec,                                    // component class
      "forzazen",                             // component libary
      "ZOPGen_prec",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZOPGen_prec: Forza ZOPGen component for interprecinct communication",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "clockFreq",  "ZOPGen core clock frequency", "1GHz" },
      { "verbose",    "Sets the output verbosity", "0" },
      { "int_id",    "Precinct ID", "0" },
      { "zone_id",    "Zone ID", "0" },
      { "num_ZOPs",   "Number of ZOPs to send per cycle", "1" },
      { "num_cycles", "Number of cycles to send ZOPs", "1" },
      { "interval", "Number of cycles to wait between sending ZOPs", "1" },
      { "test", "Which test configuration to run", "0" }
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS(
      {"rtrLink", "Links to Network.", {"merlin.linkcontrol"}},
    )

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"m_zop_iface","[FORZA] Zone NIC", "SST::Forza::zopNIC"},
    )

    // public class members
    /// ZOPGen: constructor
    ZOPGen_prec(SST::ComponentId_t id, SST::Params& params);

    /// ZOPGen: destructor
    ~ZOPGen_prec();
    void init(unsigned int phase) override;
    void setup() override;
    void complete(unsigned int phase) override;
    void finish() override;

  private:
    // private class members

    uint64_t int_id;
    uint64_t p_zone_id;
    uint64_t p_numZOPs;
    uint64_t p_numCycles;
    uint64_t p_interval;
    uint64_t p_test;

    /// ZOPGen: clock handler
    bool clock(SST::Cycle_t cycle);
    bool clock2(SST::Cycle_t cycle);

    void handleIncomingZOP(SST::Event* event);

    // private data members
    SST::Output output;             ///< ZOPGen: SST output handler
    SST::Forza::zopAPI* m_zop_iface;
  }; // class SST::ZOPGen
} // namespace SST::Forza
