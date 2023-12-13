//
// _ZOPNET_cc_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "../include/ZOPNET.h"

using namespace SST;
using namespace Forza;

zopNIC::zopNIC(ComponentId_t id, Params& params)
  : zopAPI(id, params), iFace(nullptr), msgHandler(nullptr),
    initBroadcastSent(false), numDest(0), numHarts(0),
    Precinct(0), Zone(0),
    Type(zopCompID::Z_ZAP0), msgId(nullptr), HARTFence(nullptr){

  // read the parameters
  int verbosity = params.find<int>("verbose", 0);
  output.init("zopNIC[" + getName() + ":@p:@t]: ",
              verbosity, 0, SST::Output::STDOUT);
  ReqPerCycle = params.find<unsigned>("req_per_cycle", 1);

  // register the stats
  registerStats();

  // register the clock handler
  const std::string cpuFreq = params.find<std::string>("clock", "1GHz");
  registerClock(cpuFreq, new Clock::Handler<zopNIC>(this, &zopNIC::clockTick));
  output.output("zopNIC[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  // load the SimpleNetwork interfaces
  iFace = loadUserSubComponent<SST::Interfaces::SimpleNetwork>("iface",
                                                               ComponentInfo::SHARE_NONE,
                                                               1);
  if( !iFace ){
    // load the anonymous NIC
    Params netparams;
    netparams.insert("port_name", params.find<std::string>("port", "network"));
    netparams.insert("in_buf_size", "2048B");
    netparams.insert("out_buf_size", "2048B");
    netparams.insert("link_bw", "100GiB/s");
    iFace = loadAnonymousSubComponent<SST::Interfaces::SimpleNetwork>("merlin.linkcontrol",
                                                                      "iface",
                                                                      0,
                                                                      ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS,
                                                                      netparams,
                                                                      1);
  }

  // setup the notification function
  iFace->setNotifyOnReceive(
    new SST::Interfaces::SimpleNetwork::Handler<zopNIC>(this, &zopNIC::msgNotify));
}

zopNIC::~zopNIC(){
  if( msgId )
    delete[] msgId;
  if( HARTFence )
    delete[] HARTFence;
}

void zopNIC::setNumHarts(unsigned H){
  numHarts = H;
  msgId = new SST::Forza::zopMsgID [numHarts];
  HARTFence = new unsigned [numHarts];
  for( unsigned i=0; i<numHarts; i++ ){
    HARTFence[i] = 0;
  }
}

void zopNIC::registerStats(){
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
  }){
    stats.push_back(registerStatistic<uint64_t>(stat));
  }
}

void zopNIC::recordStat(zopNIC::zopStats Stat, uint64_t Data){
  if( Stat > zopNIC::zopStats::EXCPSent ){
    return ;
  }

  stats[Stat]->addData(Data);
}

zopNIC::zopStats zopNIC::getStatFromPacket(zopEvent *ev){
  auto Packet = ev->getPacket();
  if( Packet.size() == 0 ){
    output.fatal(CALL_INFO, -1,
                 "Error: recording stat for a null packet\n" );
  }

  zopMsgT Type = ev->getType();
  switch( (zopMsgT)(Type) ){
  case zopMsgT::Z_MZOP:
    return zopStats::MZOPSent;
    break;
  case zopMsgT::Z_HZOPAC:
    return zopStats::HZOPACSent;
    break;
  case zopMsgT::Z_HZOPV:
    return zopStats::HZOPVSent;
    break;
  case zopMsgT::Z_RZOP:
    return zopStats::RZOPSent;
    break;
  case zopMsgT::Z_MSG:
    return zopStats::MSGSent;
    break;
  case zopMsgT::Z_TMIG:
    return zopStats::TMIGSent;
    break;
  case zopMsgT::Z_TMGT:
    return zopStats::TMGTSent;
    break;
  case zopMsgT::Z_SYSC:
    return zopStats::SYSCSent;
    break;
  case zopMsgT::Z_RESP:
    return zopStats::RESPSent;
    break;
  case zopMsgT::Z_FENCE:
    return zopStats::FENCESent;
    break;
  case zopMsgT::Z_EXCP:
    return zopStats::EXCPSent;
    break;
  default :
    output.fatal(CALL_INFO, -1,
                 "Error: unknown packet type=%d\n", (unsigned)(Type) );
    break;
  }

  // we should never reach this point
  return zopStats::MZOPSent;
}

void zopNIC::setMsgHandler(Event::HandlerBase* handler){
  msgHandler = handler;
}

void zopNIC::setup(){
  if( msgHandler == nullptr ){
    output.fatal(CALL_INFO, -1,
                 "%s, Error: zopNIC implements a callback based notification and the parent has not registered a callback function\n",
                 getName().c_str());
  }
}

