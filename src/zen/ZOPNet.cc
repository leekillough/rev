//
// _ZOPNET_cc_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "ZOPNet.h"

using namespace SST;
using namespace Forza;

zenZopNIC::zenZopNIC(ComponentId_t id, Params& params)
  : zenZopAPI(id, params), iFace(nullptr), msgHandler(nullptr),
    initBroadcastSent(false), numDest(0), numHarts(0),
    Precinct(0), Zone(0),
    Type(zopCompID::Z_ZAP0), msgId(nullptr), HARTFence(nullptr){

  // read the parameters
  int verbosity = params.find<int>("verbose", 0);
  output.init("zenZopNIC[" + getName() + ":@p:@t]: ",
              verbosity, 0, SST::Output::STDOUT);
  ReqPerCycle = params.find<unsigned>("req_per_cycle", 1);
  isPrec = params.find<bool>("is_prec", false);

  // register the stats
  registerStats();

  // register the clock handler
  const std::string cpuFreq = params.find<std::string>("clock", "1GHz");
  registerClock(cpuFreq, new Clock::Handler<zenZopNIC>(this, &zenZopNIC::clockTick));
  output.output("zenZopNIC[%s] Registering clock with frequency=%s\n",
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
    new SST::Interfaces::SimpleNetwork::Handler<zenZopNIC>(this, &zenZopNIC::msgNotify));
}

zenZopNIC::~zenZopNIC(){
  if( msgId )
    delete[] msgId;
}


void zenZopNIC::setNumHarts(unsigned H){
  numHarts = H;
  msgId = new SST::Forza::zopMsgID [numHarts];
  HARTFence = new unsigned [numHarts];
  for( unsigned i=0; i<numHarts; i++ ){
    HARTFence[i] = 0;
  }
}


void zenZopNIC::registerStats(){
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
    "EXCPSent",
  }){
    stats.push_back(registerStatistic<uint64_t>(stat));
  }
}

void zenZopNIC::recordStat(zenZopNIC::zopStats Stat, uint64_t Data){
  if( Stat > zenZopNIC::zopStats::EXCPSent ){
    return ;
  }

  stats[Stat]->addData(Data);
}

zenZopNIC::zopStats zenZopNIC::getStatFromPacket(zenZopEvent *ev){
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

void zenZopNIC::setMsgHandler(Event::HandlerBase* handler){
  msgHandler = handler;
}

void zenZopNIC::setup(){
  if( msgHandler == nullptr ){
    output.fatal(CALL_INFO, -1,
                 "%s, Error: zenZopNIC implements a callback based notification and the parent has not registered a callback function\n",
                 getName().c_str());
  }
}

void zenZopNIC::init(unsigned int phase){

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
      zenZopEvent *ev;
      if (!isPrec) {
        ev = new zenZopEvent(iFace->getEndpointID(),
                                    getEndpointType());
      } else {
        ev = new zenZopEvent(iFace->getEndpointID(),
                                    getEndpointTypePrec());
      }
      SST::Interfaces::SimpleNetwork::Request * req =
        new SST::Interfaces::SimpleNetwork::Request();
      req->dest = SST::Interfaces::SimpleNetwork::INIT_BROADCAST_ADDR;
      req->src = iFace->getEndpointID();
      req->givePayload(ev);
      iFace->sendUntimedData(req);

      // add myself to the local endpoint table
      if (!isPrec) {
        hostMap[iFace->getEndpointID()] = getEndpointType();
      } else {
        hostMapPrec[iFace->getEndpointID()] = getEndpointTypePrec();
      }
    }
  }

  // receive all the broadcast messages
  while( SST::Interfaces::SimpleNetwork::Request * req =
         iFace->recvUntimedData() ) {
    zenZopEvent *ev = static_cast<zenZopEvent*>(req->takePayload());
    numDest++;
    SST::Interfaces::SimpleNetwork::nid_t srcID = req->src;
    std::vector<uint64_t> Pkt = ev->getPacket();
    if (!isPrec) {
      hostMap[srcID] = static_cast<zopCompID>(Pkt[0] & Z_MASK_TYPE);
      output.verbose(CALL_INFO, 7, 0,
                    "%s received init broadcast messages from %d of type %s\n",
                    getName().c_str(), (uint32_t)(srcID),
                    endPToStr(hostMap[srcID]).c_str());
    } else {
      hostMapPrec[srcID] = static_cast<zopPrecID>(Pkt[0] & Z_MASK_TYPE);
      output.verbose(CALL_INFO, 7, 0,
                    "%s received init broadcast messages from %d of type %s\n",
                    getName().c_str(), (uint32_t)(srcID),
                    precIDToStr(hostMapPrec[srcID]).c_str());

    }
  }

  // --- begin print out the host mapping table
  if( phase == 4 ){
    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");
    output.verbose(CALL_INFO, 9, 0,
                  "               ZOP NETWORK MAPPING\n");
    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");

    if (!isPrec) {
      for(auto const& [key, val] : hostMap){
          output.verbose(CALL_INFO, 9, 0,
                        "Endpoint ID=%d is of Type=%s\n",
                        (uint32_t)(key), endPToStr(val).c_str());
      }
    } else {
      for(auto const& [key, val] : hostMapPrec){
          output.verbose(CALL_INFO, 9, 0,
                        "Endpoint ID=%d is of Type=%s\n",
                        (uint32_t)(key), precIDToStr(val).c_str());
      }
    }

    output.verbose(CALL_INFO, 9, 0,
                  "------------------------------------------------------\n");
  }
  // --- end print out the host mapping table
}

