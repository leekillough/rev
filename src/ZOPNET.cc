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
  : zopAPI(id, params), msgHandler(nullptr),
    initBroadcastSent(false), numDest(0), Type(zopEndP::Z_ZAP){

  // read the parameters
  int verbosity = params.find<int>("verbose", 0);
  output.init("zopNIC[" + getName() + ":@p:@t]: ",
              verbosity, 0, SST::Output::STDOUT);

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
    netparams.insert("in_buf_size", "256B");
    netparams.insert("out_buf_size", "256B");
    netparams.insert("link_bw", "40GiB/s");
    iFace = loadAnonymousSubComponent<SST::Interfaces::SimpleNetwork>("merlin.linkcontrol",
                                                                      "iface",
                                                                      0,
                                                                      ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS,
                                                                      netparams,
                                                                      1);
  }
}

zopNIC::~zopNIC(){
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
                   "%s received init broadcas messages from %d of type %s\n",
                   getName().c_str(), (uint32_t)(srcID),
                   endPToStr(hostMap[srcID]).c_str());
  }
}

void zopNIC::send(zopEvent *ev, uint32_t dest ){
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
  return false;
}

// EOF
