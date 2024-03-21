//
// _zen_h_
//


#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>
#include <bitset>

#define _ZEN_DEFAULT_ZIP_CREDITS_   100

namespace SST::Forza{

  // --------------------------------------------
  // ZENStatus
  // --------------------------------------------
  enum ZENStatus : uint8_t {
    UNPROCESSED         = 0x00,
    RZA_ADDR_ASSIGNED   = 0x01,
    MZOP_SENT           = 0x02,
    MZOP_ACK_PROCESSED  = 0x03,
    DONE                = 0x04,
    RZA_ADDR_ERROR      = 0x05,
  };

  // --------------------------------------------
  // ZENEntry
  // --------------------------------------------
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

  // --------------------------------------------
  // ZENTableRow
  // --------------------------------------------
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

  // --------------------------------------------
  // ZEN
  // --------------------------------------------
  class ZEN : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZEN,                                    // component class
      "forzazen",                             // component libary
      "ZEN",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZEN: Forza ZEN component",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "clockFreq",         "ZEN core clock frequency",                     "1GHz" },
      { "verbose",           "Sets the output verbsoity",                    0 },
      { "precinctId",        "[FORZA] The precinct ID of the local device",  "0"},
      { "zoneId",            "[FORZA] The zone ID of the local device",      "0"},
      { "numHarts",          "[FORZA] Number of Harts",                      "512"},
      { "numZaps",           "[FORZA] Number of ZAPS",                       "4"},
      { "numZones",          "[FORZA] Number of Zones",                      "8"},
      { "numPrecincts",      "[FORZA] Number of Precincts",                  "4"},
      { "enableDMA",         "[FORZA] Enable DMA operations",                "0"},
      { "enablePrecinctNIC", "[FORZA] Enable Precinct NIC",                  "0"},
      { "zenQSizeLimit",     "[FORZA] ZEN Queue Size Limit",                 "100000"},
      { "processPerCycle",   "[FORZA] Messages to process per cycle",        "100000"},
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS()

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"zone_nic", "[FORZA] Zone NIC", "SST::Forza::zopNIC"},
      {"precinct_nic", "[FORZA] Precinct NIC", "SST::Forza::zopNIC"},
    )

    // public class members
    /// ZEN: constructor
    ZEN(SST::ComponentId_t id, SST::Params& params);

    /// ZEN: destructor
    ~ZEN();

    /// ZEN: init function
    void init(unsigned int phase);

    /// ZEN: setup function
    void setup();

    /// ZEN: completion function
    void complete(unsigned int phase);

    /// ZEN: finish function
    void finish();

    /// ZEN: clock handler
    bool clock(SST::Cycle_t cycle);

  private:
    // private class members

    /// ZEN: Send a NACK message back to the to target device
    void sendNACK(uint16_t hart, uint8_t zcid,
                  uint8_t pcid, uint16_t prec, uint8_t id,
                  SST::Forza::zopAPI *iface);

    /// ZEN: Send an ACK message back to the to target device
    void sendACK(uint16_t hart, uint8_t zcid,
                  uint8_t pcid, uint16_t prec, uint8_t id,
                  SST::Forza::zopAPI *iface);

    /// ZEN: send a DMA store to the zone's RZA
    void sendMsgToRZADMA(uint64_t acs, uint64_t addr,
                         std::vector<uint64_t> src_payload, uint8_t cur_msg_id,
                         uint64_t hart_id, uint64_t queue_loc);

    /// ZEN: send a normal store operation to the zone's RZA
    void sendMsgToRZANonDMA(uint64_t acs, uint64_t addr, uint64_t src_payload,
                            uint8_t cur_msg_id, uint64_t hart_id,
                            uint64_t queue_loc);

    /// ZEN: sends a scratchpad WRITE to the target ZAP device
    void sendMsgToScratchpad(uint64_t dest, uint64_t zcid,
                             uint64_t scratch_addr, uint64_t size,
                             uint64_t addr);

    /// ZEN: processes the egress queue
    void processEgressQueue();

    /// ZEN: process the precinct egress queue
    void processPrecinctEgressQueue();

    /// ZEN: process the zone egress queue
    void processZoneEgressQueue();

    /// ZEN: processes messages to perform writes to a ZAP:HART Scratchpad
    void notifyHARTScratchpad();

    /// ZEN: handle incoming RZA messages
    void handleIncomingRZAMsg();

    /// ZEN: handle incoming ZOP messages
    void handleIncomingZOP(SST::Event *ev);

    /// ZEN: handle incoming precinct ZOP messages
    void handleIncomingPrecZOP(SST::Event *ev);

    /// ZEN: processes incoming setup messages
    void processSetupMsgs();

    /// ZEN: process the ZEN credits
    void processZAPCredits();

    /// ZEN: retrieves an RZA tail queue address
    int getRZATailQueue(uint64_t zap_id, uint64_t harts,
                        uint64_t size, uint64_t *taddr);

    /// ZEN: retrieve the read ACS
    uint64_t  getReadACS(uint64_t);

    /// ZEN: retrieve the write ACS
    uint64_t  getWriteACS(uint64_t);

    /// ZEN: preps to send the RZA an HZOP (really, a DMA MZOP)
    void prepSendRZAHZOP();

    /// ZEN: preps to send the RZA a STORE
    void prepSendRZAStore();

    /// ZEN: send HZOPs to the RZA
    void sendHZOPToRZA(uint64_t acs, uint64_t addr, uint64_t src_addr,
                       uint64_t size, uint8_t cur_msg_id,
                       uint64_t hart_id, uint64_t queue_loc);

    /// ZEN: forwards a packet to the precinct ZIP device
    void forwardPktToZIP(Forza::zopEvent *ev);

    /// ZEN: forwards a packet to a remote ZEN device
    void forwardPktToExtZEN(Forza::zopEvent *ev);

    /// ZEN: processes the zip message queue
    void processZIPQueue();

    // private data members
    SST::Output output;                   ///< ZEN: SST output handler
    SST::Forza::zopAPI* m_zop_iface;      ///< ZEN: ZOP Network interfaces for zone network
    SST::Forza::zopAPI* m_prec_iface;     ///< ZEN: ZOP Network interfaces for precinct network

    // ----- BEGIN SST PARAMETERS
    unsigned Precinct;              ///< ZEN: Precinct ID
    unsigned Zone;                  ///< ZEN: Zone ID
    unsigned m_num_harts;           ///< ZEN: number of harts
    unsigned m_num_zaps;            ///< ZEN: number of zaps
    unsigned m_num_zones;           ///< ZEN: number of zones
    unsigned m_num_precincts;       ///< ZEN: number of precincts
    bool dma_enabled;               ///< ZEN: enable DMA operations
    uint64_t zen_queue_size_limit;  ///< ZEN: zen queue size limit
    uint64_t process_per_cycle;     ///< ZEN: messages to process per cycle
    bool precinct_nic_enabled;      ///< ZEN: enable precinct NIC
    // ----- END SST PARAMETERS

    zopMsgID *zoneMsgID;            ///< ZEN: manually allocated message IDs

    uint64_t zip_credits;
    std::map<std::pair<uint64_t, uint64_t>, ZENTableRow*> hart_tables;
    std::map<uint64_t, ZENTableRow*> zone_tables;
    std::map<uint64_t, ZENTableRow*> precinct_tables;
    std::map<std::pair<uint64_t, uint64_t>, std::vector<ZENEntry*>> zen_queue;
    std::map<uint64_t, std::vector<ZENEntry*> > zone_queue;
    std::map<uint64_t, std::vector<ZENEntry*> > precinct_queue;
    std::vector<SST::Forza::zopEvent*> mem_acks;
    std::vector<SST::Forza::zopEvent*> setup_reqs;
    std::vector<SST::Forza::zopEvent*> zap_credits;
    std::map<uint8_t, ZENEntry*> outstanding_mem_req;

    std::queue<SST::Forza::zopEvent*> zipQ;

  }; // class SST::ZEN
} // namespace SST::Forza

#endif // _ZEN_H_

// EOF
