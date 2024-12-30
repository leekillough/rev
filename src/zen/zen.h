//
// _zen_h_
//

#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include "RingNet.h"
#include <string>
#include <bitset>
#include <queue>

constexpr uint32_t ZenSeqNumMgrDepth = 8192;  // if we change this depth, need to update shift and/or mask
static constexpr uint64_t BytesPerActorMsg = 64;

namespace SST::Forza{

  class ZenPerHartRegs {
    public:
      // Note: may need to add some other status variables, etc in here
      uint64_t status{ 1UL << 63 }; //default to turning on the enabled bit
      std::array<uint64_t, ACTOR_MSG_LENGTH> msg{}; // word 0 is control, 1-7 are data
      std::array<uint8_t, NUM_MBOXES> mbox_cntrs{};
      uint8_t msg_cur_word{1};
      bool is_sending{}; //signifies that the hart is in the process of sending a message; OR'd with mbox_busy portion of status register when status is read 
      std::array<uint64_t, 2> spawn_thread{};
      uint8_t spawn_cur_word{0};

    void allowNextMsg() { this->is_sending = false; this->msg_cur_word = 1; }
  };

class OutgoingSpawn {
  public:
    OutgoingSpawn( std::array<uint64_t, 2> data, uint8_t zap, uint16_t hart ) :
      thread(data), src_zap(zap), src_hart(hart)
      { /* empty */ }

    // More info needed?
    std::array<uint64_t, 2> thread{};
    uint8_t src_zap;
    uint16_t src_hart;
    uint8_t aid{UINT8_MAX}; // WHERE FROM?
};

class seqNumEntry {
  public:
    bool in_use{ false };
    bool acked{ false };
    bool written{ false };
    bool outstanding_read{ false };
    uint8_t src_zap{ UINT8_MAX };
    uint16_t src_hart{ UINT16_MAX };

  void set() {
    in_use = true; acked = written = outstanding_read = false;
  }

  void setMsgSrc( uint8_t zap, uint16_t hart ) {
    src_zap = zap;
    src_hart = hart;
  }

  void clear() {
    in_use = acked = written = outstanding_read = false;
    src_zap = UINT8_MAX;
    src_hart = UINT16_MAX;
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
      { "memStartAddr",      "[FORZA] Start address for message buffers",    "0x1000" }, // expected to be 0x400 aligned
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS()

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      {"zone_nic", "[FORZA] Zone NIC", "SST::Forza::zopNIC"},
      {"precinct_nic", "[FORZA] Precinct NIC", "SST::Forza::zopNIC"},
      {"ring_nic", "[FORZA] Zone Ring Network", "SST::Forza::RingNetNIC"}
    )

    // public class members
    /// ZEN: constructor
    ZEN(SST::ComponentId_t id, SST::Params& params);

    /// ZEN: destructor
    ~ZEN();

    /// ZEN: init function
    void init( unsigned int phase ) override;

    /// ZEN: setup function
    void setup() override;

    /// ZEN: completion function
    void complete( unsigned int phase ) override;

    /// ZEN: finish function
    void finish() override;

    /// ZEN: clock handler
    bool clock(SST::Cycle_t cycle);

  private:
    // Ring based functions
    void handleRingMsg( SST::Event *event );
    void handleRingOmc( SST::Forza::ringEvent *ev );
    void handleRingStatus( SST::Forza::ringEvent *ev );
    void handleRingEqData( SST::Forza::ringEvent *ev );
    void handleRingEqCtrl( SST::Forza::ringEvent *ev );
    void handleRingSpawn( SST::Forza::ringEvent *ev );
    

    void sendRingResponse( SST::Forza::ringEvent* ev, uint64_t data );

    uint16_t getRetrySeqNum();
    void ExecMsgPipeline();
    void execMsgPipe0();
    void execMsgPipe1();
    void execMsgPipe2();
    void updateMsgPipe0();

    void handleMsgResp(zopEvent *ack);
    void sendMsgToMemory( zopEvent* out_msg );

    void ExecSpawns();

    zopEvent* createMsgZop( std::array<uint64_t, ACTOR_MSG_LENGTH> msg_data, ringEvent* ring_event );

    /// ZEN: handle incoming RZA messages
    void processIncomingRZAMsgs();

    /// ZEN: handle incoming ZOP messages (from zone NoC)
    void handleIncomingZOP(SST::Event *ev);

    /// ZEN: handle incoming precinct ZOP messages (from precinct NoC)
    void handleIncomingPrecZOP(SST::Event *ev);

    /// ZEN: helper functions for zop types entering from zone noc
    void helper_handleMsgZop(SST::Forza::zopEvent *ev);

