//
// _ZOPNET_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "ZOPNET.h"

namespace SST::Forza {

zopNIC::zopNIC( ComponentId_t id, Params& params )
  : zopAPI( id, params ), iFace( nullptr ), msgHandler( nullptr ), initBroadcastSent( false ), numDest( 0 ), numHarts( 0 ),
    Precinct( 0 ), Zone( 0 ), Type( zopCompID::Z_ZAP0 ), enableTestHarness( false ), msgId( nullptr ), HARTFence( nullptr ),
    barrierSense( nullptr ) {

  // read the parameters
  int verbosity = params.find<int>( "verbose", 0 );
  output.init( "zopNIC[" + getName() + ":@p:@t]: ", verbosity, 0, SST::Output::STDOUT );
  ReqPerCycle       = params.find<unsigned>( "req_per_cycle", 1 );

  enableTestHarness = params.find<bool>( "enableTestHarness", false );
  numZones          = params.find<unsigned>( "numZones", 8 );
  numPrecincts      = params.find<unsigned>( "numPrecincts", 1 );

  // register the stats
  registerStats();

  // register the clock handler
  const std::string cpuFreq = params.find<std::string>( "clock", "1GHz" );
  registerClock( cpuFreq, new Clock::Handler<zopNIC>( this, &zopNIC::clockTick ) );
  output.output( "zopNIC[%s] Registering clock with frequency=%s\n", getName().c_str(), cpuFreq.c_str() );

  // load the SimpleNetwork interfaces
  iFace = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "iface", ComponentInfo::SHARE_NONE, 1 );
  if( !iFace ) {
    // load the anonymous NIC
    Params netparams;
    netparams.insert( "port_name", params.find<std::string>( "port", "network" ) );
    netparams.insert( "in_buf_size", "2048B" );
    netparams.insert( "out_buf_size", "2048B" );
    netparams.insert( "link_bw", "100GiB/s" );
    iFace = loadAnonymousSubComponent<SST::Interfaces::SimpleNetwork>(
      "merlin.linkcontrol", "iface", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, netparams, 1
    );
  }

  // setup the notification function
  iFace->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<zopNIC>( this, &zopNIC::msgNotify ) );
}

zopNIC::~zopNIC() {
  if( msgId )
    delete[] msgId;
  if( HARTFence )
    delete[] HARTFence;
  if( barrierSense )
    delete[] barrierSense;
  for( unsigned i = 0; i < 2; i++ ) {
    if( zoneBarrier[i] ) {
      delete[] zoneBarrier[i];
    }
    if( barrierEndpoints[i] ) {
      delete[] barrierEndpoints[i];
    }
  }
}

void zopNIC::setNumHarts( unsigned H ) {
  numHarts     = H;
  msgId        = new SST::Forza::zopMsgID[numHarts];
  HARTFence    = new unsigned[numHarts];
  barrierSense = new unsigned[numHarts];
  zoneBarrier.push_back( new unsigned[numHarts] );
  zoneBarrier.push_back( new unsigned[numHarts] );
  barrierEndpoints.push_back( new unsigned[numHarts] );
  barrierEndpoints.push_back( new unsigned[numHarts] );
  for( unsigned i = 0; i < numHarts; i++ ) {
    HARTFence[i]           = 0;
    barrierSense[i]        = 0;
    zoneBarrier[0][i]      = 0;
    zoneBarrier[1][i]      = 0;
    barrierEndpoints[0][i] = 0;
    barrierEndpoints[1][i] = 0;
  }
}

void zopNIC::registerStats() {
  for( auto* stat : {
         "BytesSent",
         "MZOPSent",
         "HZOPACSent",
         "HZOPVSent",
         "RZOPSent",
         "MSGSent",
         "TMIGSent",
         "TMGTSent",
         "SYSCSent",
         "RESPSent",
         "FENCESent",
         "EXCPSent",
       } ) {
    stats.push_back( registerStatistic<uint64_t>( stat ) );
  }
}

