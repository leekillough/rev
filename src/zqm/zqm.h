//
// _zqm_h_
//


#ifndef _ZQM_H_
#define _ZQM_H_

#include "zqm_sst.h"
#include "ZOPNet.h" // TODO: replace with version from forzarev
#include <string>

namespace SST::Forza{
#if 0
    // TJD: Figure out what this class is used for; looks like it may
    // just be the elements of a fifo (pending activity)
  class ZQMEntry {
  public:
    SST::Forza::zopEvent *msg;
    uint64_t status;
    uint64_t tail;
    ZQMEntry(SST::Forza::zopEvent *m, uint64_t s) {
      msg = m;
      status = s;
      tail = 0;
    }
  };
#endif

    class ZqmAidStateTableRow {
        // Will need functionality for updating read/write pointers
    public:
        // Note: No valid data element - assuming that if the row exists, it's valid
        uint64_t min_zap_hart;
        uint64_t max_zap_hart; // TODO: Replace with count?
        uint64_t mem_read_ptr;
        uint64_t mem_write_ptr;
        uint64_t mem_buffer_low;
        uint64_t mem_buffer_high;
        ZqmAidStateTableRow(uint64_t min_zap_hart_, uint64_t max_zap_hart_,
                            uint64_t mem_buffer_low_, uint64_t mem_buffer_high_) :
                min_zap_hart(min_zap_hart_),
                max_zap_hart(max_zap_hart_),
                mem_read_ptr(mem_buffer_low_),
                mem_write_ptr(mem_buffer_low_),
                mem_buffer_low(mem_buffer_low_),
                mem_buffer_high(mem_buffer_high_)
        { /* empty constructor */ }
    }; // end class ZqmAidStateTableRow


    class ZQM : public SST::Component{
    public:
        // register the component
        SST_ELI_REGISTER_COMPONENT(
        ZQM,                                    // component class
        "Forza",                                // component libary
        "ZQM",                                  // component name
        SST_ELI_ELEMENT_VERSION(0,0,1),         // Version of the component
        "ZQM: Forza ZQM component",             // description
        COMPONENT_CATEGORY_PROCESSOR            // category
        )

        // describe the parameters
        SST_ELI_DOCUMENT_PARAMS(
        { "clockFreq",  "ZQM core clock frequency", "1GHz" },
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
        /// ZQM: constructor
        /**
         * This needs some info on construction - number of ZAPs, number of HARTs
         * @param id
         * @param params
         */
        ZQM(SST::ComponentId_t id, SST::Params& params);

        /// ZQM: destructor
        ~ZQM();
        void init(unsigned int phase) override;
        void setup() override;
        void complete(unsigned int phase) override;
        void finish() override;

        /**
         * See text in source file
         * @param ev
         *
         * Ok, this is the top level zop handler, there are separate functions
         * for handling each subtype (they are named as "process..."
         */
        void handleIncomingZOP(SST::Event *ev);

        /**
         * This should be read/write requests
         * @param addr
         * @param msg_id
         */
        void sendMsgToRZA(uint64_t addr, uint8_t msg_id); //invoked by clock handler



        /**
         * Do what the function says - prep and send a thread to a ZAP
         */
        void sendThreadToZap(SST::Forza::zopEvent *ev);


    private:
        /// ZQM: clock handler
        bool clock(SST::Cycle_t cycle);

        bool handleNetworkEvent(int i);

        /**
         * This should be reading from the setup_reqs vector and
         * creating/deleting rows in the aid_state table
         */
        void processSetupMsgs(); // invoked by clock handler


        /**
         * This should be reading from the rza_zops vector and processing
         * them
         */
        void processRzaMsgs();  // invoked by clock handler

        /**
         * Read and process messages from the incoming_threads_vec
         *
         */
        void processIncomingThreadsMsgs(); //invoked by clock handler



        // private data members
        SST::Output output;             ///< ZQM: SST output handler
        SST::Interfaces::SimpleNetwork*     m_linkControl;

        // Setup reqs and table for them
        std::vector<SST::Forza::zopEvent*> setup_reqs;
        std::map<uint32_t, ZqmAidStateTableRow> aid_state_table;

        // MZop ACKs and tracking table for in-flight mem ops...will
        // have to do both reads and writes to memory...
        std::vector<SST::Forza::zopEvent*> rza_responses;
        std::map<uint8_t, std::pair<uint64_t, uint64_t> > outstanding_rza_reqs;

        // Incoming threads
        std::vector<SST::Forza::zopEvent*> incoming_threads_vec;

        // Shouldn't need these fields
        //std::map<uint64_t, std::vector<ZQMEntry*> > zqm_queue;
        //std::map<uint64_t, std::vector<ZQMEntry*> > zone_queue;
        //std::map<uint64_t, std::vector<ZQMEntry*> > precinct_queue;

        // [num_zaps][num_harts]
        std::vector<std::vector<bool>> zap_hart_status;

        uint64_t int_id; //
        uint8_t msg_id;
        uint64_t m_num_harts;
        SST::Forza::zopAPI* m_zop_iface;
        bool sent;
    }; // class SST::ZQM
} // namespace SST::Forza

#endif // _ZQM_H_

// EOF
