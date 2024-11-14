//
// _ZOPNet_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_RINGNET_H_
#define _SST_RINGNET_H_

// -- Standard Headers
#include <map>
#include <queue>
#include <unistd.h>
#include <vector>

// -- SST Headers
#include "SST.h"

/// -- Rev Headers
#include "../common/include/RevCommon.h"
#include "ZOPNET.h"

namespace SST::Forza {

// --------------------------------------------
// Preprocessor defs
// --------------------------------------------

#define NUM_MBOXES                 8
#define ACTOR_MSG_LENGTH           8

// CSR registers used by the ZEN
#define R_ZENSTAT                  0x802
// messaging
#define R_ZENEQC                   0x840
#define R_ZENEQD                   0x841
#define R_ZENEQS                   0x842  // for spawn
#define R_ZENOMC                   0xcc3
// spawning

// CSR registers used by the ZQM
#define R_ZQMSTAT                  0x803
#define R_ZQMMBOXREG               0x804
#define R_ZQMDQ_0                  0x808
#define R_ZQMDQ_7                  0x80F

// encoding for the ring event
/**
 * [32:31] -- cmd
 * [30:27] -- dest comp
 * [26:23] -- src comp
 * [22:11] -- csr number
 * [10:0] -- hart_id
 */
#define R_SHIFT_HARTID             0
#define R_SHIFT_CSRNUM             11
#define R_SHIFT_SRCZCID            23
#define R_SHIFT_DESTZCID           27
#define R_SHIFT_CMD                31

#define R_MASK_HARTID              0x07ff
#define R_MASK_CSR                 0x0fff
#define R_MASK_ZCID                0x0f
#define R_MASK_CMD                 0x3

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
#define ZENEQC_SHIFT_DESTCOMP      12
#define ZENEQC_SHIFT_DESTZONE      16
#define ZENEQC_SHIFT_DESTPREC      20
#define ZENEQC_SHIFT_DESTMBOX      33
#define ZENEQC_SHIFT_MSGAID        36
#define ZENEQC_SHIFT_MSGOPC        40
#define ZENEQC_SHIFT_RETRYNUM      48
#define ZENEQC_SHIFT_MSGCLR        63

#define ZENEQC_MASK_DESTPE         0x0fff
#define ZENEQC_MASK_DESTCOMP       0x0f
#define ZENEQC_MASK_DESTZONE       0x0f
#define ZENEQC_MASK_DESTPREC       0x01fff
#define ZENEQC_MASK_DESTMBOX       0x07
#define ZENEQC_MASK_MSGAID         0x0f
#define ZENEQC_MASK_MSGOPC         0x0ff
#define ZENEQC_MASK_RETRYNUM       0x01fff
#define ZENEQC_MASK_MSGCLR         0x1

#define ZENSTAT_SHIFT_SPNBUSY      32

/**
 * Assumed format when updating the ZQM MBOXREG CSR:
 * [7:0] - mboxes used (bit mask) - 8b
 * [18:8] - logical PE - 11b
 * [27:19] - physical hart - 9b
 * [31:28] - physical zap - 4b
 * [63:60] - aid
 * Don't need anything for zone id (since the zqm here will have that info)
 */

#define ZQMMBOXREG_SHIFT_MBXSUSED  0  // do this now in case it changes later
#define ZQMMBOXREG_SHIFT_LOGICALPE 8
#define ZQMMBOXREG_SHIFT_PHYSHART  19
#define ZQMMBOXREG_SHIFT_PHYSZAP   28
#define ZQMMBOXREG_SHIFT_AID       60

#define ZQMMBOXREG_MASK_MBXSUSED   0x0FF
#define ZQMMBOXREG_MASK_LOGICALPE  0x7FF
#define ZQMMBOXREG_MASK_PHYSHART   0x1FF
#define ZQMMBOXREG_MASK_PHYSZAP    0x0F
#define ZQMMBOXREG_MASK_AID        0x0F

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

public:
  // raw event constructor
  ringEvent() = default;

  // Use this constructor only for the initial broadcast
  ringEvent( zopCompID SrcComp, uint64_t Datum ) : SrcComp{ SrcComp }, Datum{ Datum } {}

  ringEvent( zopCompID SrcComp, uint16_t Hart, zopCompID DestComp, ringMsgT Type, uint16_t CSR, uint64_t Datum )
    : SrcComp{ SrcComp }, Hart{ Hart }, DestComp{ DestComp }, Type{ Type }, CSR{ CSR }, Datum{ Datum } {}