void zopNIC::init(unsigned int phase){

  output.verbose(CALL_INFO, 7, 0,
                 "%s initializing interface at phase %d\n",
                 getName().c_str(), phase );

  // pass the init on to the actual interface
  if( !iFace ){
    output.fatal(CALL_INFO, -1,
                 "%s, Error : network interface is null\n",
                 getName().c_str());
  }
  iFace->init(phase);

  // determine if we need to send the discovery broadcast message
  if( iFace->isNetworkInitialized() ){
    if( !initBroadcastSent ){
      initBroadcastSent = true;
      zopEvent *ev = new zopEvent(iFace->getEndpointID(),
                                  getEndpointType());
      SST::Interfaces::SimpleNetwork::Request * req =
        new SST::Interfaces::SimpleNetwork::Request();
      req->dest = SST::Interfaces::SimpleNetwork::INIT_BROADCAST_ADDR;
      req->src = iFace->getEndpointID();
      req->givePayload(ev);
      iFace->sendUntimedData(req);

      // add myself to the local endpoint table
      hostMap[iFace->getEndpointID()] = getEndpointType();
    }
  }

  // receive all the broadcast messages
  while( SST::Interfaces::SimpleNetwork::Request * req =
         iFace->recvUntimedData() ) {
    zopEvent *ev = static_cast<zopEvent*>(req->takePayload());
    numDest++;
    SST::Interfaces::SimpleNetwork::nid_t srcID = req->src;
    std::vector<uint64_t> Pkt = ev->getPacket();
    hostMap[srcID] = static_cast<zopCompID>(Pkt[0] & Z_MASK_TYPE);
    output.verbose(CALL_INFO, 7, 0,
                   "%s received init broadcast messages from %d of type %s\n",
                   getName().c_str(), (uint32_t)(srcID),
                   endPToStr(hostMap[srcID]).c_str());
    delete ev;
  }

  // --- begin print out the host mapping table
  if( phase == 4 ){
    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");
    output.verbose(CALL_INFO, 9, 0,
                  "               ZOP NETWORK MAPPING\n");
    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");

    for(auto const& [key, val] : hostMap){
      output.verbose(CALL_INFO, 9, 0,
                    "Endpoint ID=%d is of Type=%s\n",
                    (uint32_t)(key), endPToStr(val).c_str());
    }

    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");
    // we first walk the preInitQ to see if there are any pending requests
    // these requests are queued before the network has been fully initialized
    if( preInitQ.size() > 0 ){
      output.verbose(CALL_INFO, 9, 0,
                    "%s processing pre-init messages\n",
                    getName().c_str());
      for( auto &e : preInitQ ){
        send(e.first, e.second);
      }
      preInitQ.clear();
    }
  }
  // --- end print out the host mapping table
}

void zopNIC::send(zopEvent *ev, zopCompID dest){
  if( !initBroadcastSent ){
    output.verbose(CALL_INFO, 9, 0,
                   "Buffering message from %s to endpoint [hart:zcid:pcid:type]=[%d:%d:%d:%s] before network boots\n",
                   getName().c_str(),
                   ev->getDestHart(), ev->getDestZCID(), ev->getDestPCID(),
                   endPToStr(dest).c_str() );
    preInitQ.push_back(std::make_pair(ev, dest));
    return ;
  }
  SST::Interfaces::SimpleNetwork::Request *req =
    new SST::Interfaces::SimpleNetwork::Request();
  output.verbose(CALL_INFO, 9, 0,
                 "Sending message from %s @ phys_id=%d to endpoint[hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                 getName().c_str(), (uint32_t)(getAddress()),
                 ev->getDestHart(), ev->getDestZCID(), ev->getDestPCID(),
                 endPToStr(dest).c_str() );
  auto realDest = 0;
  for( auto i : hostMap ){
    if( i.second == dest ){
      realDest = i.first;
    }
  }
  ev->encodeEvent();
  req->dest = realDest;   // FIXME
  req->src = getAddress();
  req->givePayload(ev);
  sendQ.push_back(req);
}