    /// ZEN: determine if zop dest precinct and zone match me
    bool isDestLocal(SST::Forza::zopEvent *ev)
    {
      if ( (ev->getDestPCID() == ZoneId) &&
           (ev->getDestPrec() == PrecinctId) )
          return true;
      return false;
    }

    bool isSrcLocal(SST::Forza::zopEvent *ev)
    {
      if ( (ev->getSrcPCID() == ZoneId) &&
           (ev->getSrcPrec() == PrecinctId) )
          return true;
      return false;
    }

    void setMeAsZopSrc(SST::Forza::zopEvent *ev)
    {
      //SST::Forza::zopAPI *iface = isSrcLocal(ev) ? zone_nic : m_prec_iface;
      ev->setSrcHart(0);
      ev->setSrcZCID(SST::Forza::zopCompID::Z_ZEN);
      ev->setSrcPCID(ZoneId);
      ev->setSrcPrec(PrecinctId);
    }

    void setLocalRzaAsZopDest(SST::Forza::zopEvent *ev)
    {
      ev->setDestHart(Z_MZOP_PIPE_HART);
      ev->setDestZCID(SST::Forza::zopCompID::Z_RZA1);
      ev->setDestPCID(ZoneId);
      ev->setDestPrec(PrecinctId);
    }

    void setLocalZqmAsZopDest(SST::Forza::zopEvent *ev)
    {
      ev->setDestHart(0);
      ev->setDestZCID(SST::Forza::zopCompID::Z_ZQM);
      ev->setDestPCID(ZoneId);
      ev->setDestPrec(PrecinctId);
    }

    void setDestFromSrcInfo(SST::Forza::zopEvent *dest_packet, SST::Forza::zopEvent *src_packet)
    {
        dest_packet->setDestHart(src_packet->getSrcHart());
        dest_packet->setDestZCID(src_packet->getSrcZCID());
        dest_packet->setDestPCID(src_packet->getSrcPCID());
        dest_packet->setDestPrec(src_packet->getSrcPrec());
    }

    void setDestFromDestInfo(SST::Forza::zopEvent *dest_packet, SST::Forza::zopEvent *src_packet)
    {
        dest_packet->setDestHart(src_packet->getDestHart());
        dest_packet->setDestZCID(src_packet->getDestZCID());
        dest_packet->setDestPCID(src_packet->getDestPCID());
        dest_packet->setDestPrec(src_packet->getDestPrec());
    }

    uint64_t getRetryBuffAddr( uint16_t idx ) {
      uint64_t incr = ( idx * BytesPerActorMsg );
      return ( memStartAddr + incr );
    }

    // private data members
    SST::Output output;                    ///< ZEN: SST output handler
    SST::Forza::zopAPI* zNic{};            ///< ZEN: ZOP Network interfaces for zone network
    SST::Forza::zopMsgID *zoneMsgID{};     ///< ZEN: manually allocated message IDs
    SST::Forza::zopAPI* precNic{};         ///< ZEN: ZOP Network interfaces for precinct network
    SST::Forza::RingNetAPI* zone_ring{};   ///< ZEN: zone CSR network

    // ----- BEGIN SST PARAMETERS
    unsigned PrecinctId;            ///< ZEN: Precinct ID
    unsigned ZoneId;                ///< ZEN: Zone ID
    unsigned numHarts;              ///< ZEN: number of harts
    unsigned numZaps;               ///< ZEN: number of zaps
    unsigned numZones;              ///< ZEN: number of zones
    unsigned numPrecincts;          ///< ZEN: number of precincts
    bool dma_enabled;               ///< ZEN: enable DMA operations
    uint64_t zen_queue_size_limit;  ///< ZEN: zen queue size limit
    uint64_t process_per_cycle;     ///< ZEN: messages to process per cycle
    uint64_t memStartAddr;          ///< ZEN: start addr for retry queue
    // ----- END SST PARAMETERS

    // Internal data structures
    std::vector<std::vector<ZenPerHartRegs>> PerHartCSRs;
    std::vector<seqNumEntry> SeqNumMgrList;
    std::queue<zopEvent*> OutMsgQueue;
    std::array<zopEvent*, 3> MsgPipeline{};
    std::queue<zopEvent*> MsgAckQueue;
    std::queue<zopEvent*> RzaRespQueue;
    std::queue<OutgoingSpawn*> OutSpawnQueue;
    std::queue<zopEvent*> NackRdReqQueue;

    // This map is because we don't have a LoadDMA option in memory right now
    // thus, this stores the message and we'll get the payload from here
    // on the read return
    std::map<uint32_t, std::vector<uint64_t> > MsgDataMap;

    // Other counters, etc
    uint64_t seqNumsAvail;
    bool pipe2_alternate{false};

  }; // class SST::ZEN
} // namespace SST::Forza

#endif // _ZEN_H_

// EOF
