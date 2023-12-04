//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"
#include "ZOPNet.h"

using namespace SST::Forza;

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 7);
  output.init("ZEN[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
  int_id = params.find<uint64_t>("int_id", 0);

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZEN>(this, &ZEN::clock));
  output.output("ZEN[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "m_zop_iface" );
  m_zop_iface->setEndpointType(zopCompID::Z_ZEN);
  m_num_harts = params.find<uint64_t>("num_harts", 4);
  msg_id = 0;
  sent = false;
  registerAsPrimaryComponent();
}

ZEN::~ZEN(){
}

uint64_t ZEN::getWriteACS(uint64_t acs_pair) {
  return acs_pair & 0xFFFFFFFF00000000ULL;
}

uint64_t ZEN::getReadACS(uint64_t acs_pair) {
  return (acs_pair & 0xFFFFFFFFFFFFFFFFULL) >> 32;
}

void ZEN::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "Init id %lu\n", int_id);
  m_zop_iface->init(phase);
}

void ZEN::setup() {
  output.verbose(CALL_INFO, 1, 0, "Setup id %lu\n", int_id);
  m_zop_iface->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingZOP));

}

void ZEN::complete(unsigned int phase) {
  return;
}

void ZEN::finish() {
  output.verbose(CALL_INFO, 1, 0, "Finish()\n");
}

void ZEN::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);
  output.verbose(CALL_INFO, 1, 0, "Msg type %lu, src %ld, dest %d src%lu\n", ev->getType(),
                 ev->getPacket()[1], ev->getDestHart(), ev->getSrcHart());
  if (ev->getType() != SST::Forza::zopMsgT::Z_MSG && ev->getType() != SST::Forza::zopMsgT::Z_RESP) {
    output.verbose(CALL_INFO, 1, 0, "Invalid msg type %d, expected %d or %d\n", ev->getType(),
                   SST::Forza::zopMsgT::Z_MSG, SST::Forza::zopMsgT::Z_RESP);
    // TODO: Send NACK
    return;
  }
  if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDP) {
    // read harts/zone
    zen_queue[ev->getDestHart()].push_back(new ZENEntry(ev, 0));
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
    mem_acks.push_back(ev); // TODO: Add getMsgId() to ZOPNet
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_ZENSET) {
    setup_reqs.push_back(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_CREDIT) {
    zap_credits.push_back(ev);
  }
}

void ZEN::sendMsgToRZA(uint64_t acs, uint64_t addr, std::vector<uint64_t> src_payload, uint8_t msg_id) {
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %d, msg id %" PRIu8 "\n", addr, msg_id);
  std::vector<uint64_t> payload;
  // TODO: Update with RZA id
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart(m_zop_iface->getAddress());
  rzaMsg->setSrcZCID(0);
  rzaMsg->setSrcPCID(0);
  rzaMsg->setSrcPrec(0);
  // TODO: Update with RZA id
  rzaMsg->setDestHart(3);
  payload.push_back(acs);
  payload.push_back(addr);
  payload.push_back(src_payload.size());
  payload.insert(std::end(payload), std::begin(src_payload), std::end(src_payload));
  // payload.push_back(value);
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
  rzaMsg->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
  // TODO: Update with RZA id
}

void ZEN::sendMsgToScratchpad(uint64_t dest, uint64_t scratch_addr, uint64_t size, uint64_t addr) {
  output.verbose(CALL_INFO, 1, 0, "hart %d, scratch a %d, size %d, addr %d\n", dest, scratch_addr, size, addr);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zapMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
  zapMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  zapMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SCSD);
  zapMsg->setSrcHart(m_zop_iface->getAddress());
  zapMsg->setSrcZCID(0);
  zapMsg->setSrcPCID(0);
  zapMsg->setSrcPrec(0);
  zapMsg->setDestHart(dest);
  zapMsg->setDestZCID(0);
  zapMsg->setDestPCID(0);
  zapMsg->setDestPrec(0);
  // TODO: Push back ACS
  payload.push_back(scratch_addr);
  payload.push_back(addr);
  payload.push_back(size);
  zapMsg->setPayload(payload);
  zapMsg->encodeEvent();
  m_zop_iface->send(zapMsg, (zopCompID)dest);
}