void zopNIC::recordStat( zopNIC::zopStats Stat, uint64_t Data ) {
  if( Stat > zopNIC::zopStats::EXCPSent ) {
    return;
  }

  stats[Stat]->addData( Data );
}

zopNIC::zopStats zopNIC::getStatFromPacket( zopEvent* ev ) {
  auto Packet = ev->getPacket();
  if( Packet.size() == 0 ) {
    output.fatal( CALL_INFO, -1, "Error: recording stat for a null packet\n" );
  }

  zopMsgT Type = ev->getType();
  switch( Type ) {
  case zopMsgT::Z_MZOP: return zopStats::MZOPSent; break;
  case zopMsgT::Z_HZOPAC: return zopStats::HZOPACSent; break;
  //case zopMsgT::Z_HZOPV: return zopStats::HZOPVSent; break;
  //case zopMsgT::Z_RZOP: return zopStats::RZOPSent; break;
  case zopMsgT::Z_MSG: return zopStats::MSGSent; break;
  case zopMsgT::Z_TMIG: return zopStats::TMIGSent; break;
  //case zopMsgT::Z_TMGT: return zopStats::TMGTSent; break;
  //case zopMsgT::Z_SYSC: return zopStats::SYSCSent; break;
  case zopMsgT::Z_RESP: return zopStats::RESPSent; break;
  case zopMsgT::Z_FENCE: return zopStats::FENCESent; break;
  case zopMsgT::Z_EXCP: return zopStats::EXCPSent; break;
  default: output.fatal( CALL_INFO, -1, "Error: unknown packet type=%" PRIu8 "\n", static_cast<uint8_t>( Type ) ); break;
  }

  // we should never reach this point
  return zopStats::MZOPSent;
}

void zopNIC::setMsgHandler( Event::HandlerBase* handler ) {
  msgHandler = handler;
}

void zopNIC::setup() {
  if( msgHandler == nullptr ) {
    output.fatal(
      CALL_INFO,
      -1,
      "%s, Error: zopNIC implements a callback based notification "
      "and the parent has not registered a callback function\n",
      getName().c_str()
    );
  }
}