  // copy and move constructors
  ringEvent( const ringEvent& )            = default;
  ringEvent( ringEvent&& )                 = default;

  // do not allow assignment
  ringEvent& operator=( const ringEvent& ) = delete;
  ringEvent& operator=( ringEvent&& )      = delete;

  // destructor
  ~ringEvent() override                    = default;

  // clone
  Event* clone() override { return new ringEvent( *this ); }

  /* Set functions */

  /* Get functions */
  auto getDatum() const { return Datum; }

  auto getCSR() const { return CSR; }

  auto getHart() const { return static_cast<uint16_t>( Hart & 0x1ff ); }

  auto getOp() const { return Type; }

  auto getSrcComp() const { return SrcComp; }

  auto getDestComp() const { return DestComp; }

  auto getSrcZap() const { return static_cast<uint8_t>( SrcComp ); }

  auto getDestCompStr() const { return getCompStr( DestComp ); }

  auto getSrcCompStr() const { return getCompStr( SrcComp ); }

private:
  static std::string getCompStr( zopCompID Comp ) {
    switch( Comp ) {
    case zopCompID::Z_ZAP0: return "ZAP0";
    case zopCompID::Z_ZAP1: return "ZAP1";
    case zopCompID::Z_ZAP2: return "ZAP2";
    case zopCompID::Z_ZAP3: return "ZAP3";
    case zopCompID::Z_ZEN: return "ZEN";
    case zopCompID::Z_ZQM: return "ZQM";
    default: return "UNKNOWN";
    }
  }

public:
#if 0
  // clang-format off
  /// ringEvent: decode this event and set the appropriate internal structures
  void decodeEvent() {
    Hart     = uint16_t  ( Packet[0] >> R_SHIFT_HARTID   & R_MASK_HARTID );
    SrcComp  = zopCompID ( Packet[0] >> R_SHIFT_SRCZCID  & R_MASK_ZCID   );
    DestComp = zopCompID ( Packet[0] >> R_SHIFT_DESTZCID & R_MASK_ZCID   );
    Type     = ringMsgT  ( Packet[0] >> R_SHIFT_CMD      & R_MASK_CMD    );
    CSR      = uint16_t  ( Packet[0] >> R_SHIFT_CSRNUM   & R_MASK_CSR    );
    Datum    = Packet[1];
  }

  /// ringEvent: encode this event and set the appropriate internal packet structures
  void encodeEvent() {
    Packet.resize(2);
    Packet[0] = 0;
    Packet[0] |= uint64_t ( Hart & R_MASK_HARTID ) << R_SHIFT_HARTID;
    Packet[0] |= uint64_t ( Hart & R_MASK_ZCID   ) << R_SHIFT_SRCZCID;
    Packet[0] |= uint64_t ( Hart & R_MASK_ZCID   ) << R_SHIFT_DESTZCID;
    Packet[0] |= uint64_t ( Hart & R_MASK_CSR    ) << R_SHIFT_CSRNUM;
    Packet[0] |= uint64_t ( Hart & R_MASK_CMD    ) << R_SHIFT_CMD;
    Packet[1] = Datum;
  }
  // clang-format on
#endif

private:
  //std::vector<uint64_t> Packet;  ///< zopEvent: data payload: serialized payload

  zopCompID SrcComp  = zopCompID::Z_UNK_COMP;  /// ringEvent: Dest component
  uint16_t  Hart     = 0;                      /// ringEvent: HART involved in transaction
  zopCompID DestComp = zopCompID::Z_UNK_COMP;  /// ringEvent: Dest component
  ringMsgT  Type     = ringMsgT::R_RETDATA;    /// ringEvent: Command type
  uint16_t  CSR      = 0;                      /// ringEvent: Register accessed
  uint64_t  Datum    = 0;                      /// ringEvent: data payload

public:
  // ringEvent: event serializer
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & SrcComp;
    ser & Hart;
    ser & DestComp;
    ser & Type;
    ser & CSR;
    ser & Datum;
  }

  // ringEvent: implements the nic serialization
  ImplementSerializable( SST::Forza::ringEvent );
};  //class ringEvent

