//
// _ZOPNet_h_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_ZOPNET_H_
#define _SST_ZOPNET_H_

// -- Standard Headers
#include <vector>
#include <queue>
#include <map>
#include <unistd.h>

// -- SST Headers
#include "SST.h"

namespace SST::RevCPU{

// --------------------------------------------
// zopMsgT
// --------------------------------------------
enum class zopMsgT : uint8_t {
  Z_MZOP  = 0b0000,   /// FORZA MZOP
  Z_HZOP  = 0b0001,   /// FORZA HZOP
  Z_RZOP  = 0b0010,   /// FORZA RZOP
  Z_MSG   = 0b0011,   /// FORZA MESSAGING
  Z_TMG   = 0b0100,   /// FORZA THREAD MIGRATION
  Z_ACK   = 0b0101,   /// FORZA ACK
  Z_NACK  = 0b0110,   /// FORZA NACK
  Z_SYSC  = 0b0111,   /// FORZA SYSCALL
  // -- 0b1000 - 0b1110 UNASSIGNED
  Z_EXCP  = 0b1111,   /// FORZA EXCEPTION
};

// --------------------------------------------
// zopOpc
// --------------------------------------------
enum class zopOpc : uint8_t {
  // -- MZOPs --
  Z_NULL_OPC  = 0b00000000,   /// FORZA NULL OPCODE: DO NOT USE
  Z_RZA_LOAD  = 0b00000000,   /// FORZA RZA LOAD OPERATION
  Z_RZA_STORE = 0b00001000,   /// FORZA RZA STORE OPERATION
  Z_RZA_SEXT  = 0b00000100,   /// FORZA RZA SIGN EXTENSION (LOAD)
  Z_RZA_BYTE  = 0b00000000,   /// FORZA BYTE OPERATION
  Z_RZA_HW    = 0b00000001,   /// FORZA HALF-WORD OPERATION
  Z_RZA_W     = 0b00000010,   /// FORZA WORD OPERATION
  Z_RZA_DW    = 0b00000011,   /// FORZA DOUBLEWORD OPERATION

  // -- HZOPs --
  Z_AMOADD    = 0b00000000,   /// FORZA ATOMIC ADD
  Z_AMOAND    = 0b00001000,   /// FORZA ATOMIC AND
  Z_AMOOR     = 0b00010000,   /// FORZA ATOMIC OR
  Z_AMOXOR    = 0b00011000,   /// FORZA ATOMIC XOR
  Z_AMOSMAX   = 0b00100000,   /// FORZA SIGNED INTEGER MAX
  Z_AMOUMAX   = 0b00101000,   /// FORZA UNSIGNED INTEGER MAX
  Z_AMOSMIN   = 0b00110000,   /// FORZA SIGNED INTEGER MIN
  Z_AMOUMIN   = 0b00111000,   /// FORZA UNSIGNED INTEGER MIN
  Z_AMOSWAP   = 0b01000000,   /// FORZA SWAP
  Z_AMOCSWAP  = 0b01001000,   /// FORZA COMPARE & SWAP
  Z_AMOFADD   = 0b01010000,   /// FORZA ATOMIC FLOAT ADD
  Z_AMOFSUB   = 0b01100000,   /// FORZA ATOMIC FLOAT SUB (A-B)
  Z_AMOFSUBR  = 0b01101000,   /// FORZA ATOMIC FLOAT SUB REVERSE (B-A)

  Z_AMO_NONE  = 0b00000000,   /// FORZA ATOMIC NONE TYPE
  Z_AMO_M     = 0b00000010,   /// FORZA ATOMIC 'M' TYPE [AMO OP & FETCH]
  Z_AMO_S     = 0b00000100,   /// FORZA ATOMIC 'S' TYPE [AMO FETCH SUM & REPLACE]
  Z_AMO_MS    = 0b00000110,   /// FORZA ATOMIC 'MS' TYPE [AMO FETCH & OP]

  Z_AMOWORD   = 0b00000000,   /// FORZA AMO SIZE = WORD (32bit)
  Z_AMODWORD  = 0b00000001,   /// FORZA AMO SIZE = DOUBLEWORD (64bit)