void zopNIC::init( unsigned int phase ) {

  output.verbose( CALL_INFO, 7, 0, "%s initializing interface at phase %d\n", getName().c_str(), phase );

  // pass the init on to the actual interface
  if( !iFace ) {
    output.fatal( CALL_INFO, -1, "%s, Error : network interface is null\n", getName().c_str() );
  }
  iFace->init( phase );

  // determine if we need to send the discovery broadcast message
  if( iFace->isNetworkInitialized() ) {
    if( !initBroadcastSent ) {
      initBroadcastSent = true;
      zopEvent* ev = new zopEvent( iFace->getEndpointID(), getEndpointType(), unsigned( getPCID( getZoneID() ) ), getPrecinctID() );
      SST::Interfaces::SimpleNetwork::Request* req = new SST::Interfaces::SimpleNetwork::Request();
      req->dest                                    = SST::Interfaces::SimpleNetwork::INIT_BROADCAST_ADDR;
      req->src                                     = iFace->getEndpointID();
      req->givePayload( ev );
      iFace->sendUntimedData( req );

      // add myself to the local endpoint table
      hostMap.emplace( iFace->getEndpointID(), std::make_tuple( getEndpointType(), getPCID( getZoneID() ), getPrecinctID() ) );
    }
  }

  // receive all the broadcast messages
  while( SST::Interfaces::SimpleNetwork::Request* req = iFace->recvUntimedData() ) {
    zopEvent* ev = static_cast<zopEvent*>( req->takePayload() );
    numDest++;
    SST::Interfaces::SimpleNetwork::nid_t srcID = req->src;
    std::vector<uint64_t>                 Pkt   = ev->getPacket();
    auto                                  t     = std::make_tuple(
      static_cast<zopCompID>( Pkt[0] & Z_MASK_TYPE ),
      static_cast<zopPrecID>( Pkt[2] & Z_MASK_PCID ),
      static_cast<uint32_t>( Pkt[3] & Z_MASK_PRECINCT )
    );
    hostMap.emplace( srcID, t );
    output.verbose(
      CALL_INFO,
      7,
      0,
      "%s received init broadcast messages from %" PRId64 " of "
      "[Type][PCID][Precinct]: [%s][%" PRIu8 "][%" PRIu32 "]\n",
      getName().c_str(),
      srcID,
      endPToStr( std::get<_HM_ENDP_T>( t ) ).c_str(),
      static_cast<uint8_t>( std::get<_HM_ZID>( t ) ),
      std::get<_HM_PID>( t )
    );
    delete ev;
  }

  // --- begin print out the host mapping table
  if( phase == 4 ) {
    output.verbose( CALL_INFO, 9, 0, "------------------------------------------------------\n" );
    output.verbose( CALL_INFO, 9, 0, "               ZOP NETWORK MAPPING\n" );
    output.verbose( CALL_INFO, 9, 0, "------------------------------------------------------\n" );

    for( auto const& [key, val] : hostMap ) {
      output.verbose(
        CALL_INFO,
        9,
        0,
        "Endpoint ID=%" PRId64 " is of [Type][PCID][Precinct] = [%s][%" PRIu8 "][%" PRIu32 "]\n",
        key,
        endPToStr( std::get<_HM_ENDP_T>( val ) ).c_str(),
        static_cast<uint8_t>( std::get<_HM_ZID>( val ) ),
        std::get<_HM_PID>( val )
      );
    }

    output.verbose( CALL_INFO, 9, 0, "------------------------------------------------------\n" );
    // we first walk the preInitQ to see if there are any pending requests
    // these requests are queued before the network has been fully initialized
    if( preInitQ.size() > 0 ) {
      output.verbose( CALL_INFO, 9, 0, "%s processing pre-init messages\n", getName().c_str() );
      for( auto& e : preInitQ ) {
        send( e.first, e.second );
      }
      preInitQ.clear();
    }
  }
  // --- end print out the host mapping table
}

bool zopNIC::hasBarrier( unsigned Hart ) {
  if( barrierEndpoints[barrierSense[Hart]][Hart] > 0 ) {
    return true;
  }
  return false;
}

bool zopNIC::isBarrierComplete( unsigned Hart ) {
  if( zoneBarrier[barrierSense[Hart]][Hart] == barrierEndpoints[barrierSense[Hart]][Hart] ) {
    // completed the barrier, switch the sense
    zoneBarrier[barrierSense[Hart]][Hart]      = 0;
    barrierEndpoints[barrierSense[Hart]][Hart] = 0;
    if( barrierSense[Hart] == 0 ) {
      barrierSense[Hart] = 1;
    } else {
      barrierSense[Hart] = 0;
    }
    return true;
  }

  // barrier not complete
  return false;
}

