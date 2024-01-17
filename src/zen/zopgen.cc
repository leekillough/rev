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
              Verbosity, params.find<unsigned int>("tests", 0), SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
  int_id = params.find<uint64_t>("int_id", 0);
  // register the clock handler
  zgTime = registerClock(cpuFreq, new Clock::Handler<ZOPGen>(this, &ZOPGen::clock));
  output.output("ZOPGen[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "m_zop_iface" );
  m_zop_iface->setMsgHandler(new Event::Handler<ZOPGen>(this, &ZOPGen::handleIncomingZOP));
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
  m_zop_iface->setPrecinctID(0);
  m_zop_iface->setZoneID(params.find<unsigned>("zoneId", 0));

  num_loops1 = 0;
  num_loops2 = 0;

  t_c1 = false;
  t_c3 = false;
  t_c5 = false;
  t_p1 = UnitAlgebra("0b");
  t_p2 = UnitAlgebra("0b");
  t_p4 = 0;
  t_p5 = 0;
}

ZOPGen::~ZOPGen(){
}

void ZOPGen::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "Init id %lu\n", int_id);
  m_zop_iface->init(phase);
}

void ZOPGen::setup() {
  m_zop_iface->setup();

}

void ZOPGen::complete(unsigned int phase) {
  return;
}

void ZOPGen::finish() {
  output.verbose(CALL_INFO, 1, 0, "Finish()\n");
  t_p1 /= t_p4;
  t_p1 /= zgTime->getPeriod();
  t_p1 *= num_loops1;
  t_p2 /= t_p5;
  t_p2 /= zgTime->getPeriod();
  t_p2 *= num_loops2;
  if (t_c1) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_C1] pass\n");
  if (t_c3) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_C3] pass\n");
  if (t_c5) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_C5] pass\n");
  if (t_p4) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P1] %s\n", t_p1.toStringBestSI().c_str());
  if (t_p5) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P2] %s\n", t_p2.toStringBestSI().c_str());
  if (t_p4) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P4] %f cycles\n", 1.0*t_p4/num_loops1);
  // if (t_p5) output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P5] %f cycles\n", 1.0*t_p5/num_loops2);
  if (t_p4) {
    t_p3 = UnitAlgebra(std::to_string(2*num_loops1)+"events");
    t_p3 /= t_p4;
    t_p3 /= zgTime->getPeriod();
    output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P3] %s\n", t_p3.toStringBestSI().c_str());
  } else if (t_p5) {
    t_p3 = UnitAlgebra(std::to_string(2*num_loops2)+"events");
    t_p3 /= t_p5;
    t_p3 /= zgTime->getPeriod();
    output.verbose(CALL_INFO, 1, 1, "[TEST ZEN_P3] %s\n", t_p3.toStringBestSI().c_str());
  }
  // output.verbose(CALL_INFO, 1, 1, "num_loops1: %d, num_loops2: %d\n", num_loops1, num_loops2);
}

