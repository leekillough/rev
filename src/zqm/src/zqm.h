//
// _zqm_h_
//

/** STATUS **/
/**
 * 
 * 5-sept-2024: updated rev - definitely need more info on the spawn process and how everything fits together
 * as I'm sure there are bugs.  Next up - handling msg_ids across zopnet both send and ack'ing
 * 
 * 3-sept-2024: Most of the basic thread spawn/mig support is in here, haven't touched rev yet (that's tomorrow);
 * will need to get rid of the fatal error that exists in the top level zop handler
 * 
 * 30-aug-2024: need to add support for incoming spawned threads (well, migrating too); however,
 * this is going to require a mechanism for the Rev cores to track the number of harts available and
 * inform the zen/zqm about it..or some other hackery to allow it....
 * 
 * 28-aug-2024: Much like the ZEN at this time, this needs the actual ring interface to finish
 * up (and add the ring response code); currently this will only handle receiving messages and send
 * acks.  We don't actually send any memory zops for handling messages - everything is handled by 
 * structures here in the ZQM.  No support for migrations or spawns as of yet.  Will need to update
 * the test programs.
 */

#ifndef _ZQM_H_
#define _ZQM_H_

#include "zqm_sst.h"
#include "ZOPNET.h"
#include "RingNet.h"
#include <string>

namespace SST::Forza{

#define NUM_RECV_BUFFERS 2
#define MSG_DEPTH 8

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
    FILLING, // unused for now
    READY
};

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
};

class ZqmPerHartRegs {
public:
  // Note: may need to add some other status variables, etc in here
  // May want to put this into the zqm class
  uint64_t status{0};
  uint16_t logical_pe{UINT16_MAX};
  uint8_t aid{UINT8_MAX};
  std::bitset<NUM_MBOXES> active_mboxes;
  std::array<ZqmMsgBuffer, NUM_MBOXES> mbox_buffs;
  //std::queue<SST::Forza::zopEvent*> incoming_zop_q;
};

class ZQM : public SST::Component{
public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
    ZQM,                                    // component class
    "forzazqm",                             // component libary
    "ZQM",                                  // component name
    SST_ELI_ELEMENT_VERSION(0,0,1),         // Version of the component
    "ZQM: Forza ZQM component",             // description
    COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
    { "verbose",    "Sets the output verbsoity", 0 },
    { "clockFreq",  "ZQM core clock frequency", "1GHz" },
    { "clockTicks", "Ticks to exec (TESTING)", "100" },
    { "numCores",        "Number of RISC-V cores (ZAPs)",                 "1" },
    { "numHarts",        "Number of harts (per core/ZAP) to instantiate", "1" },
    { "precinctId",      "[FORZA] The precinct ID of the local device",   "0" },
    { "zoneId",          "[FORZA] The zone ID of the local device",       "0" },
    { "processPerCycle", "[FORZA] Messages to process per cycle",         "10" },
    { "msgQueueDepth",   "[FORZA] Depth of the incoming message queue",   "8192"},
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

    void updateMailboxes();


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
    unsigned process_per_cycle;

    std::string my_name;
    SST::Forza::zopAPI* zone_nic{};      ///< ZQM: zone zop NoC
    zopMsgID *zoneMsgID{};               ///< ZQM: manually allocated message IDs
    SST::Forza::RingNetAPI* zone_ring{}; ///< ZQM: zone csr ring network

    // From ZEN
    bool dma_enabled;

    // the uint32_t is the aid for the row
    //std::map<uint32_t, ZqmAidStateTableRow*> aid_state_table;

    // MZop ACKs and tracking table for in-flight mem ops...will
    // have to do both reads and writes to memory...
    // std::vector<SST::Forza::zopEvent*> rza_responses;
    // std::map<uint8_t, std::pair<uint64_t, uint64_t> > outstanding_rza_reqs; // TODO: WTH is the pair?

    // [numCores][numHarts]
    std::vector<std::vector<bool>> zap_hart_status; //TODO: Put this into the PerHartCSRs

    // Internal structures - ring architecture
    // key pair<aid, logical PE>, value pair<zap, phys_hart>
    std::map<std::pair<uint8_t, uint16_t>, std::pair<uint8_t, uint16_t>> LogicalToPhysicalMap;
    std::vector<std::vector<ZqmPerHartRegs>> PerHartCSRs;
    std::queue<SST::Forza::zopEvent*> IncomingMsgQueue;
    //std::vector<std::queue<SST::Forza::zopEvent*> > MailboxQueues; // TODO: Add an AID dimension - can hold all of our messages here to start with

    std::queue<uint8_t> AwaitingThreadsQueue; // ZAPs waiting for threads
    std::queue<SST::Forza::zopEvent*> RunQueue; // threads waiting for an available HART

    // Structures for holding messages - really ought to rename these and make them camelcase
    std::queue<SST::Forza::zopEvent*> msg_zop_q;
    std::queue<SST::Forza::zopEvent*> tmig_zop_q;
    //std::queue<SST::Forza::zopEvent*> to_rza_q;
    //std::map<uint16_t, MemReturnEntry*> rza_ret_wait_map;

    /// ZQM: clock handler
    bool clock(SST::Cycle_t cycle);

    void processMessagingMsgs(); // invoked by clock handler

    void sendZopAck(SST::Forza::zopEvent *event, zopMsgT msg_type, zopOpc msg_opc);

    void processRzaMsgs();  // invoked by clock handler
    //void processRzaThreadDataReturn(SST::Forza::zopEvent *ev);
    
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
    void sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc);
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

#if 0
    ZqmMailboxMetadata* getDestMboxEntry(SST::Forza::zopEvent *ev){
        auto evt = std::make_tuple( ev->getAppID(), ev->getDestHart(), (uint8_t)ev->getPktRes());
        auto iter = hart_metadata_table.find(evt);
        if (iter == hart_metadata_table.end())
            output.fatal(CALL_INFO, -1, "Could not find table entry for zop.\n"); // TODO: Add add'l debug info if needed
        return iter->second;
    }
#endif

}; // class SST::ZQM
} // namespace SST::Forza

#endif // _ZQM_H_

// EOF