void zopNIC::send_zone_barrier( unsigned Hart, unsigned endpoints ) {
  output.verbose( CALL_INFO, 9, 0, "Injecting zone barrier with sense=%u\n", barrierSense[Hart] );
  // setup the barrier
  barrierEndpoints[barrierSense[Hart]][Hart] = endpoints;

  // walk the zone network destinations and send a packet to every ZAP
  for( auto i : hostMap ) {
    auto t = i.second;
    if( ( (uint8_t) ( std::get<_HM_ENDP_T>( t ) ) < (uint8_t) ( SST::Forza::zopCompID::Z_RZA ) ) &&
        ( (unsigned) ( std::get<_HM_ZID>( t ) ) == Zone ) && ( std::get<_HM_PID>( t ) == Precinct ) ) {
      // found a candidate target
      auto realDest = i.first;

      // create the packet
      zopEvent* ev  = new zopEvent();
      ev->setType( SST::Forza::zopMsgT::Z_MSG );
      ev->setOpc( SST::Forza::zopOpc::Z_MSG_ZBAR );
      ev->setAppID( 0 );
      ev->setDestHart( 0 );                                            // ignored
      ev->setDestZCID( (uint8_t) ( SST::Forza::zopCompID::Z_ZAP0 ) );  // ignored
      ev->setDestPCID( (uint8_t) ( getPCID( getZoneID() ) ) );
      ev->setDestPrec( (uint8_t) ( getPrecinctID() ) );
      ev->setSrcHart( Hart );
      ev->setSrcZCID( (uint8_t) ( getEndpointType() ) );
      ev->setSrcPCID( (uint8_t) ( getPCID( getZoneID() ) ) );
      ev->setSrcPrec( (uint8_t) ( getPrecinctID() ) );

      std::vector<uint64_t> payload;
      payload.push_back( (uint64_t) ( barrierSense[Hart] ) );
      ev->setPayload( payload );
      ev->encodeEvent();

      // create the request
      // this may be broken... send individual requests to every other ZAP
      SST::Interfaces::SimpleNetwork::Request* req = new SST::Interfaces::SimpleNetwork::Request();
      req->dest                                    = realDest;
      req->src                                     = iFace->getEndpointID();
      req->givePayload( ev );

      // inject the request
      sendQ.push_back( req );
    }
  }

  // signal my local barrier
  for( unsigned i = 0; i < numHarts; i++ ) {
    zoneBarrier[barrierSense[Hart]][i]++;
  }
}

void zopNIC::send( zopEvent* ev, zopCompID dest ) {
  // assuming we are sending within a zone+precinct
  send( ev, dest, ev->getPCID( ev->getDestPCID() ), ev->getDestPrec() );
}

void zopNIC::send( zopEvent* ev, zopCompID dest, zopPrecID zone, unsigned prec ) {
  zopCompID TmpDest = dest;
  if( !initBroadcastSent ) {
    output.verbose(
      CALL_INFO,
      9,
      0,
      "Buffering message from %s to endpoint "
      "[hart:zcid:pcid:type]=[%" PRIu16 ":%" PRIu8 ":%" PRIu8 ":%s] before network boots\n",
      getName().c_str(),
      ev->getDestHart(),
      ev->getDestZCID(),
      ev->getDestPCID(),
      endPToStr( TmpDest ).c_str()
    );
    preInitQ.push_back( std::make_pair( ev, TmpDest ) );
    return;
  }

  // check to see whether the designated destination is within the same precinct
#if 0
  if( (getEndpointType() == Forza::zopCompID::Z_ZEN) &&
      (prec != getPrecinctID()) ){
    // this is a ZEN device and the target location is outside the local precint
    // route the request to the ZIP device
    ev->setDestPCID((uint8_t)(Forza::zopPrecID::Z_ZIP));
  }
#endif

  SST::Interfaces::SimpleNetwork::Request* req = new SST::Interfaces::SimpleNetwork::Request();
#if 0  // debugging print block; saving since I frequently need something like this
  if ( ( ev->getType() == SST::Forza::zopMsgT::Z_MZOP ) &&
       ( ev->getSrcPCID() != ev->getDestPCID() ) )
    {
    output.verbose(
      CALL_INFO,
      3,
      0,
      "TJD: Sending message from %s @ endpoint=%" PRId64 " with "
      "msg_id=%" PRIu16 " and %s to %s\n; z=%u, Z=%u, p=%u, P=%u\n",
      getName().c_str(),
      getAddress(),
      ev->getID(),
      ev->getSrcString().c_str(),
      ev->getDestString().c_str(),
      (unsigned)zone,
      Zone,
      prec,
      Precinct
    );
  }
#endif
  auto realDest = 0;
  if( ( (unsigned) zone == Zone ) && ( prec == Precinct ) ) {
    if( ev->getDestZCID() <= (uint8_t) SST::Forza::zopCompID::Z_ZAP3 && ev->getType() == SST::Forza::zopMsgT::Z_MSG ) {
      if( ev->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDP )
        TmpDest = zopCompID::Z_ZQM;  // any send message so go to a zqm
      else if( ev->getOpc() == SST::Forza::zopOpc::Z_MSG_ACK || ev->getOpc() == SST::Forza::zopOpc::Z_MSG_NACK )
        TmpDest = zopCompID::Z_ZEN;  // any msg ack/nack goes to a zen
    }
  } else if( prec == Precinct ) {  // going to a diff zone within the precinct
    /* Breadcrumbs...
     * This component serves as both the zone NoC and the precinct NoC - this leads to some unique logic (such as below)
     * For this else block, we have packets that are leaving this zone; if zopnet is in use as a zone NoC, we need
     * to redirect them to the ZEN within this zone (hence the redirect to local zone).  The ZEN is also the
     * block that connects each zone to the precinct NoC, so when zopnet is a precinct noc, we need
     * to also target a ZEN else there is no matching endpoint in the hostMap.
     */
    if( Type != zopCompID::Z_ZEN ) {
      zone = getPCID( Zone );  // redirect to local zone
    }
    TmpDest = zopCompID::Z_ZEN;
  } else {
    // Todo: Will need to redirect to local ZEN to be shoved out to the precinct noc
    output.fatal( CALL_INFO, -1, "Packet should be for a diff precinct - not yet supported\n" );
  }

  bool fnd_dest = false;
  for( auto i : hostMap ) {
    auto t = i.second;
    if( ( std::get<_HM_ENDP_T>( t ) == TmpDest ) && ( std::get<_HM_ZID>( t ) == zone ) && ( std::get<_HM_PID>( t ) == prec ) ) {
      realDest = i.first;
      fnd_dest = true;
    }
  }

  if( !fnd_dest ) {
    output.fatal(
      CALL_INFO,
      -1,
      "%s did not find hostMap destination for packet %s to %s\n",
      getName().c_str(),
      ev->getSrcString().c_str(),
      ev->getDestString().c_str()
    );
  }

  ev->encodeEvent();
  req->dest = realDest;  // FIXME - what needs fixed here?
  req->src  = getAddress();
  req->givePayload( ev );
  sendQ.push_back( req );
}