void ZOPGen::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);
  //ev->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg type %hhu, opcode %hhu, dest %hu\n", (int)(ev->getType()),
                 (int)(ev->getOpc()), ev->getDestHart());
  if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SDMA) {
    processMZOPSDMA(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SD) {
    processMZOP(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_SCSD) {
    output.verbose(CALL_INFO, 1, 0, "Dest %lu notified, scratch addr: %lu, addr: %lu, size: %lu\n", int_id, ev->getPayload()[0], ev->getPayload()[1],  ev->getPayload()[2]);
    sendLoadToRZA(ev->getPayload()[1], ev->getPayload()[2]);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpc() == SST::Forza::zopOpc::Z_MZOP_LD) {
    processLoad(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
    std::vector<uint64_t> payload = ev->getPayload();
    for (long unsigned int i = 0; i < payload.size(); ++i) {
      output.verbose(CALL_INFO, 1, 0, "Payload[%lu]: %lu\n", i, payload[i]);
    }
    if (payload[1] == 77) t_c1 = true;
    if (payload[1] == 88) t_c3 = true;
    if (t_c1 && (payload[1] > 100)) {
      t_p1 += UnitAlgebra(std::to_string(payload.size()*64)+"b");
      t_p4 += getNextClockCycle(zgTime)-payload[1];
      num_loops1++;
    }
    if (t_c3 && (payload[1] > 100)) {
      t_p2 += UnitAlgebra(std::to_string(payload.size()*64)+"b");
      t_p5 += getNextClockCycle(zgTime)-payload[1];
      num_loops2++;
    }
    sendCreditsToZEN(++cnt);
  } else if ((ev->getType() == SST::Forza::zopMsgT::Z_MSG) && (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_EXCP)) {
    t_c5 = true;
  }
}

void ZOPGen::processLoad(SST::Forza::zopEvent *ev) {
  std::vector<uint64_t> resp;
  std::vector<uint64_t> payload = ev->getPayload();
  uint64_t addr = payload[0];
  uint64_t size = payload[1];
  output.verbose(CALL_INFO, 1, 0, "Load Msg addr %lu, size %lu\n", addr, size);
  for (uint64_t i = 0; i < size; ++i) {
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
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %lu, msg id %" PRIu8 "\n", addr, msg_id);
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
  output.verbose(CALL_INFO, 1, 0, "msg id %" PRIu8 " header %lu\n", msg_id, ev->getPacket()[0]);
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
  for (long unsigned int i = 0; i < payload.size(); ++i) {
    output.verbose(CALL_INFO, 1, 0, "Payload[%lu]: %lu\n", i, payload[i]);
  }
  uint64_t addr = payload[1];
  for (long unsigned int i = 3; i < 3 + payload[2]; ++i) {
    mem_map[addr] = payload[i];
    output.verbose(CALL_INFO, 1, 0, "Stored %lu in addr %lu\n", payload[i], addr);

    addr += 1;
  }
  sendMZOPAckToZOPGen(ev);
}

void ZOPGen::processMZOP(SST::Forza::zopEvent *ev) {

  std::vector<uint64_t> payload = ev->getPayload();
  for (long unsigned int i = 0; i < payload.size(); ++i) {
    output.verbose(CALL_INFO, 1, 0, "Payload[%lu]: %lu\n", i, payload[i]);
  }
  uint64_t addr = payload[1];
  mem_map[addr] = payload[2];
  output.verbose(CALL_INFO, 1, 0, "Stored %lu in addr %lu\n", payload[2], addr);
  sendMZOPAckToZOPGen(ev);
}

void ZOPGen::sendMZOPAckToZOPGen(SST::Forza::zopEvent *ev) {
  uint8_t msg_id = ev->getID();
  output.verbose(CALL_INFO, 1, 0, "msg id %" PRIu8 " header %lu\n", msg_id, ev->getPacket()[0]);
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
  zopgenMsg->setSrcPCID(m_zop_iface->getZoneID());
  zopgenMsg->setDestHart(1);
  payload.push_back(100);
  payload.push_back(1000);
  payload.push_back(100);
  payload.push_back(100);
  zopgenMsg->setPayload(payload);
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %lu\n", zopgenMsg->getPacket()[1]);
  zopgenMsg->encodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %lu\n", zopgenMsg->getPacket()[1]);
  zopgenMsg->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg send src %d\n", zopgenMsg->getSrcHart());
  output.verbose(CALL_INFO, 1, 0, "Msg send flit %lu\n", zopgenMsg->getPacket()[1]);
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
  zopgenMsg->setSrcHart(int_id);
  zopgenMsg->setDestHart(1);
  zopgenMsg->setSrcZCID((uint8_t)zopCompID::Z_ZAP0);
  zopgenMsg->setDestZCID((uint8_t)zopCompID::Z_ZAP1);
  zopgenMsg->setSrcPCID(m_zop_iface->getZoneID());
  zopgenMsg->setDestPCID(m_zop_iface->getZoneID());
  payload.push_back(i);
  zopgenMsg->setPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZAP1);
}

void ZOPGen::sendMsgToZOPGen2(int i) {
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_SENDP);
  // TODO: Update with RZA id
  zopgenMsg->setSrcHart(int_id);
  zopgenMsg->setDestHart(1);
  zopgenMsg->setSrcZCID((uint8_t)zopCompID::Z_ZAP0);
  zopgenMsg->setDestZCID((uint8_t)zopCompID::Z_ZAP1);
  zopgenMsg->setSrcPCID(m_zop_iface->getZoneID());
  zopgenMsg->setDestPCID(1);
  payload.push_back(i);
  zopgenMsg->setPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZEN, zopPrecID::Z_ZONE0, 0);
}

bool ZOPGen::clock(Cycle_t cycle){
  if (int_id == 8) {
    setup_done = true;
  }
  if (setup_done && cycle > 300000 && !sent && int_id != 8) {
    output.verbose(CALL_INFO, 1, 0, "Msg send skip %lu\n", cycle);
    //sendMsgToZOPGen();
    sent = true;
  }
  if (!setup_done && int_id < 8) {
    output.verbose(CALL_INFO, 1, 0, "Msg setup %lu\n", int_id);
    sendSetupToZOPGen();
    setup_done = true;
  }
  if (!all_sent && sent && int_id == 0 && m_zop_iface->getZoneID() == 0) {
    output.verbose(CALL_INFO, 1, 0, "Msg send to zen%lu\n", cycle);
    sendMsgToZOPGen(77);
    sendMsgToZOPGen2(88);
    all_sent = true;
  }
  if (!all_sent2 && sent && int_id == 0 && m_zop_iface->getZoneID() == 0) {
    for (unsigned j=0; j<40; j++) {
      output.verbose(CALL_INFO, 1, 0, "Msg send to zen%" PRIu64 "\n", cycle);
      sendMsgToZOPGen(getNextClockCycle(zgTime));
      sendMsgToZOPGen2(getNextClockCycle(zgTime));
    }
    all_sent2 = true;
  }
  return false;
}

// EOF
