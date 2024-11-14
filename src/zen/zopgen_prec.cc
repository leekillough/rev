#include <sst/core/sst_config.h>
#include "zopgen_prec.h"
#include "ZOPNET.h"

using namespace SST::Forza;

ZOPGen_prec::ZOPGen_prec(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 0);
  output.init("ZOPGen_prec[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");

  int_id = params.find<uint64_t>("int_id", 0);
  p_zone_id = params.find<uint64_t>("zone_id", 1);
  p_numZOPs = params.find<uint64_t>("num_ZOPs", 1);
  p_numCycles = params.find<uint64_t>("num_cycles", 1);
  p_interval = params.find<uint64_t>("interval", 1);
  p_test = params.find<uint64_t>("test", 0);

  // register the clock handler
  if (p_test) {
    registerClock(cpuFreq, new Clock::Handler<ZOPGen_prec>(this, &ZOPGen_prec::clock2));
  } else {
    registerClock(cpuFreq, new Clock::Handler<ZOPGen_prec>(this, &ZOPGen_prec::clock));
  }
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
  output.verbose(CALL_INFO, 11, 0, "setup int_id %" PRIu64 "\n", int_id);

  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zopgenMsg = new SST::Forza::zopEvent();
  zopgenMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  //zopgenMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ZENSET);
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
  if ((0 < cycle) && (cycle % p_interval == 0) && (cycle <= p_interval*p_numCycles) && (int_id > 0)) {
    for (unsigned i=0; i<p_numZOPs; i++) {
      std::vector<uint64_t> payload;
      SST::Forza::zopEvent* zop = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_SENDP);
      zop->setSrcHart(0);
      zop->setSrcZCID(zopCompID::Z_ZAP0);
      zop->setSrcPCID((uint8_t)zopPrecID::Z_ZONE0);
      zop->setSrcPrec(int_id);
      zop->setDestHart(0);
      zop->setDestZCID(zopCompID::Z_ZAP0);
      zop->setDestPCID((uint8_t)p_zone_id);
      zop->setDestPrec(0);
      payload.push_back(100);
      zop->setPayload(payload);
      zop->encodeEvent();
      m_zop_iface->send(zop, zopCompID::Z_ZEN, zopPrecID::Z_ZONE0, 0);
    }
  }
  return false;
}

bool ZOPGen_prec::clock2(Cycle_t cycle) {
  if ((cycle == 1) && (int_id == 0)) {
    std::vector<uint64_t> payload;
    SST::Forza::zopEvent* zop = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_SENDP);
    zop->setSrcHart(0);
    zop->setSrcZCID(zopCompID::Z_ZAP0);
    zop->setSrcPCID(p_zone_id);
    zop->setSrcPrec(0);
    zop->setDestHart(0);
    zop->setDestZCID(zopCompID::Z_ZAP0);
    zop->setDestPCID((uint8_t)zopPrecID::Z_ZONE0);
    zop->setDestPrec(p_zone_id+1);
    payload.push_back(100);
    zop->setPayload(payload);
    zop->encodeEvent();
    m_zop_iface->send(zop, zopCompID::Z_ZEN, (zopPrecID)p_zone_id, 0);
  }
  return false;
}

void ZOPGen_prec::handleIncomingZOP(SST::Event* event) {
}