void zopNIC::handleLoadResponse(
  zopEvent* ev, uint64_t* Target, SST::Forza::zopOpc Opc, SST::RevCPU::MemReq Req, uint8_t ID, uint16_t SrcHart
) {
  /**
   * Notes
   * - Target argument is a host system pointer for a location in the requesting thread's reg file
   */

  // make sure this request is a response from an MZOP or HZOP RZA pipe
  if( SrcHart != Z_MZOP_PIPE_HART && SrcHart != Z_HZOP_PIPE_HART ) {
    output.fatal(
      CALL_INFO,
      -1,
      "%s, Error: Unknown FLIT Source Hart ID %" PRIu16 "; "
      "OPC=%" PRIu8 ", LENGTH=%" PRIu8 ", ID=%" PRIu8 "; PACKET %s to %s\n",
      getName().c_str(),
      SrcHart,
      static_cast<uint8_t>( ev->getOpc() ),
      ev->getLength(),
      ID,
      ev->getSrcString().c_str(),
      ev->getDestString().c_str()
    );
  }

  // retrieve the data from the response payload
  if( !ev->getFLIT( Z_FLIT_DATA_RESP, Target ) ) {
    output.fatal(
      CALL_INFO,
      -1,
      "%s, Error: zopEvent on zopNIC failed to read response FLIT; "
      "OPC=%" PRIu8 ", LENGTH=%" PRIu8 ", ID=%" PRIu8 "; PACKET %s to %s\n",
      getName().c_str(),
      static_cast<uint8_t>( ev->getOpc() ),
      ev->getLength(),
      ID,
      ev->getSrcString().c_str(),
      ev->getDestString().c_str()
    );
  }

  // setup the zopEvent infrastructure to clear the hazards
  ev->setMemReq( Req );
  ev->setTarget( Target );

  // trigger the RevCPU to clear the hazard
  ( *msgHandler )( ev );
}