void ZEN::notifyHARTScratchpad() {
 // output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad\n");
  uint64_t cur_tail = 0;
  for (int hart = 0; hart < m_num_harts; ++hart) {
    //output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad id %d\n", hart);
    for (int i = 0; i < zen_queue[hart].size(); ++i) {
      std::vector<uint64_t> payload = zen_queue[hart][i]->msg->getPayload();
      if (zen_queue[hart][i]->status == 2) {
        cur_tail = zen_queue[hart][i]->tail;
        zen_queue[hart][i]->status = 6;
        if (!hart_tables[hart]) {
          output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad id %d fail\n", hart);
        } else {
          sendMsgToScratchpad(hart, hart_tables[hart]->scratch_tail, payload.size(), zen_queue[hart][i]->tail);
        }
      }
    }
    zen_queue[hart].erase(std::remove_if(
      zen_queue[hart].begin(), zen_queue[hart].end(),
      [](auto x) {
          return x->status == 6;
      }), zen_queue[hart].end());
  }
}

void ZEN::handleIncomingRZAMsg() {
  //output.verbose(CALL_INFO, 1, 0, "Progress RZA acks\n");
  // This is supposed to look at the RZA msgs and start progressing status of whatever
  // got an ACK
  for (int i = 0; i < mem_acks.size(); ++i) {
      output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu\n", mem_acks[i]->getID());
      if (outstanding_mem_req.count(mem_acks[i]->getID())) {
        uint64_t hart_id = outstanding_mem_req[mem_acks[i]->getID()].first;
        uint64_t queue_loc = outstanding_mem_req[mem_acks[i]->getID()].second;
        zen_queue[hart_id][queue_loc]->status = 2;
        sendACKToZAP(hart_id);
        delete mem_acks[i];
        mem_acks[i] = NULL;
      }
  }
  mem_acks.erase(std::remove_if(
    mem_acks.begin(), mem_acks.end(),
    [](auto x) {
        return !x;
    }), mem_acks.end());
}

void ZEN::sendNACKToZAP(uint64_t dest) {
  std::vector<uint32_t> payload;
  // TODO: Update with ZAP id
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  nackMsg->setSrcHart(m_zop_iface->getAddress());
  nackMsg->setSrcZCID(0);
  nackMsg->setSrcPCID(0);
  nackMsg->setSrcPrec(0);
  nackMsg->setDestHart(dest);
  nackMsg->setDestZCID(0);
  nackMsg->setDestPCID(0);
  nackMsg->setDestPrec(0);
  nackMsg->encodeEvent();
  m_zop_iface->send(nackMsg, (zopCompID)dest);
}

void ZEN::sendACKToZAP(uint64_t dest) {
  std::vector<uint32_t> payload;
  // TODO: Update with ZAP id
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart(m_zop_iface->getAddress());
  ackMsg->setSrcZCID(0);
  ackMsg->setSrcPCID(0);
  ackMsg->setSrcPrec(0);
  ackMsg->setDestHart(dest);
  ackMsg->setDestZCID(0);
  ackMsg->setDestPCID(0);
  ackMsg->setDestPrec(0);
  ackMsg->encodeEvent();
  m_zop_iface->send(ackMsg, (zopCompID)dest);
}

uint64_t ZEN::getRZATailQueue(uint64_t hart_id, uint64_t size) {
  // FIXME: Track if ahead of line blocking due to no clears
  // FIXME: Track if current tail and final tail space not enough to store
  if (!hart_tables[hart_id]) { /* TODO: Raise exception */
    output.verbose(CALL_INFO, 1, 0, "no table entry for %d\n", hart_id);
    return 0;
  }
  uint64_t cur_tail = hart_tables[hart_id]->mem_cur_tail;
  uint64_t cur_head = hart_tables[hart_id]->mem_cur_head;
  output.verbose(CALL_INFO, 1, 0, "RZA: cur_head: %lu, cur_tail: %lu\n", cur_head, cur_tail);
  if (cur_tail >= cur_head) {
    if (cur_tail == cur_head && !hart_tables[hart_id]->empty) {
      return -1;
    }
    if (cur_tail + size > hart_tables[hart_id]->mem_tail) {
      // TODO: Write padding
      hart_tables[hart_id]->mem_cur_tail = hart_tables[hart_id]->mem_head + size;
      hart_tables[hart_id]->empty = false;
      return hart_tables[hart_id]->mem_head;
    } else {
      return cur_tail;
    }
  } else {
    // Wraparound
    if (cur_tail + size > cur_head) {
      // Not enough space
      return -1;
    } else {
      hart_tables[hart_id]->empty = false;
      return cur_tail;
    }
  }
}


void ZEN::printZenQueue() {
  for (int harts = 0; harts < m_num_harts; ++harts) {
    if (zen_queue[harts].size() > 0) output.verbose(CALL_INFO, 1, 0, "Hart: %d, size: %d\n", harts, zen_queue[harts].size());
  }
}

