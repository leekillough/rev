//
// _zip_events_cc_
//

#include "zip_events.h"

using namespace SST::Forza;

SST::Event* ZIPEvent::clone(void) {
  ZIPEvent* ev = new ZIPEvent(*this);
  return ev;
}

// get payload from aggregated ZOPs represented as vector concatenated packets
std::vector<uint64_t> ZIPAggEvent::getPayload() {
  return payload;
}

// get aggregated ZOPs as vector of individual ZOPs
std::vector<zopEvent> ZIPAggEvent::getZOPs() {
  std::vector<zopEvent> ZOPs;

  // iterate over bytes i of the payload vector
  unsigned i = 0;
  while (i < payload.size()) {
    // determine zopEvent Length from header flits
    uint8_t Length = ((payload[i+Z_FLIT_FLITLEN] >> Z_SHIFT_FLITLEN) & Z_MASK_FLITLEN);
    // construct new packet based on zopEvent length
    std::vector<uint64_t> newPacket;
    for (int j=0; j<Z_NUM_HEADER_FLITS+Length; j++) {
      newPacket.push_back(payload[i]);
      i++;
    }

    // create a new zopEvent with the new packet
    zopEvent newZOP;
    newZOP.setPacket(newPacket);
    newZOP.decodeEvent();

    ZOPs.push_back(newZOP);
  }

  return ZOPs;
}

void ZIPAggEvent::serialize_order(SST::Core::Serialization::serializer& _serializer) {
  SST::Event::serialize_order(_serializer);
  _serializer& payload;
}

// return number of credits being sent
int ZIPCreditEvent::getCredits() {
  return credits;
}

void ZIPCreditEvent::serialize_order(SST::Core::Serialization::serializer& _serializer) {
  SST::Event::serialize_order(_serializer);
  _serializer& credits;
}