bool zopNIC::msgNotify( int vn ) {
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv( 0 );
  if( req == nullptr ) {
    return false;
  }

  zopEvent* ev = static_cast<zopEvent*>( req->takePayload() );
  if( ev == nullptr ) {
    output.fatal( CALL_INFO, -1, "%s, Error: zopEvent on zopNIC is null\n", getName().c_str() );
  }

  auto P = ev->getPacket();

  // decode the event
  ev->decodeEvent();
  output.verbose(
    CALL_INFO,
    9,
    0,
    "%s:%s received zop message of type %s, ID=%" PRIu16 "\n",
    getName().c_str(),
    endPToStr( getEndpointType() ).c_str(),
    msgTToStr( ev->getType() ).c_str(),
    ev->getID()
  );

  // check to see if the test harness has been enabled
  // if so, immediately call the message handler
  if( enableTestHarness ) {
    ( *msgHandler )( ev );
    return true;
  }

  // check to see if this is a broadcast packet
  // if so, make sure the local device is a ZAP
  // if the local device is a ZAP, handle the broadcast
  // otherwise, ignore the packet
  if( ( ev->getType() == Forza::zopMsgT::Z_MSG ) && ( ev->getOpc() == Forza::zopOpc::Z_MSG_ZBAR ) ) {
    if( Type == Forza::zopCompID::Z_ZAP0 || Type == Forza::zopCompID::Z_ZAP1 || Type == Forza::zopCompID::Z_ZAP2 ||
        Type == Forza::zopCompID::Z_ZAP3 ) {
      return handleBarrier( ev );
    } else {
      // not a ZAP device, ignore the packet
      delete ev;
      return true;
    }
  }

  // if this is an RZA device, marshall it through to the ZIQ
  // if this is a ZEN/ZQM/ZIP, forward it in the incoming queue
  if( Type == Forza::zopCompID::Z_RZA || Type == Forza::zopCompID::Z_ZEN || Type == Forza::zopCompID::Z_ZQM ||
      Type == Forza::zopCompID::Z_PREC_ZIP ) {
    ( *msgHandler )( ev );
    return true;
  }

  // if this is a ZIP device (see above)

  // if this is a ZAP device and a thread migration or mzop (scratchpad req, methinks - tjd, 6-sept-24),
  // send it to the RevCPU handler
  if( ( Type == Forza::zopCompID::Z_ZAP0 || Type == Forza::zopCompID::Z_ZAP1 || Type == Forza::zopCompID::Z_ZAP2 ||
        Type == Forza::zopCompID::Z_ZAP3 ) &&
      ( ev->getType() == Forza::zopMsgT::Z_TMIG || ev->getType() == Forza::zopMsgT::Z_MZOP ) ) {
    ( *msgHandler )( ev );
    return true;
  }

  // this is likely a ZAP device,
  // iterate across the outstanding messages
  unsigned Cur = 0;
  for( auto const& [DestHart, ID, isRead, Target, Opc, Req] : outstanding ) {
    auto SrcHart = ev->getDestHart();
    auto EVID    = ev->getID();
    if( ( DestHart == SrcHart ) && ( ID == EVID ) ) {
      // found a match
      // if this is a read request, marshall to the RevCPU to handle the hazarding
      if( isRead ) {
        handleLoadResponse( ev, Target, Opc, Req, ID, ev->getSrcHart() );
      }

      // clear the request from the outstanding request list
      outstanding.erase( outstanding.begin() + Cur );

      // clear the message Id
      msgId[DestHart].clearMsgId( EVID );

      // we are clear to return
      delete ev;
      return true;
    }
    Cur++;
  }
#if 0
  // debug statements
  std::cout << "NO MATCHING REQUEST : src=" << (unsigned)(ev->getSrcHart())
            << "; ID= " << (unsigned)(ev->getID())
            << "; outstanding.size() = " << outstanding.size() << std::endl;
  for( auto const& [DestHart, ID, isRead, Target, Opc, Req] : outstanding ){
    std::cout << " ===== " << std::endl;
    std::cout << "SrcHart = " << (unsigned)(DestHart) << std::endl;
    std::cout << "ID = " << (unsigned)(ID) << std::endl;
    std::cout << "isRead = " << isRead << std::endl;
    std::cout << "Target = " << Target << std::endl;
    std::cout << "Opc = " << (unsigned)(Opc) << std::endl;
    std::cout << " ===== " << std::endl;
  }
#endif
  // we didn't find a matching request, return false
  return false;
}

