//
// _zqm_h_
//

/** STATUS **/
/**
 * 20-nov-2024: some support for spawns and the run queue (have been prioritizing messaging), but mostly
 * commented out.  currently this will only handle receiving messages and send
 * acks.  We don't actually send any memory zops for handling messages - everything is handled by
 * structures here in the ZQM (moving these messages to memory is next on the hit list)
 */

#ifndef _ZQM_H_
#define _ZQM_H_

#include "zqm_sst.h"
#include "ZOPNET.h"
#include "RingNet.h"
#include <string>
#include <bitset>

namespace SST::Forza{

#define NUM_RECV_BUFFERS 2
#define MSG_DEPTH 8

inline constexpr uint64_t ACTOR_MSG_BYTES = 0x40;

#define R_MASK_ZQMDQMBOX  0x07

    // This probably needs to be 
    // expanded if we're not using DMA (right now, I'm forcing the use
    // of DMA).  Might need to track my_id, parent_id, sequence_counter, 
    // originating zop
    class MemReturnEntry {
        public:
        SST::Forza::zopEvent *msg;
        std::vector<uint16_t> msg_ids;
        MemReturnEntry(SST::Forza::zopEvent *m, std::vector<uint16_t>v) :
            msg(m),
            msg_ids(v)
            { /* empty constructor */}
    };

#if 0
    // --------------------------------------------
    // ZqmMailboxMetadata
    // TODO: For sanity checking, we need the packet
    // size for this mailbox and ensure the buffer
    // pointers align on that boundary - software isn't going
    // to be happy if a full packet isn't in contiguous memory
    // Oh, and remember that we store the full ZOP in memory so 
    // we can return a credit
    // --------------------------------------------
    class ZqmMailboxMetadata {
    public:
        uint64_t acs_pair;
        uint64_t mem_head;
        uint64_t mem_tail;
        uint64_t mem_size;
        uint64_t mem_cur_head;
        uint64_t mem_cur_tail; //sent to scratchpad
        uint64_t mem_wr_ptr;  //used to send SDMA packets
        bool empty;
        uint64_t scratch_tail;
        uint8_t app_id;
        uint8_t mbox_id;
        uint8_t zap_id;
        uint16_t hart_id;
        ZqmMailboxMetadata(uint64_t acs, uint64_t mh, uint64_t mt, uint64_t ms, 
                        uint64_t st, uint8_t app, uint8_t mbox, uint8_t zap,
                        uint16_t hart) :
        acs_pair(acs), 
        mem_head(mh), 
        mem_tail(mt), 
        mem_size(ms),
        mem_cur_head(mh),
        mem_cur_tail(mh),
        mem_wr_ptr(mh),      
        empty(true), 
        scratch_tail(st), 
        app_id(app),
        mbox_id(mbox),
        zap_id(zap),
        hart_id(hart)
        { /* empty constructor */}

        /// @brief  Get current wr ptr and update it to the next addr
        /// @param size packet size we're writing
        /// @return write address (aka mem_wr_ptr)
        uint64_t getRzaWriteAddr(uint8_t size);

        // Pretty much the same as above, but for mem_cur_tail
        uint64_t getSpTailAddr(uint8_t size);
    };
#endif


#if 0
    class ZqmAidStateTableRow {
        // Will need functionality for updating read/write pointers
        /**
         * NOTE: This will NOT fill the circular buffer
         * if (mem_read_ptr == mem_write_ptr), buffer is empty
         * if ( (mem_write_ptr + thread_length_bytes) >= mem_read_ptr), buffer has space for one thread
         *   but then the pointers would match (and thus be "empty"), so no write allowed
         *
         *   run_queue_depth is {in,de}cremented when the memory {write,read} is issued
         *
         *   mem_buffer_high is assumed to be the last byte allowed in the buffer; this address+1
         *   would be owned by something else
         *     For example, if we want a buffer of 256 bytes per thread, then buffer_low should end in 0x00 and
         *     buffer_high should end in 0xff.
         *
         */
    public:
        uint32_t min_zap_hart;
        uint32_t max_zap_hart;
        uint64_t mem_read_ptr;
        uint64_t mem_write_ptr;
        uint64_t mem_buffer_low;
        uint64_t mem_buffer_high;
        bool sequential_hart_assignment;
        int32_t harts_available;
        int32_t run_queue_depth;
        int32_t outstanding_fills;
        // TODO: Add read and write ACS; may need to add to ZOP spec/payload

        // Note: No valid data element - assuming that if the row exists, it's valid
        ZqmAidStateTableRow(uint32_t min_zap_hart_, uint32_t max_zap_hart_,
                            uint64_t mem_buffer_low_addr_, uint64_t mem_buffer_high_addr_,
                            bool sequential_hart_assignment_, uint16_t numCores_) :
                min_zap_hart(min_zap_hart_),
                max_zap_hart(max_zap_hart_),
                mem_read_ptr(mem_buffer_low_addr_),
                mem_write_ptr(mem_buffer_low_addr_),
                mem_buffer_low(mem_buffer_low_addr_),
                mem_buffer_high(mem_buffer_high_addr_),
                sequential_hart_assignment(sequential_hart_assignment_),
                run_queue_depth(0),
                outstanding_fills(0)
        {
            // Note: Validation of buffer size is a separate function (hacky, but can use
            // regular SST output then)
            harts_available = (max_zap_hart - min_zap_hart + 1) * numCores_;
        }

