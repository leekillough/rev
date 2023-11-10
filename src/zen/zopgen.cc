//
// _zopgen_cc_
//

#include <sst/core/sst_config.h>
#include "zopgen.h"
#include "ZOPNet.h"

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
  //m_linkControl = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 1 );
  m_num_harts = params.find<uint64_t>("num_harts", 4);
  //assert( m_linkControl );
  msg_id = 0;
  sent = false;
  setup_done = false;
  //mem_acks.push_back(0);
  //mem_acks.push_back(1);
  //mem_acks.push_back(2);
  //m_linkControl->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<ZOPGen>(this,&ZOPGen::handleNetworkEvent) );
  // register with SST
  registerAsPrimaryComponent();
}

ZOPGen::~ZOPGen(){
}

void ZOPGen::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "Init id %lu\n", int_id);
  m_zop_iface->init(phase);
}

void ZOPGen::setup() {
  output.verbose(CALL_INFO, 1, 0, "Setup id %lu\n", int_id);
  m_zop_iface->setMsgHandler(new Event::Handler<ZOPGen>(this, &ZOPGen::handleIncomingZOP));

}

void ZOPGen::complete(unsigned int phase) {
  return;
}

void ZOPGen::finish() {
  output.verbose(CALL_INFO, 1, 0, "Finish()\n");
}

void ZOPGen::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = dynamic_cast<SST::Forza::zopEvent*>(event);
  ev->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "Msg type %d, opcode %ld, dest %d\n", ev->getType(),
                 ev->getOpcode(), ev->getDest());
  if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpcode() == SST::Forza::zopOpc::Z_RZA_STORE) {
    sendMZOPAckToZOPGen(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MZOP && ev->getOpcode() == SST::Forza::zopOpc::Z_SCR_STORE) {
    output.verbose(CALL_INFO, 1, 0, "Dest %d notified\n", int_id);
  }
}

void ZOPGen::sendMZOPAckToZOPGen(SST::Forza::zopEvent *ev) {
  uint8_t msg_id = ev->getID();
  output.verbose(CALL_INFO, 1, 0, "msg id %" PRIu8 " header %lu\n", msg_id, ev->getPacket()[0]);
  std::vector<uint32_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), 1);
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_RESP);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_SEND);
  zopgenMsg->setSrc(m_zop_iface->getAddress());
  // TODO: Update with RZA id
  zopgenMsg->setDest(1);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, 1);

}

void ZOPGen::sendSetupToZOPGen() {
  std::vector<uint32_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), 1);
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_ZENSETUP);
  zopgenMsg->setSrc(m_zop_iface->getAddress());
  // TODO: Update with RZA id
  zopgenMsg->setDest(1);
  payload.push_back(100);
  payload.push_back(1000);
  payload.push_back(900);
  payload.push_back(100);
  payload.push_back(1000);
  zopgenMsg->setPacketPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, 1);
}

void ZOPGen::sendMsgToZOPGen() {
  std::vector<uint32_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setID(msg_id);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_SEND);
  zopgenMsg->setSrc(m_zop_iface->getAddress());
  // TODO: Update with RZA id
  zopgenMsg->setDest(2);
  payload.push_back(100);
  zopgenMsg->setPacketPayload(payload);
  zopgenMsg->encodeEvent();
  // TODO: Update with RZA id
  m_zop_iface->send(zopgenMsg, 2);
}

bool ZOPGen::clock(Cycle_t cycle){
  if (setup_done && cycle > 300000 && !sent && int_id == 0) {
    output.verbose(CALL_INFO, 1, 0, "Msg send %d\n", cycle);
    sendMsgToZOPGen();
    sent = true;
  }
  if (!setup_done) {
    output.verbose(CALL_INFO, 1, 0, "Msg setup %d\n", cycle);
    sendSetupToZOPGen();
    setup_done = true;
  }
  return false;
}

// EOF
