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

  /// zopEvent: virtual function to clone an event
  virtual Event* clone(void) override{
    zopEvent *ev = new zopEvent(*this);
    return ev;
  }

  /// zopEvent: retrieve the raw packet
  std::vector<uint32_t> getPacket() { return Packet; }

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
    {"port", "Port to use, if loaded as an anonymous subcomponent", "network"},
    {"verbose", "Verbosity for output (0 = nothing)", "0"},
  )

  SST_ELI_DOCUMENT_PORTS(
    {"network", "Port to network", {"simpleNetworkExample.nicEvent"} }
  )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork"}
  )

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
  SST::Output output;                       ///< zopNIC: SST output object
  SST::Interfaces::SimpleNetwork * iFace;   ///< zopNIC: SST network interface
  SST::Event::HandlerBase *msgHandler;      ///< zopNIC: SST message handler
  bool initBroadcastSent;                   ///< zopNIC: has the broadcast msg been sent
  unsigned numDest;                         ///< zopNIC: number of destination endpoints
  zopEndP Type;                             ///< zopNIC: endpoint type
  std::queue<SST::Interfaces::SimpleNetwork::Request*> sendQ; ///< zopNIC: buffered send queue
  std::map<SST::Interfaces::SimpleNetwork::nid_t,zopEndP> hostMap;  ///< zopNIC: network ID to endpoint type mapping
};  // zopNIC


} // namespace SST::RevCPU

#endif // _SST_ZOPNET_H_

// EOF