  // -- MSG'ing --
  Z_SEND      = 0b00000000,   /// FORZA MESSAGE SEND
  Z_CREDIT    = 0b00000001,   /// FORZA CREDIT REPLENISH
  Z_ZENSETUP  = 0b00000010,   /// FORZA ZEN SETUP
};

// --------------------------------------------
// zopEndP
// --------------------------------------------
enum class zopEndP : uint32_t {
  Z_ZAP = 1u << 0,    /// FORZA ZAP endpoint
  Z_ZEN = 1u << 1,    /// FORZA ZEN endpoint
  Z_RZA = 1u << 2,    /// FORZA RZA endpoint
  Z_ZQM = 1u << 3,    /// FORZA ZQM endpoint
};

// --------------------------------------------
// zopEvent
// --------------------------------------------
class zopEvent : public SST::Event{
public:
  /// zopEvent: standard constructor
  explicit zopEvent(unsigned srcId, unsigned destId)
    : Event(){
    // build the header
    Packet.push_back(0x00ul);
    Packet.push_back(destId);
    Packet.push_back(srcId);
    Packet.push_back(0x00ul);
  }

  /// zopEvent: init broadcast constructor
  explicit zopEvent(unsigned srcId, zopEndP Type )
    : Event(){
    Packet.push_back((uint32_t)(Type));
    Packet.push_back(0x00ul);
    Packet.push_back(srcId);
    Packet.push_back(0x00ul);
  }

  explicit zopEvent(zopMsgT Type, zopOpc Opc)
    : Event(){
    Packet.push_back(((uint32_t)(Type) << 28) | (uint32_t)(Opc));
  }

  /// zopEvent: virtual function to clone an event
  virtual Event* clone(void) override{
    zopEvent *ev = new zopEvent(*this);
    return ev;
  }

  /// zopEvent: retrieve the raw packet
  std::vector<uint32_t> const getPacket() { return Packet; }

  /// zopEvent: set the packet payload.  NOTE: this is a destructive operation
  void setPacket(const std::vector<uint32_t> P){
    Packet.clear();
    for( auto i : P ){
      Packet.push_back(i);
    }
    unsigned NumFlits = (Packet.size()-4)/2;
    Packet[0] |= (NumFlits << 18);
  }

  /// zopEvent: clear the packet payload
  void clearPacket(){
    Packet.clear();
  }

  /// zopEvent: set the source ID
  void setSrc(uint32_t srcID){
    Packet[2] = srcID;
  }

  /// zopEvent: set the destination ID
  void setDest(uint32_t destID){
    Packet[1] = destID;
  }

private:
  std::vector<uint32_t> Packet; ///< zopEvent: data payload

public:
  // zopEvent: secondary constructor
  zopEvent() : Event() {}

  // zopEvent: event serializer
  void serialize_order(SST::Core::Serialization::serializer &ser) override{
    Event::serialize_order(ser);
    ser & Packet;
  }

  // zopEvent: implements the nic serialization
  ImplementSerializable(SST::RevCPU::zopEvent);

};  // class zopEvent

// --------------------------------------------
// zopAPI
// --------------------------------------------
class zopAPI : public SST::SubComponent{
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API(SST::RevCPU::zopAPI)

  /// zopAPI: constructor
  zopAPI(ComponentId_t id, Params& params) : SubComponent(id) { }

  /// zopAPI: destructor
  virtual ~zopAPI() = default;

  /// zopAPI: registers the the event handler with the core
  virtual void setMsgHandler(Event::HandlerBase* handler) = 0;

  /// zopAPI: initializes the network
  virtual void init(unsigned int phase) = 0;

  /// zopAPI : setup the network
  virtual void setup() { }

  /// zopAPI : send a message on the network
  virtual void send(zopEvent *ev, zopEndP dest) = 0;

  /// zopAPI: send a message on the network
  virtual void send(zopEvent *ev, uint32_t dest) = 0;

  /// zopAPI : retrieve the number of potential endpoints
  virtual unsigned getNumDestinations() = 0;

  /// zopAPI: return the NIC's network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress() = 0;

  /// zopAPI: set the type of the endpoint
  virtual void setEndpointType(zopEndP type) = 0;

  /// zopAPI: get the type of the endpoint
  virtual zopEndP getEndpointType() = 0;

  /// zopAPI: convert endpoint to string name
  std::string const endPToStr(zopEndP T){
    switch( T ){
    case zopEndP::Z_ZAP:
      return "ZAP";
      break;
    case zopEndP::Z_ZEN:
      return "ZEN";
      break;
    case zopEndP::Z_RZA:
      return "RZA";
      break;
    case zopEndP::Z_ZQM:
      return "ZQM";
      break;
    default:
      return "UNK";
      break;
    }
  }

