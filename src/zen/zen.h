//
// _zen_h_
//


#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "ZOPNet.h"
#include <string>
#include <bitset>

namespace SST::Forza{

  enum ZENStatus : uint64_t {
    UNPROCESSED,
    RZA_ADDR_ASSIGNED,
    MZOP_SENT,
    MZOP_ACK_PROCESSED,
    DONE,
    RZA_ADDR_ERROR
  };

  class ZENEntry {
  public:
    SST::Forza::zopEvent *msg;
    ZENStatus status;
    uint64_t tail;
    std::vector<uint8_t> msg_ids;
    uint64_t rza_start_addr;
    bool from_zip;
    ZENEntry(SST::Forza::zopEvent *m, ZENStatus s, bool src_zip) {
      msg = m;
      status = s;
      tail = 0;
      from_zip = src_zip;
    }
  };

  class ZENTableRow {
  public:
    uint64_t acs_pair;
    uint64_t mem_head;
    uint64_t mem_tail;
    uint64_t mem_size;
    uint64_t mem_cur_head;
    uint64_t mem_cur_tail;
    bool empty;
    uint64_t scratch_tail;
    uint64_t credits;
    ZENTableRow(uint64_t acs, uint64_t mh, uint64_t mt, uint64_t ms, uint64_t st, uint64_t c) :
      acs_pair(acs), mem_head(mh), mem_tail(mt), mem_size(ms), empty(true), scratch_tail(st), credits(c) {
        mem_cur_head = mh;
        mem_cur_tail = mh;
        empty = true;
      }
  };
  class ZEN : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZEN,                                    // component class
      "Forza",                             // component libary
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
    /// ZEN: constructor
    ZEN(SST::ComponentId_t id, SST::Params& params);

    /// ZEN: destructor
    ~ZEN();
    void init(unsigned int phase) override;
    void setup() override;
    void complete(unsigned int phase) override;
    void finish() override;
    void sendMsgToRZADMA(uint64_t acs, uint64_t addr, std::vector<uint64_t> src_payload, uint8_t msg_id, uint64_t hart_id, uint64_t queue_loc);
    void sendMsgToRZANonDMA(uint64_t acs, uint64_t addr, uint64_t src_payload, uint8_t cur_msg_id, uint64_t hart_id, uint64_t queue_loc);
    void sendMsgToScratchpad(uint64_t dest, uint64_t zcid, uint64_t scratch_addr, uint64_t size, uint64_t addr);
    void processEgressQueue();
    void processPrecinctEgressQueue();
    void processZoneEgressQueue();
    void notifyHARTScratchpad();
    void handleIncomingRZAMsg();
    void handleIncomingZOP(SST::Event *ev);
    void sendNACKToZAP(uint64_t hart_id, uint64_t zcid);
    void sendACKToZAP(uint64_t hart_id, uint64_t zcid);
    void sendNACKToZIP(uint64_t hart_id, uint64_t zcid);
    void sendACKToZIP(uint64_t hart_id, uint64_t zcid);
    void processSetupMsgs();
    void processZAPCredits();
    void sendMsgToZEN();
    void sendSetupToZEN();
    int getRZATailQueue(uint64_t harts, uint64_t size);
    void sendMZOPAckToZEN(SST::Forza::zopEvent *ev);
    uint64_t  getReadACS(uint64_t);
    uint64_t  getWriteACS(uint64_t);
    void printZenQueue();
    uint64_t findFirstUnsetBit(const std::bitset<256>& bv);
    void prepSendRZAHZOP();
    void prepSendRZAStore();
    void sendHZOPToRZA(uint64_t acs, uint64_t addr, uint64_t src_addr, uint64_t size, uint8_t cur_msg_id, uint64_t hart_id, uint64_t queue_loc);
    void forwardPktToZIP(Forza::zopEvent *ev);
    void forwardPktToExtZEN(Forza::zopEvent *ev);

  private:
    // private class members

    /// ZEN: clock handler
    bool clock(SST::Cycle_t cycle);

    bool handleNetworkEvent(int i);

    // private data members
    SST::Output output;             ///< ZEN: SST output handler
    SST::Interfaces::SimpleNetwork*     m_linkControl;
    std::map<uint64_t, ZENTableRow*> hart_tables;
    std::map<uint64_t, ZENTableRow*> zone_tables;
    std::map<uint64_t, ZENTableRow*> precinct_tables;
    std::map<uint64_t, std::vector<ZENEntry*> > zen_queue;
    std::map<uint64_t, std::vector<ZENEntry*> > zone_queue;
    std::map<uint64_t, std::vector<ZENEntry*> > precinct_queue;
    std::vector<SST::Forza::zopEvent*> mem_acks;
    std::vector<SST::Forza::zopEvent*> setup_reqs;
    std::vector<SST::Forza::zopEvent*> zap_credits;
    std::map<uint8_t, std::pair<uint64_t, uint64_t> > outstanding_mem_req;
    uint64_t int_id;
    std::bitset<256> msg_id;
    uint64_t m_num_harts;
    uint64_t m_num_zones;
    uint64_t m_num_precincts;
    SST::Forza::zopAPI* m_zop_iface;
    bool sent;
    bool dma_enabled;
    uint64_t process_per_cycle;
    uint64_t zen_queue_size_limit;
  }; // class SST::ZEN
} // namespace SST::Forza

#endif // _ZEN_H_

// EOF