void zenZopNIC::send(zenZopEvent *ev, zopPrecID dest){
  SST::Interfaces::SimpleNetwork::Request *req =
    new SST::Interfaces::SimpleNetwork::Request();
  output.verbose(CALL_INFO, 9, 0,
                 "Sending message from %s @ id=%d to endpoint[hart:zone:prec:Type]=[%d:%d:%d:%s], flit 1 %lu\n",
                 getName().c_str(), (uint32_t)(getAddress()),
                 ev->getDestHart(), ev->getDestZCID(), ev->getDestPCID(),
                 precIDToStr(dest).c_str(),
                 ev->getPacket()[1] );
  auto realDest = 0;
  for( auto i : hostMapPrec ){
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

void zenZopNIC::send(zenZopEvent *ev, zopCompID dest){
  SST::Interfaces::SimpleNetwork::Request *req =
    new SST::Interfaces::SimpleNetwork::Request();
  output.verbose(CALL_INFO, 9, 0,
                 "Sending message from %s @ id=%d to endpoint[hart:zone:prec:Type]=[%d:%d:%d:%s], flit 1 %lu\n",
                 getName().c_str(), (uint32_t)(getAddress()),
                 ev->getDestHart(), ev->getDestZCID(), ev->getDestPCID(),
                 endPToStr(dest).c_str(),
                 ev->getPacket()[1] );
  auto realDest = 0;
  if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDP) {
    dest = zopCompID::Z_ZEN;
    output.verbose(CALL_INFO, 9, 0,
                  "Hijacked msg, sending message from %s @ id=%d to endpoint[hart:zone:prec:Type]=[%d:%d:%d:%s], flit 1 %lu\n",
                  getName().c_str(), (uint32_t)(getAddress()),
                  ev->getDestHart(), ev->getDestZCID(), ev->getDestPCID(),
                  endPToStr(dest).c_str(),
                  ev->getPacket()[1] );
  }
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

bool zenZopNIC::msgNotify(int vn){
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv(0);
  output.verbose(CALL_INFO, 4, 0, "recv msg on %s\n", getName().c_str());
  if( req == nullptr ){
    return false;
  }

  zenZopEvent *ev = static_cast<zenZopEvent*>(req->takePayload());
  if( ev == nullptr ){
    output.fatal(CALL_INFO, -1, "%s, Error: zenZopEvent on zenZopNIC is null\n",
                 getName().c_str());
  }

  auto P = ev->getPacket();

  // decode the event
  ev->decodeEvent();
  output.verbose(CALL_INFO, 9, 0,
                 "%s:%s received zop message of type %s with opcode %" PRIu8 "\n",
                 getName().c_str(),
                 endPToStr(getEndpointType()).c_str(),
                 msgTToStr(ev->getType()).c_str(), (uint8_t)ev->getOpcode());

  // if this is an RZA device, marshall it through to the ZIQ
  if( Type == Forza::zopCompID::Z_RZA || Type == Forza::zopCompID::Z_ZEN
        || ev->getOpcode() == zopOpc::Z_MZOP_SCSD || ev->getOpcode() == zopOpc::Z_MSG_ACK ){
    (*msgHandler)(ev);
    return true;
  }

  // this is likely a ZAP device,
  // iterate across the outstanding messages
  unsigned Cur = 0;
  for( auto const& [DestHart, ID, isRead, Target, Req] : outstanding ){
    auto SrcHart = ev->getSrcHart();
    auto EVID = ev->getID();
    output.verbose(CALL_INFO, 5,0,
                       "SrcHart, EVID: %lu %lu\n", (long)SrcHart, (long)EVID);
    output.verbose(CALL_INFO, 5,0,
                       "DestHart, ID: %lu %lu\n", (long)DestHart, (long)ID);
    if( (DestHart == SrcHart) && (ID == EVID) ){
      // found a match
      // if this is a read request, marshall to the RevCPU to handle the hazarding
      if( isRead ){
        // TODO: do we need to correctly handle this?
        if( !ev->getFLIT(Z_FLIT_DATA_RESP, Target) ){
          output.verbose(CALL_INFO, 9, 0,
                       "%s, Error: zenZopEvent on zenZopNIC failed to read response FLIT; OPC=%d, LENGTH=%d, ID=%d\n",
                       getName().c_str(), (unsigned)(ev->getOpcode()),
                       (unsigned)(ev->getLength()), ID );
    (*msgHandler)(ev);
        } else {
            std::cout << "LOAD RESPONSE : 0x" << std::hex << *Target << std::dec << std::endl;
          std::cout << "LOAD RESPONSE : 0x" << std::hex << Target[0] << std::dec << std::endl;
          ev->setMemReq(Req);
          ev->setTarget(Target);
          (*msgHandler)(ev);
    }
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

  // we didn't find a matching request, return false
  return false;
}

bool zenZopNIC::handleFence(zenZopEvent *ev){
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

unsigned zenZopNIC::getNumDestinations(){
  return numDest;
}

SST::Interfaces::SimpleNetwork::nid_t zenZopNIC::getAddress(){
  return iFace->getEndpointID();
}


bool zenZopNIC::clockTick(SST::Cycle_t cycle){
  unsigned thisCycle = 0;
  unsigned Hart = 0;
  unsigned Cur = 0;

  // check if there are any outstanding requests
  if( sendQ.empty() ){
    return false;
  }

  for( auto R : sendQ ){
    if ( Type == SST::Forza::zopCompID::Z_RZA) {
        zenZopEvent *ev = static_cast<zenZopEvent*>(R->inspectPayload());
        output.verbose(CALL_INFO, 4, 0,
                   "Issuing msg from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str() );
    }
    if( thisCycle < ReqPerCycle ){
      zenZopEvent *ev = static_cast<zenZopEvent*>(R->inspectPayload());
      Hart = (unsigned)(ev->getSrcHart());
      if( Type == SST::Forza::zopCompID::Z_RZA || Type == SST::Forza::zopCompID::Z_ZEN || ev->getType() == SST::Forza::zopMsgT::Z_MSG ){
        // I am an RZA... I don't need to reserve any message IDs
        auto P = ev->getPacket();
        ev->encodeEvent();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          recordStat( getStatFromPacket(ev), 1 );
          thisCycle++;
          iFace->send(R, 0);
          sendQ.erase(sendQ.begin() + Cur);
    output.verbose(CALL_INFO, 4, 0,
                   "Issuing msg from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s] %d, type: %s, opcode: %" PRIu8 "\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str(), ev->getID(),
                   msgTToStr(ev->getType()), ev->getOpcode());
        }
      }else if( ev->getType() == SST::Forza::zopMsgT::Z_FENCE ){
        // handle the fence operation
        if( handleFence(ev) ){
          // fence is ready to clear
          sendQ.erase(sendQ.begin() + Cur);
        }
      }else if( (msgId[Hart].getNumFree() > 0) &&
                (HARTFence[Hart] == 0) ){
    output.verbose(CALL_INFO, 4, 0,
                   "Issuing msg from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str() );
        // we have a free message Id for this hart
        auto P = ev->getPacket();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          ev->setID( msgId[Hart].getMsgId() );
    output.verbose(CALL_INFO, 4, 0,
                   "Issuing msg from %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s] %d\n",
                   getName().c_str(),
                   ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                   endPToStr(getEndpointType()).c_str(), ev->getID() );
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
