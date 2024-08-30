//
// _zen_h_
//

/* STATUS */
/**
 * 
 * 30-aug-2024: added very preliminary support for spawning threads via csrs; the specs are missing a lot of
 * detail on the mechanics of this process (thread id, aid, total transmitted size, location, etc) that need
 * to be clarified.  Additionally, I just have spawned threads heading straight out to the zone crossbar (the
 * same way messages do) rather than share the access to the zone crossbar.  Probably not a huge deal for now,
 * but something we should be aware of.  Of course, there is no support on the zqm side (yet) for receiving 
 * said spawned threads.
 * 
 * 22-aug-2024: made steering changes to zopnet; need to revamp zopEvent structure to allow wider hart IDs (or 
 * revamp into logical/physical id)
 * 
 * 21-aug-2024: need the actual ring interface to finish this up (and add the ring response code); currently this will only handle sending messages 
 * and accept receiving acks.  We don't actually send and memory zops (nor write acks) for the retry buffer.  No 
 * support for migrations or spawns.  Overall, just trying to get the messaging process within a zone to work 
 * properly (due to forzarev issue #120, we have limited multizone testing; plus zopnet is going to need some work).
 * And actually, coming back to zopnet, that will need to be modified to properly steer messages to zen/zqm even
 * though the actual src/dest info may be a specific hart/pe
 * 
 */

#ifndef _ZEN_H_
#define _ZEN_H_

#include "zen_sst.h"
#include "ZOPNET.h"
#include <string>
#include <bitset>
#include <queue>

#define _ZEN_DEFAULT_ZIP_CREDITS_   100

namespace SST::Forza{

// --------------------------------------------
// Preprocessor defs
// --------------------------------------------

#define NUM_MBOXES 8
#define ACTOR_MSG_LENGTH 8

// My version of data word for the ZENENQ_CTRL
// { Fill[35:0], Precinct[10:0], Zone[2:0], Mbox[2:0], logicalPE[10:0]}
// {36, 11, 3, 3, 11}
#define R_SHIFT_MBOX 11
#define R_SHIFT_ZONE 14
#define R_SHIFT_PREC 17

#define R_MASK_LOGPE 0x7FF
#define R_MASK_MBOX  0b111
#define R_MASK_ZONE  0b111
#define R_MASK_PREC  0x7FF

// CSR registers used by the ZEN
#define R_ZENSTAT 0x802
// messaging 
#define R_ZENEQC 0x840
#define R_ZENEQD 0x841
#define R_ZENEQS 0x842 // for spawn
#define R_ZENOMC 0xcc3
// spawning

/*
 The bit assignments of the Control Word fields provided by the ZAP are shown below:
  [63]           Message Clear
  [47:40]     Message Opcode: 8b
  [39:36]     Message AID; 4b
  [35:33]     Destination Mailbox;    3b
  [32:20]     Destination Precinct; 13b
  [19:16]     Destination Zone; 4b
  [15:12]     Destination Zone Component; 4b
  [11:0]       Destination PE; 12b

  [60:48] retry seq number; 13b
  // From the Zone & Zap doc - 15-aug-2024
*/
#define ZENEQC_SHIFT_DESTCOMP 12
#define ZENEQC_SHIFT_DESTZONE 16
#define ZENEQC_SHIFT_DESTPREC 20
#define ZENEQC_SHIFT_DESTMBOX 33
#define ZENEQC_SHIFT_MSGAID   36
#define ZENEQC_SHIFT_MSGOPC   40
#define ZENEQC_SHIFT_RETRYNUM 48
#define ZENEQC_SHIFT_MSGCLR 63

#define ZENEQC_MASK_DESTPE   0x0fff
#define ZENEQC_MASK_DESTCOMP 0x0f
#define ZENEQC_MASK_DESTZONE 0x0f
#define ZENEQC_MASK_DESTPREC 0x01fff
#define ZENEQC_MASK_DESTMBOX 0x07
#define ZENEQC_MASK_MSGAID   0x0f
#define ZENEQC_MASK_MSGOPC   0x0ff
#define ZENEQC_MASK_RETRYNUM 0x01fff
#define ZENEQC_MASK_MSGCLR   0x1

#define ZENSTAT_SHIFT_SPNBUSY 32

// --------------------------------------------
// ringMsgT : Ring Msg Type
// --------------------------------------------
enum class ringMsgT : uint8_t {
  R_RETDATA = 0b00,  /// Forza RETURN DATA
  R_READ    = 0b01,  /// Forza READ
  R_RMW     = 0b10,  /// Forza RMW
  R_UPDATE  = 0b11,  /// Forza UPDATE
};

// --------------------------------------------
// ringEvent
// --------------------------------------------
class ringEvent : public SST::Event {
  /**
   * TODO: NEEDED FUNCTIONS
   * getCSR() // return a uint16_t with the CSR reg value
   * getZapId() // return a uint8_t with the zap number
   * getHartId() // return a uint16_t with the hart number 
   * getOp() // retun operation
   * getData() // return data word
   */

public:
  // Use this constructor only for the initial broadcast
  explicit ringEvent( unsigned srcID, ringCompID srcComp )
    : Event(), SrcComp( srcComp ), Hart( 0 ), DestComp( ringCompID::R_UNKNOWN ), Type( ringMsgT::R_RETDATA ),
      CSR( ringRegT::R_ZENSTAT ), Datum( srcID ) { /* Empty constructor */ }