  /// zopAPI : convert message type to string name
  std::string const msgTToStr(zopMsgT T){
    switch( T ){
    case zopMsgT::Z_MZOP:
      return "MZOP";
      break;
    case zopMsgT::Z_HZOP:
      return "HZOP";
      break;
    case zopMsgT::Z_RZOP:
      return "RZOP";
      break;
    case zopMsgT::Z_MSG:
      return "MSG";
      break;
    case zopMsgT::Z_TMG:
      return "TMG";
      break;
    case zopMsgT::Z_ACK:
      return "ACK";
      break;
    case zopMsgT::Z_NACK:
      return "NACK";
      break;
    case zopMsgT::Z_SYSC:
      return "SYSC";
      break;
    case zopMsgT::Z_EXCP:
      return "EXCP";
      break;
    default:
      return "UNK";
      break;
    }
  }
};

// --------------------------------------------
// zopNIC
// --------------------------------------------
class zopNIC : public zopAPI {
public:
  // register ELI with the SST core
  SST_ELI_REGISTER_SUBCOMPONENT(
    zopNIC,
    "revcpu",
    "zopNIC",
    SST_ELI_ELEMENT_VERSION(1, 0, 0),
    "FORZA ZOP NIC",
    SST::RevCPU::zopAPI
  )

  SST_ELI_DOCUMENT_PARAMS(
    {"clock", "Clock frequency of the NIC", "1GHz"},
    {"req_per_cycle", "Max requests to dispatch per cycle", "1"},
    {"port", "Port to use, if loaded as an anonymous subcomponent", "network"},
    {"verbose", "Verbosity for output (0 = nothing)", "0"},
  )

  SST_ELI_DOCUMENT_PORTS(
    {"network", "Port to network", {"simpleNetworkExample.nicEvent"} }
  )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork"}
  )

  SST_ELI_DOCUMENT_STATISTICS(
    {"BytesSent",       "Number of bytes sent",     "bytes",    1},
    {"MZOPSent",        "Number of MZOPs sent",     "count",    1},
    {"HZOPSent",        "Number of HZOPs sent",     "count",    1},
    {"RZOPSent",        "Number of RZOPs sent",     "count",    1},
    {"MSGSent",         "Number of MSGs sent",      "count",    1},
    {"ACKSent",         "Number of ACKs sent",      "count",    1},
    {"NACKSent",        "Number of NACKs sent",     "count",    1},
    {"SYSCSent",        "Number of Syscalls sent",  "count",    1},
    {"EXCPSent",        "Number of Exceptions sent","count",    1},
  )

  enum zopStats : uint32_t {
    BytesSent     = 0,
    MZOPSent      = 1,
    HZOPSent      = 2,
    RZOPSent      = 3,
    MSGSent       = 4,
    ACKSent       = 5,
    NACKSent      = 6,
    SYSCSent      = 7,
    EXCPSent      = 8,
  };

  /// zopNIC: constructor
  zopNIC(ComponentId_t id, Params& params);

  /// zopNIC: destructor
  virtual ~zopNIC();

  /// zopNIC: callback to parent on received messages
  virtual void setMsgHandler(Event::HandlerBase* handler);

  /// zopNIC: initialization function
  virtual void init(unsigned int phase);

  /// zopNIC: setup function
  virtual void setup();

  /// zopNIC: send an event to the destination id
  virtual void send(zopEvent *ev, zopEndP dest );

  /// zopNIC: send an event to the destination id
  virtual void send(zopEvent *ev, uint32_t dest );

  /// zopNIC: get the number of destinations
  virtual unsigned getNumDestinations();

  /// zopNIC: get the end network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress();

  /// zopNIC: set the endpoint type
  virtual void setEndpointType(zopEndP type) { Type = type; }

  /// zopNic: get the endpoint type
  virtual zopEndP getEndpointType() { return Type; }

  /// zopNIC: callback function for the SimpleNetwork interface
  bool msgNotify(int virtualNetwork);

  /// zopNIC: clock tick function
  virtual bool clockTick(Cycle_t cycle);

private:
  void registerStats();

  SST::Output output;                       ///< zopNIC: SST output object
  SST::Interfaces::SimpleNetwork * iFace;   ///< zopNIC: SST network interface
  SST::Event::HandlerBase *msgHandler;      ///< zopNIC: SST message handler
  bool initBroadcastSent;                   ///< zopNIC: has the broadcast msg been sent
  unsigned numDest;                         ///< zopNIC: number of destination endpoints
  unsigned ReqPerCycle;                     ///< zopNIC: max requests to send per cycle
  zopEndP Type;                             ///< zopNIC: endpoint type

  std::queue<SST::Interfaces::SimpleNetwork::Request*> sendQ; ///< zopNIC: buffered send queue
  std::map<SST::Interfaces::SimpleNetwork::nid_t,zopEndP> hostMap;  ///< zopNIC: network ID to endpoint type mapping

  std::vector<Statistic<uint64_t>*> stats;  ///< zopNIC: statistics vector
};  // zopNIC


} // namespace SST::RevCPU

#endif // _SST_ZOPNET_H_

// EOF
