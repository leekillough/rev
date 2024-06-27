//
// _zqm_h_
//


#ifndef _ZQM_H_
#define _ZQM_H_

#include "zqm_sst.h"
#include "ZOPNET.h"
#include <string>

namespace SST::Forza{

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
        ZqmMailboxMetadata(uint64_t acs, uint64_t mh, uint64_t mt, uint64_t ms, 
                        uint64_t st, uint8_t app, uint8_t mbox) :
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
        mbox_id(mbox)
        { /* empty constructor */}

        /// @brief  Get current wr ptr and update it to the next addr
        /// @param size packet size we're writing
        /// @return write address (aka mem_wr_ptr)
        uint64_t getRzaWriteAddr(uint8_t size);

        // Pretty much the same as above, but for mem_cur_tail
        uint64_t getSpTailAddr(uint8_t size);
    };


#if 1
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
                            bool sequential_hart_assignment_, uint16_t num_zaps_) :
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
            harts_available = (max_zap_hart - min_zap_hart + 1) * num_zaps_;
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
        { "numCores",        "Number of RISC-V cores (ZAPs)",        "1" },
        { "numHarts",        "Number of harts (per core/ZAP) to instantiate",    "1" },
        { "precinctId",      "[FORZA] The precinct ID of the local device",  "0" },
        { "zoneId",          "[FORZA] The zone ID of the local device",      "0" },
        { "processPerCycle", "[FORZA] Messages to process per cycle", "10" }
        )

        // describe the ports
        SST_ELI_DOCUMENT_PORTS()

        // describe the statistics
        // TODO: Add stats
        SST_ELI_DOCUMENT_STATISTICS()

        // describe the subcomponent slots
        SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
        {"zone_nic","[FORZA] Zone NIC", "SST::Forza::zopNIC"},
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

        /**
         * Ok, this is the top level zop handler, there are separate functions
         * for handling each subtype (they are named as "process...)"
         */
        void handleIncomingZOP(SST::Event *ev);

    private:
        /// ZQM: clock handler
        bool clock(SST::Cycle_t cycle);

        //bool handleNetworkEvent(int i);

        void processMessagingMsgs(); // invoked by clock handler
        //void processMessagingZqmSet(SST::Forza::zopEvent *event);
        void processMessagingHartDone(SST::Forza::zopEvent *event);
        void processMessagingZqmMboxSet(SST::Forza::zopEvent *event);
        void sendMessagingAck(SST::Forza::zopEvent *event);

        void processRzaMsgs();  // invoked by clock handler
        //void processRzaThreadDataReturn(SST::Forza::zopEvent *ev);
        
        // tdysart, 27-june-2024 removing thread management for now
#if 0
        void sendThreadToRza(SST::Forza::zopEvent *thread);
        void getThreadFromRza(uint32_t app_id);
        void sendThreadToZap(SST::Forza::zopEvent *thread);
#endif

        /**
         * Read and process messages from the incoming_threads_vec
         *
         */
        // tdysart, 27-june-2024 removing thread management for now
        //void processIncomingThreadsMsgs(); //invoked by clock handler

        /**
         * @param thread
         * @return true if dest zap/hart filled, false otherwise
         */
        // tdysart, 27-june-2024 removing thread management for now
        //bool selectRandomDestHart(SST::Forza::zopEvent *thread);
        //void selectSequentialDestHart(SST::Forza::zopEvent *thread, ZqmAidStateTableRow *aid_state);

        /**
         * Get the pointer to the state table for the given AID
         * @param aid - application ID
         * @return pointer to state table row; nullptr if not found (also a fatal error)
         */
        // ZqmAidStateTableRow* getAidStateTableRow(uint32_t aid);

        /**
         * If a hart is empty, try to fill it (mostly for migrating thread applications)
         */
        // tdysart, 27-june-2024 removing thread management for now
        //void fillEmptyHart();

        // private data members
        SST::Output output;             ///< ZQM: SST output handler

        // the uint32_t is the aid for the row
        //std::map<uint32_t, ZqmAidStateTableRow*> aid_state_table;

        // MZop ACKs and tracking table for in-flight mem ops...will
        // have to do both reads and writes to memory...
        std::vector<SST::Forza::zopEvent*> rza_responses;
        std::map<uint8_t, std::pair<uint64_t, uint64_t> > outstanding_rza_reqs; // TODO: WTH is the pair?

        // Incoming threads
        std::vector<SST::Forza::zopEvent*> incoming_threads_vec;

        // [num_zaps][num_harts]
        std::vector<std::vector<bool>> zap_hart_status;

        zopMsgID *zoneMsgID;            ///< ZQM: manually allocated message IDs

        std::string my_name;
        SST::Forza::zopAPI* zone_nic;
        bool sent;

        // Parameters to maintain
        std::string clockFreq;
        unsigned num_zaps;
        uint16_t num_harts;
        unsigned precinct_id;
        unsigned zone_id;
        unsigned process_per_cycle;

        // Parameters for testing
        SST::Cycle_t cycleCount;

        // Structures for holding messages
        std::queue<SST::Forza::zopEvent*> setup_reqs;

        // Structures for holding state in the ZQM
        // Pair is {AppID, Zap, Hart}, MboxId
        std::map<std::pair<uint64_t, uint64_t>, ZqmMailboxMetadata*> hart_metadata_table;

        // Vector of outstanding scratchpad transactions
        // I would expect this to generally operate in FIFO order, but
        // it's not a system requirement (Zap traffic may influence)
        std::vector<uint16_t> outstanding_spad_reqs;

        // Functions copied over from the ZEN
        void sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc);
        void handleScratchpadAck(uint16_t msg_id);
        void sendMsgToScratchpad(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload, uint16_t msg_id, bool destsp_is_src);


        void setMeAsZopSrc(SST::Forza::zopEvent *ev)
        {
            ev->setSrcHart(0);
            ev->setSrcZCID(SST::Forza::zopCompID::Z_ZQM);
            ev->setSrcPCID(zone_id);
            ev->setSrcPrec(precinct_id);
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

        // Create a metadata hash consisting of {AppID, Zap, Hart}
        // For now - physical HART == logical HART; long run this is probably
        // logical thread ID instead of physical HART
        uint64_t getMetadataHash(SST::Forza::zopEvent *ev, bool use_dest){
        uint64_t rv = 0;
        uint64_t hart_id = (use_dest) ? ev->getDestHart() : ev->getSrcHart();
        uint64_t zap_id = (use_dest) ? ev->getDestZCID() : ev->getSrcZCID();
        uint64_t hdr_app_id = ev->getAppID();
        rv =  (hdr_app_id << (Z_SHIFT_HARTID + Z_SHIFT_ZCID)) | (zap_id << Z_SHIFT_HARTID) | (hart_id);
        return rv; 
        // TODO: Look at just returning the metadata lookup pair.
        }

        // Functions for simple loopback testing
        // Remove in the near future.
#if 0
        void doSimpleMsg();
        void sendDummyThread();
        void sendHartDone();
        void configMTApp(); // mostly equiv to doSimpleMsg()
        void sendMtThread(); // mostly equiv to sendDummyThread()
        void configMtAndRunQueue();
        void sendHartDoneForRzaTest();
        void sendLdmaPacket();
#endif

    }; // class SST::ZQM
} // namespace SST::Forza

#endif // _ZQM_H_

// EOF
