//
// _zqm_h_
//

#ifndef _ZQM_H_
#define _ZQM_H_

#include "zqm_sst.h"
#include "ZOPNET.h"
#include "RingNet.h"
#include <string>
#include <bitset>

namespace SST::Forza{

inline constexpr uint8_t NUM_RECV_BUFFERS = 2;
inline constexpr uint64_t ACTOR_MSG_BYTES = 0x40;
inline constexpr uint16_t R_MASK_ZQMDQMBOX = 0b111;

// This probably needs to be
// expanded if we're not using DMA (right now, I'm forcing the use
// of DMA).  Might need to track my_id, parent_id, sequence_counter,
// originating zop
class MemReturnEntry {
public:
  SST::Forza::zopEvent* msg;
  std::vector<uint16_t> msg_ids;

  MemReturnEntry( SST::Forza::zopEvent* m, std::vector<uint16_t> v ) : msg( m ), msg_ids( v ) { /* empty constructor */ }
};

enum class msgBuffState : uint8_t {
    IDLE,
    FILLING,
    READY
};

// There will be one of these per mbox within a per-hart register
class ZqmMboxBuffer {
public:
  uint8_t                   cur_wr_entry{ 0 };
  uint8_t                   cur_rd_entry{ 0 };
  std::vector<msgBuffState> buff_state{ msgBuffState::IDLE };

  void updateWrEntry() {
    uint8_t tmp  = ( cur_wr_entry + 1 ) % buff_state.size();
    cur_wr_entry = tmp;
  }

  void updateRdEntry() {
    uint8_t tmp  = ( cur_rd_entry + 1 ) % buff_state.size();
    cur_rd_entry = tmp;
  }

  msgBuffState getCurWrState() const { return buff_state[cur_wr_entry]; }
  msgBuffState getCurRdState() const { return buff_state[cur_rd_entry]; }
  void setCurWrState(msgBuffState state) { buff_state[cur_wr_entry] = state; }
  void setCurRdState(msgBuffState state) { buff_state[cur_rd_entry] = state; }
};

class ZqmPerHartRegs {
public:
  uint16_t logical_pe{UINT16_MAX};
  uint8_t aid{UINT8_MAX};
  uint64_t mem_base_addr{UINT64_MAX};
  uint16_t msgs_per_mbox{UINT16_MAX};

  std::bitset<NUM_MBOXES> active_mboxes;
  std::array<ZqmMboxBuffer, NUM_MBOXES> mbox_buff_state;

  uint64_t getMsgBuffAddr( uint8_t mbox_id, uint8_t entry_num ) {
      uint64_t mbox_base_addr = mem_base_addr + ( mbox_id * msgs_per_mbox * ACTOR_MSG_BYTES );
      uint64_t entry_offset = entry_num * ACTOR_MSG_BYTES;
      return ( mbox_base_addr + entry_offset );
  }
};

class ZqmMboxInQueue {
    public:
    std::queue< std::pair<zopEvent*, uint16_t> > mbox_queue; // .second=retry cntr
    uint16_t head_check_cycle_cntr{0};
    bool head_ready{false};
};

class ZQM : public SST::Component {
public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
    ZQM,                                    // component class
    "forzazqm",                             // component libary
    "ZQM",                                  // component name
    SST_ELI_ELEMENT_VERSION(0,0,2), // Version of the component
    "ZQM: Forza ZQM component",             // description
    COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
    { "verbose",    "Sets the output verbsoity", "7" },
    { "clockFreq",  "ZQM core clock frequency", "1GHz" },
    { "numCores",         "Number of RISC-V cores (ZAPs)",                    "1" },
    { "numHarts",         "Number of harts (per core/ZAP) to instantiate",    "1" },
    { "precinctId",       "[FORZA] The precinct ID of the local device",      "0" },
    { "zoneId",           "[FORZA] The zone ID of the local device",          "0" },
    { "processPerCycle",  "[FORZA] Messages to process per cycle",            "10" },
    { "msgQueueDepth",    "[FORZA] Depth of the incoming message queue",      "512" },
    { "memStartAddr",     "[FORZA] Start address for message buffers",        "0x100000" }, // expected to be 0x400 aligned
    { "msgsPerMbox",      "[FORZA] Number of messages per mailbox per Hart",  "2" },
    { "cyclesPerRecycle", "[FORZA] Cycles before recycling incoming msg",     "1024"}, // TODO: check w/RTL folks
    { "recyclesToNack",   "[FORZA] Recycles before NACKing incoming msg",     "64"} // TODO: check w/RTL folks
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS()

    // describe the statistics
    // TODO: Add stats
    SST_ELI_DOCUMENT_STATISTICS()

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"zone_nic", "[FORZA] Zone NIC", "SST::Forza::zopNIC"},
    {"ring_nic", "[FORZA] Zone Ring Network", "SST::Forza::RingNetNIC"}
    )

    // public class members
    /// ZQM: constructor
    ZQM(SST::ComponentId_t id, SST::Params& params);

    /// ZQM: destructor
    ~ZQM();
    void init(unsigned int phase) override;
    void setup() override;
    void complete(unsigned int phase) override;
    void finish() override;


    // Ring message handling functions
    void handleRingMsg( SST::Event *event );
    void handleRingStatus( SST::Forza::ringEvent *ev );
    void handleRingMboxReg( SST::Forza::ringEvent *ev );
    void handleRingDq( SST::Forza::ringEvent *ev );

    void sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data );

    void updateInMboxQueue();
    void selectInMboxQueue();

    /**
     * Ok, this is the top level zop handler, there are separate functions
     * for handling each subtype (they are named as "process...)"
     */
    void handleIncomingZOP(SST::Event *ev);