void ZEN::processEgressQueue() {
  //output.verbose(CALL_INFO, 1, 0, "Progress egress queue\n");
  for (int harts = 0; harts < m_num_harts; ++harts) {
    for (int i = 0; i < zen_queue[harts].size(); ++i) {
      std::vector<uint64_t> payload = zen_queue[harts][i]->msg->getPayload();
      if (zen_queue[harts][i]->status == 0) {
        output.verbose(CALL_INFO, 1, 0, "progress status %lu\n", zen_queue[harts][i]->status);
        // issue memory request
        // TODO: lookup table to find addr
        uint64_t rza_addr = getRZATailQueue(harts, (uint64_t)zen_queue[harts][i]->msg->getLength());
        if (rza_addr < 0) {
          sendNACKToZAP(harts);
          zen_queue[harts][i]->status = 5;
          return;
        }
        sendMsgToRZA(getWriteACS(hart_tables[harts]->acs_pair), rza_addr, payload, msg_id);
        zen_queue[harts][i]->status = 1;
        zen_queue[harts][i]->tail = rza_addr;
        outstanding_mem_req[msg_id++] = std::make_pair(harts, i);
      }
      zen_queue[harts].erase(std::remove_if(
        zen_queue[harts].begin(), zen_queue[harts].end(),
        [](auto x) {
            return x->status == 5;
        }), zen_queue[harts].end());
      }
  }
}

void ZEN::processZAPCredits() {
  //output.verbose(CALL_INFO, 1, 0, "Progress credits\n");
  for (int i = 0; i < zap_credits.size(); ++i) {
    uint64_t hart_id = zap_credits[i]->getSrcHart();
    uint64_t credits = setup_reqs[i]->getCredit();
    uint64_t cur_tail = hart_tables[hart_id]->mem_cur_tail;
    uint64_t cur_head = hart_tables[hart_id]->mem_cur_head;
    if (cur_tail > cur_head) {
      if (cur_head + credits > cur_tail) {
        // TODO: NACK
        return;
      }
      hart_tables[hart_id]->mem_cur_head += credits;
      if (hart_tables[hart_id]->mem_cur_head == cur_tail) {
        hart_tables[hart_id]->empty = true;
      }
    } else if (cur_tail < cur_head) {
      cur_head = (cur_head + credits) % hart_tables[hart_id]->mem_tail;
      if (cur_head > cur_tail) {
        // TODO: NACK;
      }
      hart_tables[hart_id]->mem_cur_head = cur_head;
      if (hart_tables[hart_id]->mem_cur_head == cur_tail) {
        hart_tables[hart_id]->empty = true;
      }
    }
    delete zap_credits[i];
    zap_credits[i] = NULL;
  }
  zap_credits.erase(std::remove_if(
    zap_credits.begin(), zap_credits.end(),
    [](auto x) {
        return !x;
    }), zap_credits.end());
}

void ZEN::processSetupMsgs() {
  //output.verbose(CALL_INFO, 1, 0, "Progress setup msg\n");
  for (int i = 0; i < setup_reqs.size(); ++i) {
    std::vector<uint64_t> payload = setup_reqs[i]->getPayload();
    uint64_t hart_id = setup_reqs[i]->getSrcHart();
    output.verbose(CALL_INFO, 1, 0, "setup pkt size %lu for hart %lu\n", payload.size(), hart_id);
    if (setup_reqs[i]->getPayload().size() < 4 || hart_tables.find(hart_id) != hart_tables.end()) {
      sendNACKToZAP(hart_id);
    }
    // TODO: Specify payload format
    uint64_t acs_pair = payload[0];
    uint64_t mem_start_addr = payload[1];
    uint64_t mem_end_addr = payload[2];
    uint64_t size = payload[3];
    uint64_t scratch_tail = payload[4];
    hart_tables[hart_id] = new ZENTableRow(acs_pair, mem_start_addr, mem_end_addr, size, scratch_tail, 500);
    //sendACKToZAP(hart_id);
    output.verbose(CALL_INFO, 1, 0, "setup hart table %d\n", hart_id);
    delete setup_reqs[i];
    setup_reqs[i] = NULL;
  }
  setup_reqs.erase(std::remove_if(
    setup_reqs.begin(), setup_reqs.end(),
    [](auto x) {
        return !x;
    }), setup_reqs.end());
}

bool ZEN::clock(Cycle_t cycle){
  processZAPCredits();
  notifyHARTScratchpad();
  handleIncomingRZAMsg();
  processEgressQueue();
  processSetupMsgs();
  //printZenQueue();
  return false;
}

// EOF
