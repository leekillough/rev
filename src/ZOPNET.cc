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
using namespace RevCPU;

zopNIC::zopNIC(ComponentId_t id, Params& params)
  : zopAPI(id, params), iFace(nullptr), msgHandler(nullptr),
    initBroadcastSent(false), numDest(0), Type(zopEndP::Z_ZAP){

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
}

void zopNIC::registerStats(){
  for( auto* stat : {
    "BytesSent",
    "MZOPSent",
    "HZOPSent",
    "RZOPSent",
    "MSGSent",
    "ACKSent",
    "NACKSent",
    "SYSCSent",
    "EXCPSent",
  }){
    stats.push_back(registerStatistic<uint64_t>(stat));
  }
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
      SST::Interfaces::SimpleNetwork::Request * req = new SST::Interfaces::SimpleNetwork::Request();
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
    std::vector<uint32_t> Pkt = ev->getPacket();
    hostMap[srcID] = static_cast<zopEndP>(Pkt[0]);
    output.verbose(CALL_INFO, 7, 0,
                   "%s received init broadcast messages from %d of type %s\n",
                   getName().c_str(), (uint32_t)(srcID),
                   endPToStr(hostMap[srcID]).c_str());
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
  }
  // --- end print out the host mapping table
}

void zopNIC::send(zopEvent *ev, zopEndP dest ){
  SST::Interfaces::SimpleNetwork::Request *req =
    new SST::Interfaces::SimpleNetwork::Request();
  output.verbose(CALL_INFO, 9, 0,
                 "Sending message from %s @ id=%d to endpoint=%s\n",
                 getName().c_str(), (uint32_t)(getAddress()),
                 endPToStr(dest).c_str() );
  uint32_t realDest = 0;
  for(auto const& [key, val] : hostMap){
    if( val == dest ){
      realDest = key;
    }
  }
  auto Packet = ev->getPacket();
  Packet[1] = realDest;
  Packet[2] = (uint32_t)(getAddress());
  ev->setPacket(Packet);
  req->dest = realDest;
  req->src = getAddress();
  req->givePayload(ev);
  sendQ.push(req);
}

void zopNIC::send(zopEvent *ev, uint32_t dest ){
  SST::Interfaces::SimpleNetwork::Request *req =
    new SST::Interfaces::SimpleNetwork::Request();
  output.verbose(CALL_INFO, 9, 0,
                 "Sending message from %s @ id=%d to endpoint=%d\n",
                 getName().c_str(), (uint32_t)(getAddress()),
                 dest);
  auto Packet = ev->getPacket();
  Packet[1] = dest;
  Packet[2] = (uint32_t)(getAddress());
  ev->setPacket(Packet);
  req->dest = dest;
  req->src = getAddress();
  req->givePayload(ev);
  sendQ.push(req);
}


bool zopNIC::msgNotify(int vn){
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv(0);
  if( req != nullptr ){
    zopEvent *ev = static_cast<zopEvent*>(req->takePayload());
    if( !ev ){
      output.fatal(CALL_INFO, -1, "%s, Error: zopEvent on zopNIC is null\n",
                   getName().c_str());
    }
    output.verbose(CALL_INFO, 9, 0,
                   "%s received zop message\n",
                   getName().c_str());
    (*msgHandler)(ev);
    delete req;
  }
  return true;
}

unsigned zopNIC::getNumDestinations(){
  return numDest;
}

SST::Interfaces::SimpleNetwork::nid_t zopNIC::getAddress(){
  return iFace->getEndpointID();
}

bool zopNIC::clockTick(SST::Cycle_t cycle){
  unsigned thisCycle = 0;
  while( (!sendQ.empty()) && (thisCycle < ReqPerCycle) ){
    SST::Interfaces::SimpleNetwork::Request *R = sendQ.front();
    zopEvent *ev = static_cast<zopEvent*>(R->takePayload());
    auto P = ev->getPacket();
    if( iFace->spaceToSend(0, P.size()*32) &&
        iFace->send(sendQ.front(), 0) ){
      sendQ.pop();
      thisCycle++;
    }else{
      break;
    }
  }

  return false;
}

// EOF
