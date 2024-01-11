//
// _zip_cc_
//

#include "zip_nic.h"

using namespace SST::Forza;

ZIPHFINIC::ZIPHFINIC(ComponentId_t id, Params& params)
  : nicAPI(id, params) {
  // setup the initial logging functions
  int verbosity = params.find<int>("verbose", 0);
  output = new SST::Output("ZIPHFINIC["+getName()+":@p:@t]: ", verbosity, 0, SST::Output::STDOUT);

  const std::string nicClock = params.find<std::string>("clock", "1GHz");
  registerClock(nicClock, new Clock::Handler<ZIPHFINIC>(this, &ZIPHFINIC::clockTick));

  // load the SimpleNetwork interfaces
  iFace = loadUserSubComponent<SST::Interfaces::SimpleNetwork>("iface", ComponentInfo::SHARE_NONE, 1);
  if( !iFace ){
    // load the anonymous nic
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

  iFace->setNotifyOnReceive(new SST::Interfaces::SimpleNetwork::Handler<ZIPHFINIC>(this, &ZIPHFINIC::msgNotify));

  initBroadcastSent = false;

  numDest = 0;

  msgHandler = nullptr;
}

ZIPHFINIC::~ZIPHFINIC(){
  delete output;
}

void ZIPHFINIC::setMsgHandler(Event::HandlerBase* handler){
  msgHandler = handler;
}

void ZIPHFINIC::init(unsigned int phase){
  iFace->init(phase);

  if( iFace->isNetworkInitialized() ){
    if( !initBroadcastSent) {
      initBroadcastSent = true;
      ZIPEvent *ev = new ZIPEvent();

      SST::Interfaces::SimpleNetwork::Request * req = new SST::Interfaces::SimpleNetwork::Request();
      req->dest = SST::Interfaces::SimpleNetwork::INIT_BROADCAST_ADDR;
      req->src = iFace->getEndpointID();
      req->givePayload(ev);
      iFace->sendUntimedData(req);
      output->verbose(CALL_INFO, 1, 0, "sent init message phase %d\n", phase);
    }
  }

  while( SST::Interfaces::SimpleNetwork::Request * req = iFace->recvUntimedData() ) {
    ZIPEvent *ev = static_cast<ZIPEvent*>(req->takePayload());
    numDest++;
    output->verbose(CALL_INFO, 1, 0, "received init message phase %d\n", phase);
  }
}

void ZIPHFINIC::setup(){
  if( msgHandler == nullptr ){
    output->fatal(CALL_INFO, -1,
                  "%s, Error: ZIPHFINIC implements a callback-based notification and parent has not registerd a callback function\n",
                  getName().c_str());
  }
}

bool ZIPHFINIC::msgNotify(int vn){
  output->verbose(CALL_INFO, 1, 0, "received message\n");
  SST::Interfaces::SimpleNetwork::Request* req = iFace->recv(0);
  if( req != nullptr ){
    if( req != nullptr ){
      ZIPEvent *ev = static_cast<ZIPEvent*>(req->takePayload());
      delete req;
      (*msgHandler)(ev);
    }
  }
  return true;
}

void ZIPHFINIC::send(ZIPEvent* event, int destination){
  SST::Interfaces::SimpleNetwork::Request *req = new SST::Interfaces::SimpleNetwork::Request();
  req->dest = destination;
  req->src = iFace->getEndpointID();
  req->givePayload(event);
  // check if event is ZIPAggEvent and therefore has payload
  ZIPAggEvent* aggEvent = dynamic_cast<ZIPAggEvent*>(event);
  if (aggEvent != nullptr) {
    req->size_in_bits = aggEvent->getPayload().size()*64;
  } else {
    req->size_in_bits = 0;
  }
  sendQ.push(req);
  output->verbose(CALL_INFO, 1, 0, "adding to sendQ\n");
}

int ZIPHFINIC::getNumDestinations(){
  return numDest;
}

SST::Interfaces::SimpleNetwork::nid_t ZIPHFINIC::getAddress(){
  return iFace->getEndpointID();
}

bool ZIPHFINIC::clockTick(Cycle_t cycle){
  while( !sendQ.empty() ){
    if(iFace->send(sendQ.front(), 0)) {
      output->verbose(CALL_INFO, 1, 0, "message sent\n");
      sendQ.pop();
    }else{
      break;
    }
  }

  return false;
}

// EOF