private:

    // private data members
    SST::Output output;             ///< ZQM: SST output handler

    // Parameters to maintain
    std::string clockFreq;
    unsigned numCores;
    uint16_t numHarts;
    unsigned PrecinctId;
    unsigned ZoneId;
    unsigned processPerCycle;
    uint32_t msgQueueDepth;
    uint64_t memStartAddr;
    uint32_t msgsPerMbox;
    uint32_t cyclesPerRecycle;
    uint32_t recyclesToNack;

    std::string my_name;
    SST::Forza::zopAPI* zone_nic{};      ///< ZQM: zone zop NoC
    zopMsgID *zoneMsgID{};               ///< ZQM: manually allocated message IDs
    SST::Forza::RingNetAPI* zone_ring{}; ///< ZQM: zone csr ring network

    // From ZEN
    bool dma_enabled;

    // Internal structures - ring architecture
    // key pair<aid, logical PE>, value pair<zap, phys_hart>
    std::map<std::pair<uint8_t, uint16_t>, std::pair<uint8_t, uint16_t>> LogicalToPhysicalMap;
    std::vector<std::vector<ZqmPerHartRegs>> PerHartCSRs;

    std::vector<ZqmMboxInQueue> IncomingMsgQueues;

    std::queue<uint8_t> AwaitingThreadsQueue; // ZAPs waiting for threads
    std::queue<SST::Forza::zopEvent*> RunQueue; // threads waiting for an available HART

    // Structures for holding messages - really ought to rename these and make them camelcase
    std::queue<SST::Forza::zopEvent*> msg_zop_q;
    std::queue<SST::Forza::zopEvent*> tmig_zop_q;
    std::queue<SST::Forza::zopEvent*> rza_response_q;
    //std::queue<SST::Forza::zopEvent*> to_rza_q;
    //std::map<uint16_t, MemReturnEntry*> rza_ret_wait_map;

    // Internal state
    uint8_t num_incoming_msg_queues_ready{};
    uint8_t cur_incoming_msg_queue{}; // Incoming msg queue to process next (round-robin arbitration)
    uint8_t process_incoming_msg_queue{}; // Select a msg queue to process

    /// ZQM: clock handler
    bool clock(SST::Cycle_t cycle);

    void processMessagingMsgs(); // invoked by clock handler

    void sendZopAck(SST::Forza::zopEvent *event, zopMsgT msg_type, zopOpc msg_opc);

    void processRzaMsgs();  // invoked by clock handler
    //void processRzaThreadDataReturn(SST::Forza::zopEvent *ev);

    void convertLogicPEToPhysPE( zopEvent* msg );

    void updateCurIncomingMsgQueue() {
        uint8_t tmp = ( cur_incoming_msg_queue + 1 ) % NUM_MBOXES;
        cur_incoming_msg_queue = tmp;
    }

    void updateProcessIncomingMsgQueue() {
        uint8_t tmp = ( process_incoming_msg_queue + 1 ) % NUM_MBOXES;
        process_incoming_msg_queue = tmp;
    }

    // tdysart, 27-june-2024 removing thread management for now
#if 0
    void sendThreadToRza(SST::Forza::zopEvent *thread);
    void getThreadFromRza(uint32_t app_id);
#endif

    /**
     * Read and process incoming TMIG messages
     *
     */
    void processTMigMsgs(); //invoked by clock handler

    /**
     * If a ZAP has a hart, try to fill it
     */
    void sendThreadToZap();



    // Functions copied over from the ZEN
#if 0    
    void prepSendRZAStore();
    void sendSdmaToRza(ZqmMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                        std::vector<uint64_t> store_payload, uint16_t msg_id,
                        uint64_t wr_addr);
#endif

    void sendMsgToMemory( SST::Forza::zopEvent* ev, uint64_t wr_addr, uint8_t wr_entry );

    void setMeAsZopSrc(SST::Forza::zopEvent *ev)
    {
        ev->setSrcHart(0);
        ev->setSrcZCID(SST::Forza::zopCompID::Z_ZQM);
        ev->setSrcPCID(ZoneId);
        ev->setSrcPrec(PrecinctId);
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

    // Goes to RZA1 since that's the memory component used by the ZQM
    void setLocalRzaAsZopDest(SST::Forza::zopEvent *ev)
    {
        ev->setDestHart(Z_MZOP_PIPE_HART);
        ev->setDestZCID(SST::Forza::zopCompID::Z_RZA1);
        ev->setDestPCID(ZoneId);
        ev->setDestPrec(PrecinctId);
    }

}; // class SST::ZQM
} // namespace SST::Forza

#endif // _ZQM_H_

// EOF
