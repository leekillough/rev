//
// _zopgen_h_
//


#ifndef _ZOPGen_H_
#define _ZOPGen_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>

namespace SST::Forza{
  class ZOPGen : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZOPGen,                                    // component class
      "forzazen",                             // component libary
      "ZOPGen",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZOPGen: Forza ZOPGen component",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "clockFreq",  "ZOPGen core clock frequency", "1GHz" },
      { "verbose",    "Sets the output verbosity", "0" },
      { "tests",      "Show test output", "0" },
      { "zoneId",     "Set zone ID", "0" }
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
    ZOPGen(SST::ComponentId_t id, SST::Params& params);

    /// ZOPGen: destructor
    ~ZOPGen();
    void init(unsigned int phase) override;
    void setup() override;
    void complete(unsigned int phase) override;
    void finish() override;
    void handleIncomingZOP(SST::Event *ev);
    void sendMsgToZOPGen(int i);
    void sendMsgToZOPGen2(int i);
    void sendSetupToZOPGen();
    void sendMZOPAckToZOPGen(SST::Forza::zopEvent *ev);
    void processMZOP(SST::Forza::zopEvent *ev);
    void processMZOPSDMA(SST::Forza::zopEvent *ev);
    void sendMZOPRespToZAP(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload);
    void sendMsgToRZA(uint64_t addr, uint64_t size);
    void processLoad(SST::Forza::zopEvent *ev);
    void sendLoadToRZA(uint64_t addr, uint64_t size) ;
    uint64_t getReadACS(uint64_t acs_pair) ;
    uint64_t getWriteACS(uint64_t acs_pair) ;
    void sendCreditsToZEN(int i);
  private:
    // private class members

    /// ZOPGen: clock handler
    bool clock(SST::Cycle_t cycle);

    // private data members
    SST::Output output;             ///< ZOPGen: SST output handler
    uint64_t int_id;
    uint8_t msg_id, cnt;
    uint64_t m_num_harts;
    SST::Forza::zopAPI* m_zop_iface;
    std::unordered_map<uint64_t, uint64_t> mem_map;
    bool sent, setup_done, all_sent, all_sent2;

    unsigned int num_loops1, num_loops2;

    // tests
    bool t_c1;
    bool t_c3;
    bool t_c5;
    UnitAlgebra t_p1;
    UnitAlgebra t_p2;
    UnitAlgebra t_p3;
    unsigned int t_p4;
    unsigned int t_p5;

    TimeConverter* zgTime;
  }; // class SST::ZOPGen
} // namespace SST::Forza

#endif // _ZOPGen_H_

// EOF
