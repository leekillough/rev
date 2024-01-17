//
// _zopgen_cc_
//

#include <sst/core/sst_config.h>
#include "zopgen.h"
#include "ZOPNET.h"

using namespace SST::Forza;

ZOPGen::ZOPGen(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 7);
  output.init("ZOPGen[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
  int_id = params.find<uint64_t>("int_id", 0);
  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZOPGen>(this, &ZOPGen::clock));
  output.output("ZOPGen[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "m_zop_iface" );
  if (int_id == 0) {
    m_zop_iface->setEndpointType(zopCompID::Z_ZAP0);
  } else if (int_id == 1) {
    m_zop_iface->setEndpointType(zopCompID::Z_ZAP1);
  } else {
    m_zop_iface->setEndpointType(zopCompID::Z_RZA);
  }

  //m_linkControl = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 1 );
  m_num_harts = params.find<uint64_t>("num_harts", 4);
  //assert( m_linkControl );
  msg_id = 0;
  cnt = 0;
  sent = false;
  all_sent = false;
  setup_done = false;
  //mem_acks.push_back(0);
  //mem_acks.push_back(1);
  //mem_acks.push_back(2);
  //m_linkControl->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<ZOPGen>(this,&ZOPGen::handleNetworkEvent) );
  // register with SST
  registerAsPrimaryComponent();
  m_zop_iface->setNumHarts(3);
}

ZOPGen::~ZOPGen(){
}

void ZOPGen::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "Init id %llu\n", int_id);
  m_zop_iface->init(phase);
}

void ZOPGen::setup() {
  m_zop_iface->setMsgHandler(new Event::Handler<ZOPGen>(this, &ZOPGen::handleIncomingZOP));

}

void ZOPGen::complete(unsigned int phase) {
  return;
}

void ZOPGen::finish() {
  output.verbose(CALL_INFO, 1, 0, "Finish()\n");
}

void ZOPGen::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);
  //ev->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg type %hhu, opcode %hhu, dest %hu\n", ev->getType(),
                 ev->getOpc(), ev->getDestHart());
  if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SDMA) {
    processMZOPSDMA(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SD) {
    processMZOP(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SCSD) {
    output.verbose(CALL_INFO, 1, 0, "Dest %llu notified, scratch addr: %llu, addr: %llu, size: %llu\n", int_id, ev->getPayload()[0], ev->getPayload()[1],  ev->getPayload()[2]);
    sendLoadToRZA(ev->getPayload()[1], ev->getPayload()[2]);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_LD) {
    processLoad(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
    std::vector<uint64_t> payload = ev->getPayload();
    for (int i = 0; i < payload.size(); ++i) {
      output.verbose(CALL_INFO, 1, 0, "Payload[%d]: %llu\n", i, payload[i]);
    }
    sendCreditsToZEN(++cnt);
  }
}

void ZOPGen::processLoad(SST::Forza::zopEvent *ev) {
  std::vector<uint64_t> resp;
  std::vector<uint64_t> payload = ev->getPayload();
  uint64_t addr = payload[0];
  uint64_t size = payload[1];
  output.verbose(CALL_INFO, 1, 0, "Load Msg addr %llu, size %llu\n", addr, size);
  for (int i = 0; i < size; ++i) {
    resp.push_back(mem_map[addr + i]);
  }
  sendMZOPRespToZAP(ev, resp);
}

void ZOPGen::sendLoadToRZA(uint64_t addr, uint64_t size) {
  std::vector<uint64_t> payload;
  // TODO: Update with RZA id
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_LD);
  rzaMsg->setSrcHart(0);
  rzaMsg->setSrcZCID(int_id);
  rzaMsg->setSrcPCID(0);
  rzaMsg->setSrcPrec(0);
  // TODO: Update with RZA id
  rzaMsg->setDestHart(3);
  //rzaMsg->setRead();
  //rzaMsg->setMsgId(4);
  // fake acs
  payload.push_back(getReadACS(100));
  payload.push_back(addr-size+1);
 // payload.push_back(size);
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  // TODO: Update with RZA id
}

void ZOPGen::sendMsgToRZA(uint64_t addr, uint64_t size) {
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %llu, msg id %" PRIu8 "\n", addr, msg_id);
  std::vector<uint64_t> payload;
  // TODO: Update with RZA id
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart(1);
  rzaMsg->setSrcZCID(int_id);
  rzaMsg->setSrcPCID(0);
  rzaMsg->setSrcPrec(0);
  // TODO: Update with RZA id
  rzaMsg->setDestHart(3);
  payload.push_back(size);
  payload.push_back(addr);
  // payload.push_back(value);
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  // TODO: Update with RZA id
}

void ZOPGen::sendMZOPRespToZAP(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload) {
  uint8_t msg_id = ev->getID();
  output.verbose(CALL_INFO, 1, 0, "msg id %" PRIu8 " header %llu\n", msg_id, ev->getPacket()[0]);
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_RESP);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_RESP_LR);
  zopgenMsg->setSrcHart(m_zop_iface->getAddress());
  // TODO: Update with RZA id
  zopgenMsg->setDestHart(1);
  zopgenMsg->setPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, (zopCompID)1);

}

void ZOPGen::processMZOPSDMA(SST::Forza::zopEvent *ev) {

  std::vector<uint64_t> payload = ev->getPayload();
  for (int i = 0; i < payload.size(); ++i) {
    output.verbose(CALL_INFO, 1, 0, "Payload[%d]: %llu\n", i, payload[i]);
  }
  uint64_t addr = payload[1];
  for (int i = 3; i < 3 + payload[2]; ++i) {
    mem_map[addr] = payload[i];
    output.verbose(CALL_INFO, 1, 0, "Stored %llu in addr %llu\n", payload[i], addr);

    addr += 1;
  }
  sendMZOPAckToZOPGen(ev);
}