  // raw event constructor
  explicit ringEvent()
    : Event(), SrcComp( ringCompID::R_UNKNOWN ), Hart( 0 ), DestComp( ringCompID::R_UNKNOWN ), Type( ringMsgT::R_RETDATA ),
      CSR( ringRegT::R_ZENSTAT ), Datum( 0 ) { /* Empty constructor */ }

  explicit ringEvent( ringCompID srcComp, uint16_t hart, ringCompID destComp, ringMsgT type, ringRegT csr, uint64_t datum )
    : Event(), SrcComp( srcComp ), Hart( hart ), DestComp( destComp ), Type( type ), CSR( csr ),
      Datum( datum ) { /* Empty constructor */ }

  virtual Event* clone( void ) override {
    ringEvent* ev = new ringEvent( *this );
    return ev;
  }

  /** TODO: ADD SERIALIZERS? **/

  /* Set functions */

  /* Get functions */
  ringCompID getSrcComp() { return SrcComp; }

  ringCompID getDestComp() { return DestComp; }

  std::string getDestCompStr() { return getCompStr( false ); }

  std::string getSrcCompStr() { return getCompStr( true ); }

  std::string getCompStr( bool isSrc ) {
    ringCompID C = ( isSrc ) ? SrcComp : DestComp;
    switch( C ) {
    case ringCompID::R_ZAP0: return "ZAP0"; break;
    case ringCompID::R_ZAP1: return "ZAP1"; break;
    case ringCompID::R_ZAP2: return "ZAP2"; break;
    case ringCompID::R_ZAP3: return "ZAP3"; break;
    case ringCompID::R_ZEN: return "ZEN"; break;
    case ringCompID::R_ZQM: return "ZQM"; break;
    default: return "UNKNOWN"; break;
    }
  }

private:
  ringCompID SrcComp;   /// ringEvent: Dest component
  uint16_t   Hart;      /// ringEvent: HART involved in transaction
  ringCompID DestComp;  /// ringEvent: Dest component
  ringMsgT   Type;      /// ringEvent: Command type
  ringRegT   CSR;       /// ringEvent: Register accessed
  uint64_t   Datum;     /// ringEvent: data payload // TODO: Vector?

public:
  // ringEvent: event serializer
  //void serialize_order( SST::Core::Serialization::serializer& ser ) override {
  // we only serialize the raw packet
  //  Event::serialize_order( ser );
  //ser & Packet;
  //}

  // ringEvent: implements the nic serialization
  ImplementSerializable( SST::Forza::ringEvent );
};  //class ringEvent



class ZenPerHartRegs {
  // Note: may need to add some other status variables, etc in here
  // May want to put this into the zen class
private:
  uint64_t status{ (1UL << 63;) }; //default to turning on the enabled bit
  std::array<uint64_t, ACTOR_MSG_LENGTH> msg{}; // word 0 is control, 1-7 are data
  std::array<uint8_t, NUM_MBOXES> mbox_cntrs{};
  uint8_t msg_cur_word{1};
  bool is_sending{}; //signifies that the hart is in the process of sending a message; OR'd with mbox_busy portion of status register when status is read 
  std::array<uint64_t, 2> spawn_thread{};
  uint8_t spawn_cur_word{0};
};

// Configure these as a base + inhereted classes?
class OutgoingMessage {

  OutgoingMessage( std::array<uint64_t, ACTOR_MSG_LENGTH> data, uint8_t zap, uint16_t hart ) :
    msg(data), src_zap(zap), src_hart(hart)
    { /* empty */}

  private:
    // More info needed?
    std::array<uint64_t, ACTOR_MSG_LENGTH> msg{};
    uint8_t src_zap;
    uint16_t src_hart;
    uint32_t msg_id{UINT32_MAX}; // retry number/id
    uint64_t mem_addr; // currently unused
};

class OutgoingSpawn {

