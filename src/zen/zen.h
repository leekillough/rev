//
// _zen_h_
//


#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>
#include <bitset>
#include <queue>

#define _ZEN_DEFAULT_ZIP_CREDITS_   100

namespace SST::Forza{

  // This probably needs to be 
  // expanded if we're not using DMA (right now, I'm forcing the use
  // of DMA).  Might need to track my_id, parent_id, sequence_counter, 
  // originating zop
#if 0
  class MemReturnEntry {
    public:
      SST::Forza::zopEvent *msg;
      std::vector<uint16_t> msg_ids;
      MemReturnEntry(SST::Forza::zopEvent *m, std::vector<uint16_t>v) :
        msg(m),
        msg_ids(v)
        { /* empty constructor */}
  };
#endif

  // --------------------------------------------
  // ZenMailboxMetadata
  // TODO: For sanity checking, we need the packet
  // size for this mailbox and ensure the buffer
  // pointers align on that boundary - software isn't going
  // to be happy if a full packet isn't in contiguous memory
  // Oh, and remember that we store the full ZOP in memory so 
  // we can return a credit
  // --------------------------------------------
#if 0
  class ZenMailboxMetadata {
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
    ZenMailboxMetadata(uint64_t acs, uint64_t mh, uint64_t mt, uint64_t ms, 
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
#endif


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
    // No NACKs for now
    /*
    void sendNACK(uint16_t hart, uint8_t zcid,
                  uint8_t pcid, uint16_t prec, uint8_t id,
                  SST::Forza::zopAPI *iface);
    */

    /// ZEN: Send an ACK message back to the to target device
    /* Remove this version
    void sendACK(uint16_t hart, uint8_t zcid,
                  uint8_t pcid, uint16_t prec, uint8_t id,
                  SST::Forza::zopAPI *iface);
    */

    void sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc);

    /// ZEN: send a DMA store to the zone's RZA
    // tdysart, 27-june-24: currently unused - zen isn't sending anything to memory
#if 0 
    void sendSdmaToRza(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                       std::vector<uint64_t> store_payload, uint16_t msg_id,
                       uint64_t wr_addr);

    void sendSdmaToRzaAsSequence(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                                 std::vector<uint64_t> store_payload, 
                                 std::vector<uint16_t> msg_ids, uint64_t wr_addr);
#endif

    /// ZEN: send message to scratchpad
    // tdysart, 27-june-24: currently unused - zen isn't sending anything to scratchpad
    //void sendMsgToScratchpad(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload, uint16_t msg_id, bool destsp_is_src);

    /// ZEN: sends a scratchpad WRITE to the target ZAP device
    /* planning on deletion of this version
    void sendMsgToScratchpad(uint64_t dest, uint64_t zcid,
                             uint64_t scratch_addr, uint64_t size,
                             uint64_t addr);
    */

    /// ZEN: processes messages to perform writes to a ZAP:HART Scratchpad
    // tdysart, 27-june-24: currently unused - zen isn't sending anything to scratchpad
    //void notifyHARTScratchpad();

    /// ZEN: handle incoming RZA messages
    // tdysart, 27-june-24: currently unused - zen isn't using rza yet
    //void handleIncomingRZAMsg();

    /// ZEN: handle incoming ZOP messages (from zone NoC)
    void handleIncomingZOP(SST::Event *ev);

    /// ZEN: handle incoming precinct ZOP messages (from precinct NoC)
    void handleIncomingPrecZOP(SST::Event *ev);

    /// ZEN: helper functions for zop types entering from zone noc
    void helper_handleFromZoneMsgZop(SST::Forza::zopEvent *ev);

    /// ZEN: helper functions for zop types entering from precinct noc
    // tdysart, 27-june-24: currently unused - zen shouldn't receive 
    // messaging packets (as of now) from precinct noc
    //void helper_handleFromPrecMsgZop(SST::Forza::zopEvent *ev);

    /// ZEN: processes incoming setup messages
    // tdysart, 27-june-24: currently unused - zen shouldn't receive 
    // setup packets right now    
    //void processSetupMsgs();

    /// ZEN: retrieve the read ACS
    uint64_t  getReadACS(uint64_t);

    /// ZEN: retrieve the write ACS
    uint64_t  getWriteACS(uint64_t);

    /// ZEN: preps to send the RZA a STORE
    // tdysart, 27-june-24: currently unused - zen shouldn't receive 
    // setup packets right now    
    //void prepSendRZAStore();

    /// ZEN: deal with an ack returning from the scratchpad
    // tdysart, 27-june-24: currently unused - zen isn't sending anything to scratchpad
    //void handleScratchpadAck(uint16_t msg_id);

    void processFromZoneMsgQueue();

    /// ZEN: Create a metadata hash consisting of {AppID, Zap, Hart}
    /// For now - physical HART == logical HART; long run this is probably
    /// logical thread ID instead of physical HART
    uint64_t getMetadataHash(SST::Forza::zopEvent *ev, bool use_dest){
      uint64_t rv = 0;
      uint64_t hart_id = (use_dest) ? ev->getDestHart() : ev->getSrcHart();
      uint64_t zap_id = (use_dest) ? ev->getDestZCID() : ev->getSrcZCID();
      uint64_t hdr_app_id = ev->getAppID();
      rv =  (hdr_app_id << (Z_SHIFT_HARTID + Z_SHIFT_ZCID)) | (zap_id << Z_SHIFT_HARTID) | (hart_id);
      return rv; 
      // TODO: Look at just returning the metadata lookup pair.
    }

#if 0
    ZenMailboxMetadata* getMboxEntry(SST::Forza::zopEvent *ev){
      uint64_t metadata_hash = getMetadataHash(ev, false);
      auto iter = hart_metadata_table.find(std::pair<uint64_t, uint64_t>(metadata_hash, ev->getPktRes()));
      if (iter == hart_metadata_table.end())
        output.fatal(CALL_INFO, -1, "Could not find table entry for zop.\n"); // TODO: Add add'l debug info if needed
      return iter->second;
    }