        /**
         * If doing an update, then we verify that there is something to read
         * or space to write
         * @param do_read -- true on read, false on write
         * @param update_ptr -- true updates ptr, false otherwise
         * @return - desired ptr or 0 on validation fail
         */
        uint64_t getMemAddr(bool do_read, bool update_ptr);

        bool validateMemBuffSize();
    }; // end class ZqmAidStateTableRow
#endif


/**
 * So here's what I'm going to do for now - incoming messaging zop comes in
 * and we'll just dump into a queue.  Each cycle, we'll pull the head of the
 * queue and see if it's hart has an open message buffer - if so, we'll put it
 * in there; if not, we'll just recycle it.

 * Now, on the zap side, we'll start by doing something similar to what the
 * zen does; basically it just does a sequence of CSR reads and gets one word back
 * at a time. Easy enough to update the hclib code to do something else down the 
 * road 
 * 
 * Overall, this approach should let me stand this unit up pretty quickly and then
 * we can continue to add additional details as available 
 */

enum class msgBuffState : uint8_t {
    IDLE,
    FILLING,
    READY
};

// TODO: Delete class once msgs go to memory
class ZqmMsgBuffer {
public:
    void setMsg( std::vector<uint64_t> P ){
        msg.clear();
        for (auto i : P)
            msg.push_back(i);
    }

    std::vector<uint64_t> msg;
    uint8_t msg_cur_word{1};
    msgBuffState buff_state{msgBuffState::IDLE};
    uint64_t mem_addr;
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

  msgBuffState getCurWrState() { return buff_state[cur_wr_entry]; }
  msgBuffState getCurRdState() { return buff_state[cur_rd_entry]; }
  void setCurWrState(msgBuffState state) { buff_state[cur_wr_entry] = state; }
  void setCurRdState(msgBuffState state) { buff_state[cur_wr_entry] = state; }
};

class ZqmPerHartRegs {
public:

  // disallow copying and assignment
  //ZqmPerHartRegs ( const ZqmPerHartRegs& ) = delete;
  //ZqmPerHartRegs& operator=( const ZqmPerHartRegs& ) = delete;

  // Note: may need to add some other status variables, etc in here
  // May want to put this into the zqm class
  uint64_t status{0}; // TODO: recompute and not store?
  uint16_t logical_pe{UINT16_MAX};
  uint8_t aid{UINT8_MAX};
  uint64_t mem_base_addr{UINT64_MAX};
  uint16_t msgs_per_mbox{UINT16_MAX};

  std::bitset<NUM_MBOXES> active_mboxes;
  std::array<ZqmMsgBuffer, NUM_MBOXES> mbox_buffs; // TODO: delete once msgs are in memory
  std::array<ZqmMboxBuffer, NUM_MBOXES> mbox_buff_state;

  uint64_t getMsgBuffAddr( uint8_t mbox_id, uint8_t entry_num ) {
      uint64_t mbox_base_addr = mem_base_addr + ( mbox_id * msgs_per_mbox * ACTOR_MSG_BYTES );
      uint64_t entry_offset = entry_num * ACTOR_MSG_BYTES;
      return ( mbox_base_addr + entry_offset );
  }
};

// TODO: Create a class to act as the incoming mailbox queues
/*
 * Contains at least the following:
 * queue of pairs (zop, retry cntr)
 * cycle cntr for how long head has been there
 * flag for when it's ready to write to memory
 *
 * handle the logic for checking the head - this probably
 * has to be arbitrated somehow; can't imagine that all
 * 8 mailboxes can check at the same time - maybe it checks
 * every 8th cycle (that would at least be a reasonable
 * starting point here), then we can lower the retry cnt
 */

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
    { "memStartAddr",     "[FORZA] Start address for message buffers",        "0x400" }, // expected to be 0x400 aligned
    { "msgsPerMbox",      "[FORZA] Number of messages per mailbox per Hart",  "2" },
    { "cyclesPerRecycle", "[FORZA] Cycles before recycling incoming msg",     "100"}, // TODO: check w/RTL folks
    { "recyclesToNack",   "[FORZA] Recycles before NACKing incoming msg",     "16"} // TODO: check w/RTL folks
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

    // TODO: Delete these two tracking tables - zops to mem set
    // src zap/hart to physical dest zap/hart, then the per
    // hart csr state can do the tracking instead

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

    void setLocalRzaAsZopDest(SST::Forza::zopEvent *ev)
    {
        ev->setDestHart(Z_MZOP_PIPE_HART);
        ev->setDestZCID(SST::Forza::zopCompID::Z_RZA);
        ev->setDestPCID(ZoneId);
        ev->setDestPrec(PrecinctId);
    }

}; // class SST::ZQM
} // namespace SST::Forza

#endif // _ZQM_H_

// EOF