  OutgoingSpawn(std::array<uint64_t, 2> data, uint8_t zap, uint16_t hart ) :
    thread(data), src_zap(zap), src_hart(hart)
    { /* empty */ }

  private:
    // More info needed?
    std::array<uint64_t, 2> thread{};
    uint8_t src_zap;
    uint16_t src_hart;
    uint8_t aid{UINT8_MAX}; // WHERE FROM?
};

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
      { "seqMgrDepth",       "[FORZA] Number of entries in retry buffer",    "8192"},
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

    // Ring based functions
    void handleRingMsg( SST::Event *event );
    void handleRingOmc( SST::Forza::ringEvent *ev );
    void handleRingStatus( SST::Forza::ringEvent *ev );
    void handleRingEqData( SST::Forza::ringEvent *ev );
    void handleRingEqCtrl( SST::Forza::ringEvent *ev );
    void handleRingSpawn( SST::Forza::ringEvent *ev );
    

    void sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data );

    uint32_t getRetrySeqNum();
    void ExecMsgPipeline();
    void execMsgPipe0();
    void execMsgPipe1();
    void execMsgPipe2();
    void updateMsgPipe0();

    void handleMsgAck(zopEvent *ack);

    /**
     * msg: zen msg to be sent
     * is_msg: true if sending a message, false is to rza/retry buffer memory
     */
    void sendMsgZop(OutgoingMessage *msg, bool is_msg, uint16_t zop_msg_id);


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

    /// ZEN: handle incoming RZA messages
    // tdysart, 27-june-24: currently unused - zen isn't using rza yet
    //void handleIncomingRZAMsg();

    /// ZEN: handle incoming ZOP messages (from zone NoC)
    void handleIncomingZOP(SST::Event *ev);

    /// ZEN: handle incoming precinct ZOP messages (from precinct NoC)
    void handleIncomingPrecZOP(SST::Event *ev);

    /// ZEN: helper functions for zop types entering from zone noc
    void helper_handleMsgZop(SST::Forza::zopEvent *ev);

    /// ZEN: retrieve the read ACS
    uint64_t  getReadACS(uint64_t) { return (acs_pair & Z_ACS_READ) >> 32; }

    /// ZEN: retrieve the write ACS
    uint64_t  getWriteACS(uint64_t) { return acs_pair & Z_ACS_WRITE; }


    /// ZEN: preps to send the RZA a STORE
    // tdysart, 27-june-24: currently unused - zen shouldn't receive 
    // setup packets right now    
    //void prepSendRZAStore();

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

    void setLocalZqmAsZopDest(SST::Forza::zopEvent *ev)
    {
      ev->setDestHart(0);
      ev->setDestZCID(SST::Forza::zopCompID::Z_ZQM);
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
    SST::Forza::zopAPI* zone_nic{};      ///< ZEN: ZOP Network interfaces for zone network
    SST::Forza::zopMsgID *zoneMsgID{};            ///< ZEN: manually allocated message IDs
    SST::Forza::zopAPI* m_prec_iface{};     ///< ZEN: ZOP Network interfaces for precinct network

    // ----- BEGIN SST PARAMETERS - some of these should be camel case to match other code
    unsigned Precinct;              ///< ZEN: Precinct ID
    unsigned Zone;                  ///< ZEN: Zone ID
    unsigned m_num_harts;           ///< ZEN: number of harts
    unsigned m_num_zaps;            ///< ZEN: number of zaps
    unsigned m_num_zones;           ///< ZEN: number of zones
    unsigned m_num_precincts;       ///< ZEN: number of precincts
    bool dma_enabled;               ///< ZEN: enable DMA operations
    uint64_t zen_queue_size_limit;  ///< ZEN: zen queue size limit
    uint64_t process_per_cycle;     ///< ZEN: messages to process per cycle
    uint32_t SeqNumMgrDepth;        ///< ZEN: sequence number manager depth
    // ----- END SST PARAMETERS

    // Internal data structures
    std::vector<std::vector<ZenPerHartRegs>> PerHartCSRs;
    std::vector<bool> SeqNumMgrList;
    std::map<int32_t, OutgoingMessage*> RetryMgrMap;
    std::queue<OutgoingMessage*> OutMsgQueue;
    std::array<OutgoingMessage*, 3> MsgPipeline;
    std::queue<zopEvent*> MsgAckQueue;
    std::queue<OutgoingSpawn*> OutSpawnQueue;

    /*
      This is incremented in handleIncomingPrecZOP()
    */
    //uint64_t zip_credits;

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
    //std::queue<SST::Forza::zopEvent*> setup_reqs;

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
