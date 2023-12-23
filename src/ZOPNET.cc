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

void zopNIC::handleLoadResponse(zopEvent *ev,
                                uint64_t *Target,
                                SST::Forza::zopOpc Opc,
                                SST::RevCPU::MemReq Req,
                                uint8_t ID,
                                uint16_t SrcHart){
  // retrieve the data from the response payload
  uint64_t tmp = 0x00ull;
  if( !ev->getFLIT(Z_FLIT_DATA_RESP, &tmp) ){
    output.fatal(CALL_INFO, -1,
                 "%s, Error: zopEvent on zopNIC failed to read response FLIT; OPC=%d, LENGTH=%d, ID=%d\n",
                 getName().c_str(), (unsigned)(ev->getOpc()),
                 (unsigned)(ev->getLength()), ID );
  }

  // write the data depending upon what the sending opcode was
  if( SrcHart == Z_MZOP_PIPE_HART ){
    // this request is a response from an MZOP RZA pipe
    switch( Opc ){
    case SST::Forza::zopOpc::Z_MZOP_LB:
      *(reinterpret_cast<uint8_t *>(*Target)) = static_cast<uint8_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_MZOP_LH:
      *(reinterpret_cast<uint16_t *>(Target)) = static_cast<uint16_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_MZOP_LW:
      *(reinterpret_cast<uint32_t *>(Target)) = static_cast<uint32_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_MZOP_LD:
      *Target = tmp;
      break;
    case SST::Forza::zopOpc::Z_MZOP_LSB:
      *(reinterpret_cast<int8_t *>(Target)) = static_cast<int8_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_MZOP_LSH:
      *(reinterpret_cast<int16_t *>(Target)) = static_cast<int16_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_MZOP_LSW:
      *(reinterpret_cast<int32_t *>(Target)) = static_cast<int32_t>(tmp);
      break;
    default:
      output.fatal(CALL_INFO, -1, "%s, Error: zopEvent on zopNIC MZOP load response could not be handled; OPC=%d\n",
                  getName().c_str(), (unsigned)(Opc));
      break;
    }
  }else if( SrcHart == Z_HZOP_PIPE_HART ){
    // this request is a response from an HZOP RZA pipe
    switch( Opc ){
    case SST::Forza::zopOpc::Z_HAC_32_BASE_ADD:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_AND:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_OR:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_XOR:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_SMAX:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_MAX:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_SMIN:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_MIN:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_SWAP:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_CAS:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_FADD:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_FSUB:
    case SST::Forza::zopOpc::Z_HAC_32_BASE_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_32_M_ADD:
    case SST::Forza::zopOpc::Z_HAC_32_M_AND:
    case SST::Forza::zopOpc::Z_HAC_32_M_OR:
    case SST::Forza::zopOpc::Z_HAC_32_M_XOR:
    case SST::Forza::zopOpc::Z_HAC_32_M_SMAX:
    case SST::Forza::zopOpc::Z_HAC_32_M_MAX:
    case SST::Forza::zopOpc::Z_HAC_32_M_SMIN:
    case SST::Forza::zopOpc::Z_HAC_32_M_MIN:
    case SST::Forza::zopOpc::Z_HAC_32_M_SWAP:
    case SST::Forza::zopOpc::Z_HAC_32_M_CAS:
    case SST::Forza::zopOpc::Z_HAC_32_M_FADD:
    case SST::Forza::zopOpc::Z_HAC_32_M_FSUB:
    case SST::Forza::zopOpc::Z_HAC_32_M_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_32_S_ADD:
    case SST::Forza::zopOpc::Z_HAC_32_S_AND:
    case SST::Forza::zopOpc::Z_HAC_32_S_OR:
    case SST::Forza::zopOpc::Z_HAC_32_S_XOR:
    case SST::Forza::zopOpc::Z_HAC_32_S_SMAX:
    case SST::Forza::zopOpc::Z_HAC_32_S_MAX:
    case SST::Forza::zopOpc::Z_HAC_32_S_SMIN:
    case SST::Forza::zopOpc::Z_HAC_32_S_MIN:
    case SST::Forza::zopOpc::Z_HAC_32_S_SWAP:
    case SST::Forza::zopOpc::Z_HAC_32_S_CAS:
    case SST::Forza::zopOpc::Z_HAC_32_S_FADD:
    case SST::Forza::zopOpc::Z_HAC_32_S_FSUB:
    case SST::Forza::zopOpc::Z_HAC_32_S_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_32_MS_ADD:
    case SST::Forza::zopOpc::Z_HAC_32_MS_AND:
    case SST::Forza::zopOpc::Z_HAC_32_MS_OR:
    case SST::Forza::zopOpc::Z_HAC_32_MS_XOR:
    case SST::Forza::zopOpc::Z_HAC_32_MS_SMAX:
    case SST::Forza::zopOpc::Z_HAC_32_MS_MAX:
    case SST::Forza::zopOpc::Z_HAC_32_MS_SMIN:
    case SST::Forza::zopOpc::Z_HAC_32_MS_MIN:
    case SST::Forza::zopOpc::Z_HAC_32_MS_SWAP:
    case SST::Forza::zopOpc::Z_HAC_32_MS_CAS:
    case SST::Forza::zopOpc::Z_HAC_32_MS_FADD:
    case SST::Forza::zopOpc::Z_HAC_32_MS_FSUB:
    case SST::Forza::zopOpc::Z_HAC_32_MS_FRSUB:
      *(reinterpret_cast<uint32_t *>(Target)) = static_cast<uint32_t>(tmp);
      break;
    case SST::Forza::zopOpc::Z_HAC_64_BASE_ADD:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_AND:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_OR:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_XOR:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_SMAX:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_MAX:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_SMIN:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_MIN:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_SWAP:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_CAS:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_FADD:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_FSUB:
    case SST::Forza::zopOpc::Z_HAC_64_BASE_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_64_M_ADD:
    case SST::Forza::zopOpc::Z_HAC_64_M_AND:
    case SST::Forza::zopOpc::Z_HAC_64_M_OR:
    case SST::Forza::zopOpc::Z_HAC_64_M_XOR:
    case SST::Forza::zopOpc::Z_HAC_64_M_SMAX:
    case SST::Forza::zopOpc::Z_HAC_64_M_MAX:
    case SST::Forza::zopOpc::Z_HAC_64_M_SMIN:
    case SST::Forza::zopOpc::Z_HAC_64_M_MIN:
    case SST::Forza::zopOpc::Z_HAC_64_M_SWAP:
    case SST::Forza::zopOpc::Z_HAC_64_M_CAS:
    case SST::Forza::zopOpc::Z_HAC_64_M_FADD:
    case SST::Forza::zopOpc::Z_HAC_64_M_FSUB:
    case SST::Forza::zopOpc::Z_HAC_64_M_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_64_S_ADD:
    case SST::Forza::zopOpc::Z_HAC_64_S_AND:
    case SST::Forza::zopOpc::Z_HAC_64_S_OR:
    case SST::Forza::zopOpc::Z_HAC_64_S_XOR:
    case SST::Forza::zopOpc::Z_HAC_64_S_SMAX:
    case SST::Forza::zopOpc::Z_HAC_64_S_MAX:
    case SST::Forza::zopOpc::Z_HAC_64_S_SMIN:
    case SST::Forza::zopOpc::Z_HAC_64_S_MIN:
    case SST::Forza::zopOpc::Z_HAC_64_S_SWAP:
    case SST::Forza::zopOpc::Z_HAC_64_S_CAS:
    case SST::Forza::zopOpc::Z_HAC_64_S_FADD:
    case SST::Forza::zopOpc::Z_HAC_64_S_FSUB:
    case SST::Forza::zopOpc::Z_HAC_64_S_FRSUB:
    case SST::Forza::zopOpc::Z_HAC_64_MS_ADD:
    case SST::Forza::zopOpc::Z_HAC_64_MS_AND:
    case SST::Forza::zopOpc::Z_HAC_64_MS_OR:
    case SST::Forza::zopOpc::Z_HAC_64_MS_XOR:
    case SST::Forza::zopOpc::Z_HAC_64_MS_SMAX:
    case SST::Forza::zopOpc::Z_HAC_64_MS_MAX:
    case SST::Forza::zopOpc::Z_HAC_64_MS_SMIN:
    case SST::Forza::zopOpc::Z_HAC_64_MS_MIN:
    case SST::Forza::zopOpc::Z_HAC_64_MS_SWAP:
    case SST::Forza::zopOpc::Z_HAC_64_MS_CAS:
    case SST::Forza::zopOpc::Z_HAC_64_MS_FADD:
    case SST::Forza::zopOpc::Z_HAC_64_MS_FSUB:
    case SST::Forza::zopOpc::Z_HAC_64_MS_FRSUB:
      *Target = tmp;
      break;
    default:
      output.fatal(CALL_INFO, -1, "%s, Error: zopEvent on zopNIC HZOP AMO response could not be handled; OPC=%d\n",
                  getName().c_str(), (unsigned)(Opc));
      break;
    }
  }

  // setup the zopEvent infrastructure to clear the hazards
  ev->setMemReq(Req);
  ev->setTarget(Target);

  // trigger the RevCPU to clear the hazard
  (*msgHandler)(ev);
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
  output.verbose(CALL_INFO, 9, 0,
                 "%s:%s received zop message of type %s\n",
                 getName().c_str(),
                 endPToStr(getEndpointType()).c_str(),
                 msgTToStr(ev->getType()).c_str());

  // if this is an RZA device, marshall it through to the ZIQ
  if( Type == Forza::zopCompID::Z_RZA ){
    (*msgHandler)(ev);
    return true;
  }

  // this is likely a ZAP device,
  // iterate across the outstanding messages
  unsigned Cur = 0;
  for( auto const& [DestHart, ID, isRead, Target, Opc, Req] : outstanding ){
    auto SrcHart = ev->getDestHart();
    auto EVID = ev->getID();
    if( (DestHart == SrcHart) && (ID == EVID) ){
      // found a match
      // if this is a read request, marshall to the RevCPU to handle the hazarding
      if( isRead ){
        handleLoadResponse(ev, Target, Opc, Req, ID, ev->getSrcHart());
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

bool zopNIC::handleFence(zopEvent *ev){
  // first, determine if this fence has already been encountered
  // if not, incrememnt the fence counter for this hart

  // if this fence has been encountered, then check the oustanding
  // operation vector to see if we have any outstanding requests
  // if no outstanding requests exist, then clear the fence

  // this function returns TRUE if the fence is ready to clear
  // otherwise, this function returns false

  auto ReqHart = ev->getSrcHart();

  if( ev->getFence() ){
    // fence has been encountered, check to see if we need to clear
    for( auto const& [Hart, ID, isRead, Target, Opc, Req] : outstanding ){
      if( Hart == ReqHart ){
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
  bool erase = false;

  // check if there are any outstanding requests
  if( sendQ.empty() ){
    return false;
  }

  for( auto it = sendQ.begin(); it != sendQ.end(); ){
    erase = false;
    if( thisCycle < ReqPerCycle ){
      zopEvent *ev = static_cast<zopEvent*>((*it)->inspectPayload());
      Hart = (unsigned)(ev->getSrcHart());
      if( Type == SST::Forza::zopCompID::Z_RZA ){
        // I am an RZA... I don't need to reserve any message IDs
        auto P = ev->getPacket();
        ev->encodeEvent();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          recordStat( getStatFromPacket(ev), 1 );
          thisCycle++;
          iFace->send((*it), 0);
          it = sendQ.erase(it);
          erase = true;
        }
      }else if( ev->getType() == SST::Forza::zopMsgT::Z_FENCE ){
        // handle the fence operation
        if( handleFence(ev) ){
          // fence is ready to clear
          it = sendQ.erase(it);
          erase = true;
        }
      }else if( (msgId[Hart].getNumFree() > 0) &&
                (HARTFence[Hart] == 0) ){
        // we have a free message Id for this hart
        auto P = ev->getPacket();
        if( iFace->spaceToSend(0, P.size()*64) ){
          // we have space to send
          ev->setID( msgId[Hart].getMsgId() );
          auto V = std::make_tuple(Hart, ev->getID(), ev->isRead(),
                                   ev->getTarget(), ev->getOpc(),
                                   ev->getMemReq());
          outstanding.push_back(V);
          ev->encodeEvent();
          recordStat( getStatFromPacket(ev), 1 );
          thisCycle++;
          iFace->send((*it), 0);
          it = sendQ.erase(it);
          erase = true;
        }
      }
    }else{
      // saturated the number of outstanding requests
      return false;
    }
    if( !erase ){
      it++;
    }
  }
  return false;
}

// EOF