// -------------------------------------------------------
// RingNetAPI
// -------------------------------------------------------
class RingNetAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Forza::RingNetAPI )

  /// RingNetAPI: constructor
  RingNetAPI( ComponentId_t id, Params& params ) : SubComponent( id ) {}

  /// RingNetAPI: default destructor
  ~RingNetAPI() override                                    = default;

  /// RingNetAPI: registers the event handler with the core
  virtual void setMsgHandler( Event::HandlerBase* handler ) = 0;

  /// RingNetAPI: initializes the network
  virtual void init( unsigned int phase )                   = 0;

  /// RingNetAPI: setup the network
  virtual void setup() {}

  /// RingNetAPI: send a message on the network
  virtual void send( ringEvent* ev, uint64_t dest )          = 0;

  /// RingNetAPI: retrieve the number of destinations
  virtual unsigned getNumDestinations()                      = 0;

  /// RingNetAPI: returns NIC's network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress() = 0;

  /// RingNetAPI: return the next possible network address (ring topology)
  virtual uint64_t getNextAddress()                          = 0;

  /// RingNetAPI: set the type of the endpoint
  virtual void setEndpointType( zopCompID type )             = 0;

  /// RingNetAPI: get the type of the endpoint
  virtual zopCompID getEndpointType()                        = 0;

};  // class RingNetAPI

// -------------------------------------------------------
// RingNetNIC
// -------------------------------------------------------
class RingNetNIC : public RingNetAPI {
public:
  // register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    RingNetNIC,
    "forza",       // component library
    "RingNetNIC",  // component name
    SST_ELI_ELEMENT_VERSION( 0, 0, 1 ),
    "RingNet SST NIC",
    SST::Forza::RingNetAPI
  )

  // register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "clock", "Clock frequency of the NIC", "1Ghz" },
    { "port", "Port to use, if loaded as an anonymous subcomponent", "network" },
    { "verbose", "Verbosity for output (0 = nothing)", "0" },
  )

  // register the ports
  SST_ELI_DOCUMENT_PORTS( { "network", "Port to network", { "simpleNetworkExample.nicEvent" } } )

  // register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( { "iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork" } )

  // RingNetNIC: constructor
  RingNetNIC( ComponentId_t id, Params& params );

  /// RingNetNIC: destructor
  ~RingNetNIC() override = default;

  /// RingNetNIC: callback to parent on received messages
  virtual void setMsgHandler( Event::HandlerBase* handler );

  /// RingNetNIC: init function
  virtual void init( unsigned int phase );

  /// RingNetNIC: setup function
  virtual void setup();

  /// RingNetNIC: send event to the destination id
  virtual void send( ringEvent* ev, uint64_t dest );

  /// RingNetNIC: retrieve the number of destinations
  virtual unsigned getNumDestinations();

  /// RingNetNIC: get the endpoint's network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress();

  /// RingNetAPI: return the next possible network address (ring topology)
  virtual uint64_t getNextAddress();

  /// RingNetNIC: set the endpoint type
  virtual void setEndpointType( zopCompID type ) { Type = type; }

  /// RingNetNic: get the endpoint type
  virtual zopCompID getEndpointType() { return Type; }

  /// RingNetNIC: clock function
  virtual bool clockTick( Cycle_t cycle );

  /// RingNetNIC: callback function for the SimpleNetwork interface
  bool msgNotify( int virtualNetwork );

protected:
  SST::Output output;       ///< RingNetNIC: SST output object
  int         verbosity{};  ///< RingNetNIC: verbosity

  TimeConverter*                   timeConverter{};  ///< SST time conversion handler
  SST::Clock::Handler<RingNetNIC>* clockHandler{};   ///< Clock Handler
  SST::Interfaces::SimpleNetwork*  iFace{};          ///< RingNetNIC: SST network interface

  SST::Event::HandlerBase* msgHandler{};  ///< RingNetNIC: SST message handler

  zopCompID Type{ zopCompID::Z_UNK_COMP };  ///< RingNetNIC: zone component type
  bool      initBcastSent{};                ///< RingNetNIC: has the init bcast been sent?

  int numDest{};  ///< RingNetNIC: number of network destinations

  std::vector<uint64_t> endPoints;  ///< RingNetNIC: vector of endpoint IDs

  std::queue<SST::Interfaces::SimpleNetwork::Request*> sendQ;  ///< buffered send queue

};  // class RingNetNIC

}  // namespace SST::Forza

#endif  // _SST_RINGNET_H_

// EOF
