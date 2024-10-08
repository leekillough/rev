//
// _RingNet_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RingNet.h"

namespace SST::Forza {

//------------------------------------------
// RingNetNIC
//------------------------------------------
RingNetNIC::RingNetNIC( SST::ComponentId_t id, SST::Params& params ) : RingNetAPI( id, params ) {

  int verbosity = params.find<int>( "verbose", 0 );
  output.init( "RingNetNIC[" + getName() + ":@p:@t]: ", verbosity, 0, SST::Output::STDOUT );

  const std::string nicClock = params.find<std::string>( "clock", "1GHz" );
  iFace                      = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "iface", ComponentInfo::SHARE_NONE, 1 );

  clockHandler               = new SST::Clock::Handler<RingNetNIC>( this, &RingNetNIC::clockTick );
  output.output( "ringNIC[%s] Registering clock with frequency=%s\n", getName().c_str(), nicClock.c_str() );
  timeConverter = registerClock( nicClock, clockHandler );

  if( !iFace ) {
    // load the anonymous nic
    Params netparams;
    netparams.insert( "port_name", params.find<std::string>( "port", "network" ) );
    netparams.insert( "in_buf_size", "256B" );
    netparams.insert( "out_buf_size", "256B" );
    netparams.insert( "link_bw", "40GiB/s" );
    iFace = loadAnonymousSubComponent<SST::Interfaces::SimpleNetwork>(
      "merlin.linkcontrol", "iface", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, netparams, 1
    );
  }

  iFace->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<RingNetNIC>( this, &RingNetNIC::msgNotify ) );
}

RingNetNIC::~RingNetNIC() {}

void RingNetNIC::setMsgHandler( Event::HandlerBase* handler ) {
  msgHandler = handler;
}

void RingNetNIC::init( unsigned int phase ) {
  iFace->init( phase );

  if( Type == zopCompID::Z_UNK_COMP )
    output.fatal( CALL_INFO, -1, "Running init without setting the type\n" );

  // send the init data
  if( iFace->isNetworkInitialized() ) {
    if( !initBcastSent ) {
      initBcastSent = true;

      uint64_t id   = (uint64_t) ( iFace->getEndpointID() );
      output.verbose( CALL_INFO, 5, 0, "[FORZA] Broadcasting endpoint id=%" PRIu64 "\n", id );
      ringEvent*                               ev  = new ringEvent( Type, id );
      SST::Interfaces::SimpleNetwork::Request* req = new SST::Interfaces::SimpleNetwork::Request();
      req->dest                                    = SST::Interfaces::SimpleNetwork::INIT_BROADCAST_ADDR;
      req->src                                     = iFace->getEndpointID();
      req->givePayload( ev );
      iFace->sendUntimedData( req );
    }
  }

  // receive all the init data
  while( SST::Interfaces::SimpleNetwork::Request* req = iFace->recvUntimedData() ) {
    ringEvent* ev = static_cast<ringEvent*>( req->takePayload() );

    // decode the endpoint id
    uint64_t endP = ev->getDatum();
    output.verbose( CALL_INFO, 5, 0, "Receiving endpoint id=%" PRIu64 "\n", endP );

    endPoints.push_back( endP );
  }

  std::sort( endPoints.begin(), endPoints.end() );

  // sanity check the number of endpoints
  if( iFace->isNetworkInitialized() ) {
    if( !initBcastSent ) {
      if( endPoints.size() == 0 ) {
        output.fatal( CALL_INFO, -1, "%s : Error : minimum number of endpoints must be 2\n", getName().c_str() );
      }
    }
  }
}

void RingNetNIC::setup() {
  if( msgHandler == nullptr ) {
    output.fatal( CALL_INFO, -1, "%s : Error : RingNetNIC requires a callback-based notification\n", getName().c_str() );
  }
}

bool RingNetNIC::msgNotify( int vn ) {
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv( 0 );
  if( req != nullptr ) {
    ringEvent* ev = static_cast<ringEvent*>( req->takePayload() );
    delete req;
    ( *msgHandler )( ev );
  }
  return true;
}

void RingNetNIC::send( ringEvent* event, uint64_t destination ) {
  SST::Interfaces::SimpleNetwork::Request* req = new SST::Interfaces::SimpleNetwork::Request();
  req->dest                                    = destination;
  req->src                                     = iFace->getEndpointID();
  req->givePayload( event );
  sendQ.push( req );
}

unsigned RingNetNIC::getNumDestinations() {
  return endPoints.size();
}

SST::Interfaces::SimpleNetwork::nid_t RingNetNIC::getAddress() {
  return iFace->getEndpointID();
}

uint64_t RingNetNIC::getNextAddress() {
  uint64_t myAddr   = (uint64_t) ( getAddress() );
  uint64_t lastAddr = endPoints.back();
  output.verbose(
    CALL_INFO,
    11,
    0,
    "RingNet: my_addr=%lu, num_endpts=%u, lastAddr=%lu\n",
    myAddr,
    endPoints.size(),
    endPoints[( endPoints.size() - 1 )]
  );
  if( myAddr > lastAddr )
    return endPoints[0];

  for( unsigned i = 0; i < endPoints.size(); i++ )
    if( endPoints[i] > myAddr )
      return endPoints[i];

#if 0
  for( unsigned i = 0; i < endPoints.size(); i++ ) {
    output.verbose( CALL_INFO, 5, 0, "TJD: endpoint[%u]=%lu\n", i, endPoints[i] );
  }
  for( unsigned i = 0; i < endPoints.size(); i++ ) {
    if( endPoints[i] == myAddr ) {
      if( ( i + 1 ) <= ( endPoints.size() - 1 ) ) {
        return endPoints[i + 1];
      } else {
        return endPoints[0];
      }
    }
  }
  return 0;
#endif
  output.fatal( CALL_INFO, -1, "Failed to find a valid endPoint\n" );
}

bool RingNetNIC::clockTick( Cycle_t cycle ) {
  while( !sendQ.empty() ) {
    if( iFace->spaceToSend( 0, (int) ( sendQ.front()->size_in_bits ) ) && iFace->send( sendQ.front(), 0 ) ) {
      sendQ.pop();
    } else {
      break;
    }
  }
  return false;
}

}  // namespace SST::Forza

// EOF