void ZOPGen::processMZOP(SST::Forza::zopEvent *ev) {

  std::vector<uint64_t> payload = ev->getPayload();
  for (int i = 0; i < payload.size(); ++i) {
    output.verbose(CALL_INFO, 1, 0, "Payload[%d]: %llu\n", i, payload[i]);
  }
  uint64_t addr = payload[1];
  mem_map[addr] = payload[2];
  output.verbose(CALL_INFO, 1, 0, "Stored %llu in addr %llu\n", payload[2], addr);
  sendMZOPAckToZOPGen(ev);
}

void ZOPGen::sendMZOPAckToZOPGen(SST::Forza::zopEvent *ev) {
  uint8_t msg_id = ev->getID();
  output.verbose(CALL_INFO, 1, 0, "msg id %" PRIu8 " header %llu\n", msg_id, ev->getPacket()[0]);
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_RESP);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_SENDP);
  zopgenMsg->setSrcHart(m_zop_iface->getAddress());
  // TODO: Update with RZA id
  zopgenMsg->setDestHart(1);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZEN);

}
uint64_t ZOPGen::getWriteACS(uint64_t acs_pair) {
  return acs_pair & 0xFFFFFFFF00000000ULL;
}

uint64_t ZOPGen::getReadACS(uint64_t acs_pair) {
  return (acs_pair & 0xFFFFFFFFFFFFFFFFULL) >> 32;
}

void ZOPGen::sendSetupToZOPGen() {
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ZENSET);
  zopgenMsg->setSrcZCID(int_id);
  zopgenMsg->setSrcHart(int_id);
  zopgenMsg->setDestHart(1);
  payload.push_back(100);
  payload.push_back(1000);
  payload.push_back(20);
  payload.push_back(100);
  zopgenMsg->setPayload(payload);
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %llu\n", zopgenMsg->getPacket()[1]);
  zopgenMsg->encodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %llu\n", zopgenMsg->getPacket()[1]);
  zopgenMsg->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %llu\n", zopgenMsg->getPacket()[1]);
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZEN);
}

void ZOPGen::sendCreditsToZEN(int i) {
  output.verbose(CALL_INFO, 1, 0, "Msg credits %d\n", i);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_CREDIT);
  zopgenMsg->setCredit(8);
  zopgenMsg->setSrcZCID(int_id);
  zopgenMsg->setSrcHart(int_id);
  // TODO: Update with RZA id
  if (int_id == 0)
    zopgenMsg->setDestHart(1);
  else
    zopgenMsg->setDestHart(0);
//  payload.push_back(10);
//  payload.push_back(100*(i+3));
//  payload.push_back(1000000);
//  payload.push_back(100*(i+3));
//  payload.push_back(100*(i+3));
//  payload.push_back(100*(i+3));
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZEN);
  if (i % 10 == 0) all_sent = false;
}


void ZOPGen::sendMsgToZOPGen(int i) {
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_SENDP);
    // TODO: Update with RZA id
    if (int_id == 0) {
        zopgenMsg->setSrcHart(int_id);
        zopgenMsg->setDestHart(1);
        zopgenMsg->setSrcZCID((uint8_t)zopCompID::Z_ZAP0);
        zopgenMsg->setDestZCID((uint8_t)zopCompID::Z_ZAP1);
        zopgenMsg->setDestPCID(0);
    } else {
        zopgenMsg->setSrcHart(int_id);
        zopgenMsg->setDestHart(0);
        zopgenMsg->setSrcZCID((uint8_t)zopCompID::Z_ZAP1);
        zopgenMsg->setDestZCID((uint8_t)zopCompID::Z_ZAP0);
        zopgenMsg->setDestPCID(0);
    }
  payload.push_back(100+i);
//  payload.push_back(10);
//  payload.push_back(100*(i+3));
//  payload.push_back(1000000);
//  payload.push_back(100*(i+3));
//  payload.push_back(100*(i+3));
//  payload.push_back(100*(i+3));
  zopgenMsg->setPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  if (int_id == 0) m_zop_iface->send(zopgenMsg, zopCompID::Z_ZAP1);
  else m_zop_iface->send(zopgenMsg, zopCompID::Z_ZAP0);
}

bool ZOPGen::clock(Cycle_t cycle){
  if (int_id == 8) {
    setup_done = true;
  }
  if (setup_done && cycle > 300000 && !sent && int_id != 8) {
    output.verbose(CALL_INFO, 1, 0, "Msg send skip %llu\n", cycle);
    //sendMsgToZOPGen();
    sent = true;
  }
  if (!setup_done && int_id < 8) {
    output.verbose(CALL_INFO, 1, 0, "Msg setup %llu\n", int_id);
    sendSetupToZOPGen();
    setup_done = true;
  }
  if (!all_sent && sent) {
    if (int_id != cnt/10 % 2) return false;
    else { output.verbose(CALL_INFO, 1, 0, "int id sending: %llu\n", int_id); }
    for (int i = 0; i < 10; ++i) {
      output.verbose(CALL_INFO, 1, 0, "Msg send to zen%llu\n", cycle);
      sendMsgToZOPGen(i);
    }
    all_sent = true;
  }
  return false;
}

// EOF
