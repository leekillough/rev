//
// _zopgen_h_
//


#ifndef _ZOPGen_H_
#define _ZOPGen_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>

namespace SST::Forza{
  class ZOPGenEntry {
  public:
    SST::Forza::zopEvent *msg;
    uint64_t status;
    ZOPGenEntry(SST::Forza::zopEvent *m, uint64_t s) {
      msg = m;
      status = s;
    }
  };

  class ZOPGenTableRow {
  public:
    uint64_t mem_head;
    uint64_t mem_tail;
    uint64_t mem_size;
    uint64_t mem_cur_head;
    uint64_t mem_cur_tail;
    bool rev;
    uint64_t scratch_tail;
    uint64_t scratch_cur_tail;
    uint64_t credits;
    ZOPGenTableRow(uint64_t mh, uint64_t mt, uint64_t ms, bool rev, uint64_t st, uint64_t sh, uint64_t c) :
      mem_head(mh), mem_tail(mt), mem_size(ms), rev(rev), scratch_tail(st), scratch_cur_tail(sh), credits(c) {
        mem_cur_head = mh;
        mem_cur_tail = mh;
      }
  };
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
      { "verbose",    "Sets the output verbsoity", 0 }
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS(
      {"rtrLink", "Links to Network.", {"merlin.linkcontrol"}},
    )

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"m_zop_iface","[FORZA] Zone NIC", "SST::Forza::zenZopNIC"},
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
    void sendMsgToRZA(uint64_t addr, uint8_t msg_id);
    void sendMsgToScratchpad(uint64_t dest, uint64_t addr);
    void processEgressQueue();
    void notifyHARTScratchpad();
    void handleIncomingRZAMsg();
    void handleIncomingZOP(SST::Event *ev);
    void sendNACKToZAP(uint64_t hart_id);
    void sendACKToZAP(uint64_t hart_id);
    void processSetupMsgs();
    void processZAPCredits();
    void sendMsgToZOPGen(int i);
    void sendSetupToZOPGen();
    uint64_t getRZATailQueue(uint64_t harts, uint64_t size);
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

    bool handleNetworkEvent(int i);

    // private data members
    SST::Output output;             ///< ZOPGen: SST output handler
    SST::Interfaces::SimpleNetwork*     m_linkControl;
    std::map<uint64_t, ZOPGenTableRow*> hart_tables;
    std::map<uint64_t, std::vector<ZOPGenEntry*> > zopgen_queue;
    std::vector<SST::Forza::zopEvent*> mem_acks;
    std::vector<SST::Forza::zopEvent*> setup_reqs;
    std::vector<SST::Forza::zopEvent*> zap_credits;
    std::map<uint8_t, std::pair<uint64_t, uint64_t> > outstanding_mem_req;
    uint64_t int_id;
    uint8_t msg_id, cnt;
    uint64_t m_num_harts;
    SST::Forza::zopAPI* m_zop_iface;
    std::unordered_map<uint64_t, uint64_t> mem_map;
    bool sent, setup_done, all_sent;
  }; // class SST::ZOPGen
} // namespace SST::Forza

#endif // _ZOPGen_H_

// EOF