bool zopNIC::handleBarrier( zopEvent* ev ) {
  uint64_t tmp{};
  if( !ev->getFLIT( Z_FLIT_SENSE, &tmp ) ) {
    output.fatal(
      CALL_INFO,
      -1,
      "%s, Error: zopEvent on zopNIC failed to read sense FLIT; "
      "OPC=%" PRIu8 ", LENGTH=%" PRIu8 ", ID=%" PRIu16 "\n",
      getName().c_str(),
      static_cast<uint8_t>( ev->getOpc() ),
      ev->getLength(),
      ev->getID()
    );
  }
  uint32_t sense = static_cast<uint32_t>( tmp );

  if( sense > 1 ) {
    output.fatal( CALL_INFO, -1, "%s, Error: received zone barrier with sense > 1; SENSE=%" PRIu32 "\n", getName().c_str(), sense );
  }

  for( size_t i = 0; i < numHarts; i++ ) {
    zoneBarrier[sense][i]++;
  }

  delete ev;
  return true;
}

bool zopNIC::handleFence( zopEvent* ev ) {
  // first, determine if this fence has already been encountered
  // if not, incrememnt the fence counter for this hart

  // if this fence has been encountered, then check the oustanding
  // operation vector to see if we have any outstanding requests
  // if no outstanding requests exist, then clear the fence

  // this function returns TRUE if the fence is ready to clear
  // otherwise, this function returns false

  auto ReqHart = ev->getSrcHart();

  if( ev->getFence() ) {
    // fence has been encountered, check to see if we need to clear
    for( auto const& [Hart, ID, isRead, Target, Opc, Req] : outstanding ) {
      if( Hart == ReqHart ) {
        // this is an outstanding request for the same Hart, do not clear it
        return false;
      }
    }

    // no outstanding requests for this hart, clear it
    HARTFence[ReqHart]--;
    output.verbose(
      CALL_INFO,
      9,
      0,
      "Clearing FENCE from %s @ [hart:zcid:pcid:type]=[%" PRIu16 ":%" PRIu8 ":%" PRIu8 ":%s]\n",
      getName().c_str(),
      ev->getSrcHart(),
      ev->getSrcZCID(),
      ev->getSrcPCID(),
      endPToStr( getEndpointType() ).c_str()
    );
    return true;
  } else {
    // fence has not been encountered, set it
    ev->setFence();
    HARTFence[ReqHart]++;
    recordStat( getStatFromPacket( ev ), 1 );
    output.verbose(
      CALL_INFO,
      9,
      0,
      "Issuing FENCE from %s @ [hart:zcid:pcid:type]=[%" PRIu16 ":%" PRIu8 ":%" PRIu8 ":%s]\n",
      getName().c_str(),
      ev->getSrcHart(),
      ev->getSrcZCID(),
      ev->getSrcPCID(),
      endPToStr( getEndpointType() ).c_str()
    );
    return false;
  }

  return false;  // not ready to clear
}

unsigned zopNIC::getNumDestinations() {
  return numDest;
}

SST::Interfaces::SimpleNetwork::nid_t zopNIC::getAddress() {
  return iFace->getEndpointID();
}

unsigned zopNIC::getNumZaps() {
  unsigned count = 0;
  for( auto i : hostMap ) {
    auto t = i.second;
    if( ( (unsigned) std::get<_HM_ZID>( t ) != Zone ) || ( std::get<_HM_PID>( t ) != Precinct ) )
      continue;
    zopCompID zc = std::get<_HM_ENDP_T>( t );
    if( ( zc == zopCompID::Z_ZAP0 ) || ( zc == zopCompID::Z_ZAP1 ) || ( zc == zopCompID::Z_ZAP2 ) || ( zc == zopCompID::Z_ZAP3 ) )
      count++;
  }
  return count;
}