bool zopNIC::msgNotify(int vn){
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv(0);
  if( req == nullptr ){
    return false;
  }

  zopEvent *ev = static_cast<zopEvent*>(req->takePayload());
  if( ev == nullptr ){
    output.fatal(CALL_INFO, -1, "%s, Error: zopEvent on zopNIC is null\n",
                 getName().c_str());
  }

  auto P = ev->getPacket();

  // decode the event
  ev->decodeEvent();
#if 0
  output.verbose(CALL_INFO, 9, 0,
                 "%s:%s received zop message of type %s\n",
                 getName().c_str(),
                 endPToStr(getEndpointType()).c_str(),
                 msgTToStr(ev->getType()).c_str());
#endif

  // if this is an RZA device, marshall it through to the ZIQ
  if( Type == Forza::zopCompID::Z_RZA ){
    (*msgHandler)(ev);
    return true;
  }

  // this is likely a ZAP device,
  // iterate across the outstanding messages
  unsigned Cur = 0;
  for( auto const& [DestHart, ID, isRead, Target, Req] : outstanding ){
    auto SrcHart = ev->getSrcHart();
    auto EVID = ev->getID();
    if( (DestHart == SrcHart) && (ID == EVID) ){
      // found a match
      // if this is a read request, marshall to the RevCPU to handle the hazarding
      if( isRead ){
        if( !ev->getFLIT(Z_FLIT_DATA_RESP, Target) ){
          output.fatal(CALL_INFO, -1,
                       "%s, Error: zopEvent on zopNIC failed to read response FLIT; OPC=%d, LENGTH=%d, ID=%d\n",
                       getName().c_str(), (unsigned)(ev->getOpc()),
                       (unsigned)(ev->getLength()), ID );
        }
        ev->setMemReq(Req);
        ev->setTarget(Target);
        (*msgHandler)(ev);
      }

      // clear the request from the outstanding request list
      outstanding.erase(outstanding.begin() + Cur);

      // clear the message Id
      msgId[DestHart].clearMsgId(EVID);

      // we are clear to return
      delete ev;
      return true;
    }
    Cur++;
  }

  return true;
}

bool zopNIC::handleFence(zopEvent *ev){
  // first, determine if this fence has already been encountered
  // if not, incrememnt the fence counter for this hart

  // if this fence has been encountered, then check the oustanding
  // operation vector to see if we have any outstanding requests
  // if no outstanding requests exist, then clear the fence

  // this function returns TRUE if the fence is ready to clear
  // otherwise, this function returns false

  unsigned ReqHart = (unsigned)(ev->getSrcHart());

  if( ev->getFence() ){
    // fence has been encountered, check to see if we need to clear
    for( auto const& [Hart, ID, isRead, Target, Req] : outstanding ){
      if( (unsigned)(Hart) == ReqHart ){
        // this is an outstanding request for the same Hart, do not clear it
        return false;
      }
    }

    // no outstanding requests for this hart, clear it
    HARTFence[ReqHart]--;
    output.verbose(CALL_INFO, 9, 0,
                   "Clearing FENCE from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str() );
    return true;
  }else{
    // fence has not been encountered, set it
    ev->setFence();
    HARTFence[ReqHart]++;
    output.verbose(CALL_INFO, 9, 0,
                   "Issuing FENCE from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str() );
    return false;
  }

  return false;   // not ready to clear
}

unsigned zopNIC::getNumDestinations(){
  return numDest;
}

SST::Interfaces::SimpleNetwork::nid_t zopNIC::getAddress(){
  return iFace->getEndpointID();
}

bool zopNIC::clockTick(SST::Cycle_t cycle){
  unsigned thisCycle = 0;
  unsigned Hart = 0;
  unsigned Cur = 0;

  // check if there are any outstanding requests
  if( sendQ.empty() ){
    return false;
  }

  for( auto R : sendQ ){
    if( thisCycle < ReqPerCycle ){
      zopEvent *ev = static_cast<zopEvent*>(R->inspectPayload());
      Hart = (unsigned)(ev->getSrcHart());
      if( Type == SST::Forza::zopCompID::Z_RZA ){
        // I am an RZA... I don't need to reserve any message IDs
        auto P = ev->getPacket();
        ev->encodeEvent();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          recordStat( getStatFromPacket(ev), 1 );
          thisCycle++;
          iFace->send(R, 0);
          sendQ.erase(sendQ.begin() + Cur);
        }
      }else if( ev->getType() == SST::Forza::zopMsgT::Z_FENCE ){
        // handle the fence operation
        if( handleFence(ev) ){
          // fence is ready to clear
          sendQ.erase(sendQ.begin() + Cur);
        }
      }else if( (msgId[Hart].getNumFree() > 0) &&
                (HARTFence[Hart] == 0) ){
        // we have a free message Id for this hart
        auto P = ev->getPacket();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          ev->setID( msgId[Hart].getMsgId() );
          auto V = std::make_tuple(Hart, ev->getID(), ev->isRead(),
                                   ev->getTarget(), ev->getMemReq());
          outstanding.push_back(V);
          ev->encodeEvent();
          recordStat( getStatFromPacket(ev), 1 );
          thisCycle++;
          iFace->send(R, 0);
          sendQ.erase(sendQ.begin() + Cur);
        }
      }
    }else{
      // saturated the number of outstanding requests
      return false;
    }
    Cur++;
  }
  return false;
}

// EOF
