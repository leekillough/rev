//
// _RevNIC_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVNIC_H_
#define _SST_REVNIC_H_

// -- Standard Headers
#include <queue>
#include <string>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

// -- SST Headers
#include "SST.h"

namespace SST::RevCPU {

/**
 * nicEvent : inherited class to handle the individual network events for RevNIC
 */
class nicEvent final : public SST::Event {
public:
  /// nicEvent: standard constructor
  explicit nicEvent( std::string name ) : Event(), SrcName( std::move( name ) ) {}

  /// nicEvent: extended constructor
  nicEvent( std::string name, std::vector<uint8_t> data ) : Event(), SrcName( std::move( name ) ), Data( std::move( data ) ) {}

  /// nicEvent: retrieve the source name
  std::string getSource() { return SrcName; }

  // nicEvent: retrieve the data payload
  std::vector<uint8_t> getData() { return Data; }

  /// nicEvent: virtual function to clone an event
  Event* clone() final {
    nicEvent* ev = new nicEvent( *this );
    return ev;
  }

private:
  std::string          SrcName{};  ///< nicEvent: Name of the sending device
  std::vector<uint8_t> Data{};     ///< nicEvent: Data payload

public:
  /// nicEvent: secondary constructor
  nicEvent()        = default;
  ~nicEvent() final = default;

  /// nicEvent: event serializer
  void serialize_order( SST::Core::Serialization::serializer& ser ) final {
    Event::serialize_order( ser );
    ser& SrcName;
    ser& Data;
  }

  /// nicEvent: implements the NIC serialization
  ImplementSerializable( SST::RevCPU::nicEvent );
};  // end nicEvent

/**
 * nicAPI : Handles the subcomponent NIC API
 */
class nicAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::RevCPU::nicAPI )

  /// nicEvent: constructor
  nicAPI( ComponentId_t id, Params& ) : SubComponent( id ) {}

  /// nicEvent: default destructor
  ~nicAPI() override                                         = default;

  /// nicEvent: registers the event handler with the core
  virtual void setMsgHandler( Event::HandlerBase* handler )  = 0;

  /// nicEvent: initializes the network
  void init( uint32_t phase ) override                       = 0;

  /// nicEvent: setup the network
  void setup() override                                      = 0;

  /// nicEvent: send a message on the network
  virtual void send( nicEvent* ev, int dest )                = 0;

  /// nicEvent: retrieve the number of potential destinations
  virtual int getNumDestinations()                           = 0;

  /// nicEvent: returns the NIC's network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress() = 0;
};  /// end nicAPI

/**
 * RevNIC: the Rev network interface controller subcomponent
 */
class RevNIC final : public nicAPI {
public:
  // Register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    RevNIC, "revcpu", "RevNIC", SST_ELI_ELEMENT_VERSION( 1, 0, 0 ), "RISC-V SST NIC", SST::RevCPU::nicAPI
  )

  // Register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "clock", "Clock frequency of the NIC", "1Ghz" },
    { "port", "Port to use, if loaded as an anonymous subcomponent", "network" },
    { "verbose", "Verbosity for output (0 = nothing)", "0" },
  )

  // Register the ports
  SST_ELI_DOCUMENT_PORTS( { "network", "Port to network", { "simpleNetworkExample.nicEvent" } } )

  // Register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( { "iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork" } )

  /// RevNIC: constructor
  RevNIC( ComponentId_t id, Params& params );

  /// RevNIC: destructor
  ~RevNIC() final;

  /// RevNIC: Callback to parent on received messages
  void setMsgHandler( Event::HandlerBase* handler ) final;

  /// RevNIC: initialization function
  void init( uint32_t phase ) final;

  /// RevNIC: setup function
  void setup() final;

  /// RevNIC: send event to the destination id
  void send( nicEvent* ev, int dest ) final;

  /// RevNIC: retrieve the number of destinations
  int getNumDestinations() final;

  /// RevNIC: get the endpoint's network address
  SST::Interfaces::SimpleNetwork::nid_t getAddress() final;

  /// RevNIC: callback function for the SimpleNetwork interface
  bool msgNotify( int virtualNetwork );

  /// RevNIC: clock function
  virtual bool clockTick( Cycle_t cycle );

protected:
  SST::Output*                    output{};             ///< RevNIC: SST output object
  SST::Interfaces::SimpleNetwork* iFace{};              ///< RevNIC: SST network interface
  SST::Event::HandlerBase*        msgHandler{};         ///< RevNIC: SST message handler
  bool                            initBroadcastSent{};  ///< RevNIC: has the init bcast been sent?
  int                             numDest{};            ///< RevNIC: number of SST destinations

  std::queue<SST::Interfaces::SimpleNetwork::Request*> sendQ{};  ///< RevNIC: buffered send queue

  /// RevNIC: disallow copying and assignment
  RevNIC( const RevNIC& )            = delete;
  RevNIC& operator=( const RevNIC& ) = delete;
};  // end RevNIC

}  // namespace SST::RevCPU

#endif  // _SST_REVNIC_H_

// EOF