    ZenMailboxMetadata* getDestMboxEntry(SST::Forza::zopEvent *ev){
      uint64_t metadata_hash = getMetadataHash(ev, true);
      auto iter = hart_metadata_table.find(std::pair<uint64_t, uint64_t>(metadata_hash, ev->getPktRes()));
      if (iter == hart_metadata_table.end())
        output.fatal(CALL_INFO, -1, "Could not find table entry for zop.\n"); // TODO: Add add'l debug info if needed
      return iter->second;
    }
#endif

    /// ZEN: determine if zop dest precinct and zone match me
    bool isDestLocal(SST::Forza::zopEvent *ev)
    {
      if ( (ev->getDestPCID() == Zone) &&
           (ev->getDestPrec() == Precinct) )
          return true;
      return false;
    }

    bool isSrcLocal(SST::Forza::zopEvent *ev)
    {
      if ( (ev->getSrcPCID() == Zone) &&
           (ev->getSrcPrec() == Precinct) )
          return true;
      return false;
    }

    void setMeAsZopSrc(SST::Forza::zopEvent *ev)
    {
      //SST::Forza::zopAPI *iface = isSrcLocal(ev) ? zone_nic : m_prec_iface;
      ev->setSrcHart(0);
      ev->setSrcZCID(SST::Forza::zopCompID::Z_ZEN);
      ev->setSrcPCID(Zone);
      ev->setSrcPrec(Precinct);
    }

    void setLocalRzaAsZopDest(SST::Forza::zopEvent *ev)
    {
      ev->setDestHart(Z_MZOP_PIPE_HART);
      ev->setDestZCID(SST::Forza::zopCompID::Z_RZA);
      ev->setDestPCID(Zone);
      ev->setDestPrec(Precinct);
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

    // private data members
    SST::Output output;                   ///< ZEN: SST output handler
    SST::Forza::zopAPI* zone_nic;      ///< ZEN: ZOP Network interfaces for zone network
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

    /*
      This is incremented in handleIncomingPrecZOP()
    */
    uint64_t zip_credits;

    // Pair is {AppID, Zap, Hart}, MboxId
    //std::map<std::pair<uint64_t, uint64_t>, ZenMailboxMetadata*> hart_metadata_table;
    
    // TODO: These should be nothing more than credit counters
    //std::map<uint64_t, ZenMailboxMetadata*> zone_tables;
    //std::map<uint64_t, ZenMailboxMetadata*> precinct_tables;

    /* 
      Add to map: handleIncomingZOP() - destination is same zone; basically the default
        case for messaging types in this function
      Add to map: handleIncomingPrecZOP() - destination is unchecked (but assumed to be this zone)
    */
    //std::map<std::pair<uint64_t, uint64_t>, std::vector<ZENEntry*>> zen_queue;

    /*
      Creating a new data structure to handle MSG.SENDP and MSG.MBXDONE messages
    */
    std::queue<SST::Forza::zopEvent*> from_zone_messaging_queue;

    /*
      Add to map: handleIncomingZOP() - destination is same precinct, diff zone
    */
    //std::map<uint64_t, std::vector<ZENEntry*> > zone_queue;

    /*
      Add to map: handleIncomingZOP() - destination is diff precinct
    */
    //std::map<uint64_t, std::vector<ZENEntry*> > precinct_queue;


    /*
      Add to vector: handleIncomingZOP() - RZA response
    */
    //std::queue<SST::Forza::zopEvent*> mem_acks; 

    /*
      Add to queue: handleIncomingZOP() - ZEN setup message
    */
    std::queue<SST::Forza::zopEvent*> setup_reqs;

    /*
      Add to vector: handleIncomingZOP() - Credit message
      Add to vector: handleIncomingPrecZOP() - Credit message
    */
    //std::vector<SST::Forza::zopEvent*> zap_credits;


    //std::map<uint8_t, ZENEntry*> outstanding_mem_req;

    /* 
      Add to queue: handleIncomingPrecZOP() - source precinct != my_precinct
      This is for incoming messages; precessing this queue will send out credit packets
        Depending on implementation of credits, we may need to be returning some to the 
        ZIP for anything that came from outside this precinct
    */
    //std::queue<SST::Forza::zopEvent*> zipQ;

    // Messages placed here are heading to the precinct NoC
    //std::queue<SST::Forza::zopEvent*> to_precinct_noc_q;

    // Messages placed here are being forward onto zone NOC
    //std::queue<SST::Forza::zopEvent*> to_zone_noc_q;

    // Messages placed here are for the local RZA
    //std::queue<SST::Forza::zopEvent*> to_rza_q;

    // Messages awaiting return from RZA
    // key is msg_ids[0]
    //std::map<uint16_t, MemReturnEntry*> rza_ret_wait_map;

    // Messages for updating the scratchpad
    //std::queue<SST::Forza::zopEvent*> update_scratchpad_q;

    // Vector of outstanding scratchpad transactions
    // I would expect this to generally operate in FIFO order, but
    // it's not a system requirement (Zap traffic may influence)
    //std::vector<uint16_t> outstanding_spad_reqs;

  }; // class SST::ZEN
} // namespace SST::Forza

#endif // _ZEN_H_

// EOF
