#include <sst/core/sst_config.h>
#include "zopgen_prec.h"
#include "ZOPNET.h"

using namespace SST::Forza;

ZOPGen_prec::ZOPGen_prec(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 7);
  output.init("ZOPGen_prec[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");

  int_id = params.find<uint64_t>("int_id", 0);

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZOPGen_prec>(this, &ZOPGen_prec::clock));
  output.output("ZOPGen_prec[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "m_zop_iface" );
  m_zop_iface->setEndpointType(zopCompID::Z_ZAP0);
  m_zop_iface->setNumHarts(3);
  m_zop_iface->setPrecinctID(int_id);
  m_zop_iface->setZoneID(0);

  m_zop_iface->setMsgHandler(new Event::Handler<ZOPGen_prec>(this, &ZOPGen_prec::handleIncomingZOP));
}

ZOPGen_prec::~ZOPGen_prec(){
}

void ZOPGen_prec::init(unsigned int phase) {
  m_zop_iface->init(phase);
}

void ZOPGen_prec::setup() {
  m_zop_iface->setup();
  output.verbose(CALL_INFO, 11, 0, "setup int_id %llu\n", int_id);

  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ZENSET);
  zopgenMsg->setSrcZCID(0);
  zopgenMsg->setSrcHart(0);
  payload.push_back(100);
  payload.push_back(100);
  payload.push_back(100);
  payload.push_back(100);
  payload.push_back(100);
  zopgenMsg->setPayload(payload);
  zopgenMsg->encodeEvent();
  m_zop_iface->send(zopgenMsg, zopCompID::Z_ZEN);
}

void ZOPGen_prec::complete(unsigned int phase) {
  m_zop_iface->complete(phase);
}

void ZOPGen_prec::finish() {
  m_zop_iface->finish();
}

bool ZOPGen_prec::clock(Cycle_t cycle){
  if ((cycle == 1) && (int_id == 0)) {
    for (unsigned i=0; i<10; i++) {
      std::vector<uint64_t> payload;
      SST::Forza::zopEvent* zop = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_SENDP);
      zop->setSrcHart(0);
      zop->setSrcZCID(zopCompID::Z_ZAP0);
      zop->setSrcPCID((uint8_t)zopPrecID::Z_ZONE0);
      zop->setSrcPrec(0);
      zop->setDestHart(0);
      zop->setDestZCID(zopCompID::Z_ZAP0);
      zop->setDestPCID((uint8_t)zopPrecID::Z_ZONE0);
      zop->setDestPrec(1);
      payload.push_back(100);
      zop->setPayload(payload);
      zop->encodeEvent();
      m_zop_iface->send(zop, zopCompID::Z_ZEN, zopPrecID::Z_ZONE0, 0);
    }
  }
  return false;
}

void ZOPGen_prec::handleIncomingZOP(SST::Event* event) {
}