bool zopNIC::clockTick( SST::Cycle_t cycle ) {
  unsigned thisCycle = 0;
  unsigned Hart      = 0;
  bool     erase     = false;

  // check if there are any outstanding requests
  if( sendQ.empty() ) {
    return false;
  }

  for( auto it = sendQ.begin(); it != sendQ.end(); ) {
    erase = false;
    if( thisCycle < ReqPerCycle ) {
      zopEvent* ev = static_cast<zopEvent*>( ( *it )->inspectPayload() );
      Hart         = (unsigned) ( ev->getSrcHart() );
      if( Type == SST::Forza::zopCompID::Z_RZA || Type == SST::Forza::zopCompID::Z_ZEN || Type == SST::Forza::zopCompID::Z_ZQM ) {
        // I am an RZA... I don't need to reserve any message IDs
        // ZEN ACKs and NACKs do not use message IDs, ZEN ZOPs to the RZA internally
        // handle message IDs.
        auto P = ev->getPacket();
        ev->encodeEvent();
        if( iFace->spaceToSend( 0, P.size() * 64 ) ) {
          // we have space to send
          recordStat( getStatFromPacket( ev ), 1 );
          recordStat( zopStats::BytesSent, P.size() * 64 );
          thisCycle++;
          iFace->send( ( *it ), 0 );
          it    = sendQ.erase( it );
          erase = true;
          output.verbose( CALL_INFO, 9, 0, "ZOPNET: Sending message to Type=%s\n", endPToStr( Type ).c_str() );
        }
      } else if( ev->getType() == SST::Forza::zopMsgT::Z_FENCE ) {
        // handle the fence operation
        if( handleFence( ev ) ) {
          // fence is ready to clear
          it    = sendQ.erase( it );
          erase = true;
        }
      } else if( ( msgId[Hart].getNumFree() > 0 ) && ( HARTFence[Hart] == 0 ) ) {
        /**
         * Underlying issue here is do we allow rev to managae the msgIds or do we let zopnet handle it
         * if we let zopnet handle it, we can probably free an ID for a hart somewhere along the way without
         * needing an ack (probably better for migrations and thread requests)
         */

        // we have a free message Id for this hart
        auto P = ev->getPacket();
        if( iFace->spaceToSend( 0, P.size() * 64 ) ) {
          // we have space to send
          // bypass this process if we're sending a zone barrier
          // zone barriers require no msg id and/or response
          // tdysart note (31-may-2024): This is where some messages are having their msg_id changed.
          // Use skip to not update the msg_id for responses from zap to zen (these are scratchpad acks)
          bool skip =
            ( ev->getType() == SST::Forza::zopMsgT::Z_RESP ) && ( ( ev->getDestZCID() == (uint8_t) SST::Forza::zopCompID::Z_ZEN ) ||
                                                                  ( ev->getDestZCID() == (uint8_t) SST::Forza::zopCompID::Z_ZQM ) );
          skip = ( skip && ( ev->getType() == SST::Forza::zopMsgT::Z_TMIG ) );  //also skip anything migrate related
          if( ( ev->getOpc() != SST::Forza::zopOpc::Z_MSG_ZBAR ) && ( !skip ) ) {
            ev->setID( msgId[Hart].getMsgId() );
            auto V = std::make_tuple( Hart, ev->getID(), ev->isRead(), ev->getTarget(), ev->getOpc(), ev->getMemReq() );
            outstanding.push_back( V );
          }
          ev->encodeEvent();
          recordStat( getStatFromPacket( ev ), 1 );
          recordStat( zopStats::BytesSent, P.size() * 64 );
          thisCycle++;
          iFace->send( ( *it ), 0 );
          it    = sendQ.erase( it );
          erase = true;
        }
      }
    } else {
      // saturated the number of outstanding requests
      return false;
    }
    if( !erase ) {
      it++;
    }
  }
  return false;
}
}  // namespace SST::Forza

// EOF
