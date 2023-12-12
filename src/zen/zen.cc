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
  dma_enabled = (bool)params.find<uint64_t>("dma_enabled", 0);
  zen_queue_size_limit = params.find<uint64_t>("zen_queue_size_limit", 100000);
  process_per_cycle = params.find<uint64_t>("zen_process_per_cycle", 100000);

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

uint64_t ZEN::findFirstUnsetBit(const std::bitset<256>& bv) {
    for (int i = 0; i < bv.size(); ++i) {
        if (!bv[i]) {
            return i;
        }
    }
    return (uint64_t)-1;
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
  bool from_zip = false;
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);
  output.verbose(CALL_INFO, 1, 0, "Msg type %lu, src %ld, dest %d src%lu\n", ev->getType(),
                 ev->getPacket()[1], ev->getDestHart(), ev->getSrcHart());
  if (ev->getType() != SST::Forza::zopMsgT::Z_MSG && ev->getType() != SST::Forza::zopMsgT::Z_RESP) {
    output.verbose(CALL_INFO, 1, 0, "Invalid msg type %d, expected %d or %d\n", ev->getType(),
                   SST::Forza::zopMsgT::Z_MSG, SST::Forza::zopMsgT::Z_RESP);
    if (m_zop_iface->getPCID(ev->getSrcPCID()) == zopPrecID::Z_ZIP) {
      sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID());
    } else {
      sendNACKToZIP(ev->getSrcHart(), ev->getSrcZCID());
    }
    return;
  }
  if (ev->getType() == SST::Forza::zopMsgT::Z_MSG &&
      (ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDP || ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDAS)) {
    if (m_zop_iface->getPCID(ev->getSrcPCID()) == zopPrecID::Z_ZIP) {
      from_zip = true;
    }
    if (zen_queue[ev->getDestHart()].size() < zen_queue_size_limit) {
      zen_queue[ev->getDestHart()].push_back(new ZENEntry(ev, ZENStatus::UNPROCESSED, from_zip));
    } else {
      output.verbose(CALL_INFO, 1, 0, "Destination %d queue full\n", ev->getDestHart(),
                    SST::Forza::zopMsgT::Z_MSG, SST::Forza::zopMsgT::Z_RESP);
      sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID());
      return;
    }
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
    mem_acks.push_back(ev); // TODO: Add getMsgId() to ZOPNet
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_ZENSET) {
    setup_reqs.push_back(ev);
  } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_CREDIT) {
    zap_credits.push_back(ev);
  }
}

void ZEN::sendMsgToRZA(uint64_t acs, uint64_t addr, std::vector<uint64_t> src_payload, uint8_t msg_id, uint64_t hart_id, uint64_t queue_loc) {
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %d, msg id %" PRIu8 "\n", addr, msg_id);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcPCID(m_zop_iface->getZoneID());
  rzaMsg->setSrcPrec(m_zop_iface->getZoneID());
  // Likely to be used later for extended msg ids
  rzaMsg->setDestHart(3);
  payload.push_back(acs);
  payload.push_back(addr);
  payload.push_back(src_payload.size());
  payload.insert(std::end(payload), std::begin(src_payload), std::end(src_payload));
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
  rzaMsg->decodeEvent();
  output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
}

void ZEN::sendHZOPToRZA(uint64_t acs, uint64_t addr, uint64_t src_addr, uint64_t size, uint8_t cur_msg_id, uint64_t hart_id, uint64_t queue_loc) {
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %d, msg id %" PRIu8 "\n", addr, cur_msg_id);

  // TODO: Fix HZOP format
  std::vector<uint64_t> payload;
  payload.push_back(acs);
  payload.push_back(src_addr);
  payload.push_back(addr);
  payload.push_back(size);
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(cur_msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcPCID(m_zop_iface->getZoneID());
  rzaMsg->setSrcPrec(m_zop_iface->getZoneID());
  // Likely to be used later for extended msg ids
  rzaMsg->setDestHart(3);
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
}

void ZEN::sendMsgToRZANonDMA(uint64_t acs, uint64_t addr, uint64_t src_payload, uint8_t cur_msg_id, uint64_t hart_id, uint64_t queue_loc) {
  output.verbose(CALL_INFO, 1, 0, "Msg tgt %d, msg id %" PRIu8 "\n", addr, cur_msg_id);

  // TODO: Update with RZA id
  std::vector<uint64_t> payload;
  payload.push_back(acs);
  payload.push_back(addr);
  payload.push_back(src_payload);
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(cur_msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcPCID(0);
  rzaMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  // TODO: Update with RZA id
  rzaMsg->setDestHart(3);
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  // TODO: Update with RZA id
}

void ZEN::sendMsgToScratchpad(uint64_t dest, uint64_t zcid, uint64_t scratch_addr, uint64_t size, uint64_t addr) {
  output.verbose(CALL_INFO, 1, 0, "hart %d, scratch a %d, size %d, addr %d\n", dest, scratch_addr, size, addr);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zapMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
  zapMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  zapMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SCSD);
  zapMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  zapMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  zapMsg->setSrcPCID(m_zop_iface->getZoneID());
  zapMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  zapMsg->setDestHart(dest);
  zapMsg->setDestZCID(zcid);
  zapMsg->setDestPCID(m_zop_iface->getZoneID());
  zapMsg->setDestPrec(m_zop_iface->getPrecinctID());
  // TODO: Push back ACS
  payload.push_back(scratch_addr);
  payload.push_back(addr);
  payload.push_back(size);
  zapMsg->setPayload(payload);
  zapMsg->encodeEvent();
  m_zop_iface->send(zapMsg, (zopCompID)dest);
}

void ZEN::notifyHARTScratchpad() {
  uint64_t cur_processed = 0;
 // output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad\n");
  uint64_t cur_tail = 0;
  for (int hart = 0; hart < m_num_harts; ++hart) {
    uint64_t next_tail = 0;
    bool tail_set = false;
    if (zen_queue[hart].size() == 0) {
      continue;
    }
    for (int i = 0; i < zen_queue[hart].size(); ++i) {
      //output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad id %d for msg payload %lu with status %d\n",
       //              hart, zen_queue[hart][i]->msg->getPayload()[0], zen_queue[hart][i]->status);
      std::vector<uint64_t> payload = zen_queue[hart][i]->msg->getPayload();
      if (zen_queue[hart][i]->status == ZENStatus::MZOP_ACK_PROCESSED) {
        cur_tail = zen_queue[hart][i]->tail;
        output.verbose(CALL_INFO, 1, 0, "set queue loc %lu to status %lu\n", i, 6);
        zen_queue[hart][i]->status = ZENStatus::DONE;
        if (!hart_tables[hart]) {
          output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad id %d fail\n", hart);
          break;
        } else {
          next_tail = zen_queue[hart][i]->tail;
          sendMsgToScratchpad(hart, zen_queue[hart][i]->msg->getDestZCID(), hart_tables[hart]->scratch_tail, payload.size(), next_tail);
          tail_set = true;
        }
      } else if (zen_queue[hart][i]->status < ZENStatus::MZOP_ACK_PROCESSED) {
        if (tail_set)output.verbose(CALL_INFO, 1, 0, "break! %d -> %lu\n", i, zen_queue[hart][i]->status );
        break;
      }
      cur_processed++;
      if (cur_processed > process_per_cycle) {
        break;
      }
    }
    uint64_t zq_sz = zen_queue[hart].size();
    zen_queue[hart].erase(std::remove_if(
      zen_queue[hart].begin(), zen_queue[hart].end(),
      [](auto x) {
          return x->status == ZENStatus::DONE;
      }), zen_queue[hart].end());

    if (zq_sz != zen_queue[hart].size()) {
      output.verbose(CALL_INFO, 1, 0, "zenq size %d\n", zen_queue[hart].size());
    }
  }
}

void ZEN::handleIncomingRZAMsg() {
  uint64_t cur_processed = 0;
  for (int i = 0; i < mem_acks.size(); ++i) {
    uint8_t inc_msg_id = mem_acks[i]->getID();
    output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu\n", inc_msg_id);
    if (outstanding_mem_req.count(inc_msg_id)) {
      uint64_t hart_id = outstanding_mem_req[inc_msg_id].first;
      uint64_t queue_loc = outstanding_mem_req[inc_msg_id].second;
      for (int k = 0; k < zen_queue[hart_id][queue_loc]->msg_ids.size(); ++k) {
        output.verbose(CALL_INFO, 1, 0, "%"PRIu8"\n", zen_queue[hart_id][queue_loc]->msg_ids[k]);
      }
      auto it = std::find(zen_queue[hart_id][queue_loc]->msg_ids.begin(), zen_queue[hart_id][queue_loc]->msg_ids.end(), inc_msg_id);
      if (it != zen_queue[hart_id][queue_loc]->msg_ids.end()) {
        zen_queue[hart_id][queue_loc]->msg_ids.erase(it);
        output.verbose(CALL_INFO, 1, 0, "progress status msg_id erased to size\n", zen_queue[hart_id][queue_loc]->msg_ids.size());
      } else {
        output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu not found\n", inc_msg_id);
        continue;
      }
      msg_id[inc_msg_id] = false;
      if (zen_queue[hart_id][queue_loc]->msg_ids.size() == 0) {
        output.verbose(CALL_INFO, 1, 0, "set queue loc %lu to status %lu\n", queue_loc, 2);
        zen_queue[hart_id][queue_loc]->status = ZENStatus::MZOP_ACK_PROCESSED;
        if (!zen_queue[hart_id][queue_loc]->from_zip) {
          sendACKToZAP(zen_queue[hart_id][queue_loc]->msg->getSrcHart(), zen_queue[hart_id][queue_loc]->msg->getSrcZCID());
        } else {
          sendACKToZIP(zen_queue[hart_id][queue_loc]->msg->getSrcHart(), zen_queue[hart_id][queue_loc]->msg->getSrcZCID());
        }
      }
      delete mem_acks[i];
      mem_acks[i] = NULL;
    } else {
      output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu not found\n", inc_msg_id);
    }
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
  }
  mem_acks.erase(std::remove_if(
    mem_acks.begin(), mem_acks.end(),
    [](auto x) {
        return !x;
    }), mem_acks.end());
}


void ZEN::sendNACKToZIP(uint64_t hart_id, uint64_t zcid) {
  std::vector<uint32_t> payload;
  // TODO: Update with precinct NW ZOP call
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent();
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  // This should not matter, we could use this later to augment msg_id
  nackMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  nackMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  nackMsg->setSrcPCID(m_zop_iface->getZoneID());
  nackMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  nackMsg->setDestHart(hart_id);
  nackMsg->setDestZCID(zcid);
  nackMsg->setDestPCID((uint8_t)zopPrecID::Z_ZIP);
  nackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  nackMsg->encodeEvent();
  //m_zop_iface->sendPrecinct(ackMsg, zopPrecID::Z_ZIP);
}

void ZEN::sendACKToZIP(uint64_t hart_id, uint64_t zcid) {
  std::vector<uint32_t> payload;
  // TODO: Update with precinct NW ZOP call
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent();
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart(9);
  ackMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  ackMsg->setSrcPCID(m_zop_iface->getZoneID());
  ackMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  ackMsg->setDestHart(hart_id);
  ackMsg->setDestZCID(zcid);
  ackMsg->setDestPCID((uint8_t)zopPrecID::Z_ZIP);
  ackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  ackMsg->setDestPrec(0);
  ackMsg->encodeEvent();
  //m_zop_iface->sendPrecinct(ackMsg, zopPrecID::Z_ZIP);
}

void ZEN::sendNACKToZAP(uint64_t hart_id, uint64_t zcid) {
  std::vector<uint32_t> payload;
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent();
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  // This should not matter, we could use this later to augment msg_id
  nackMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  nackMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  nackMsg->setSrcPCID(m_zop_iface->getZoneID());
  nackMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  nackMsg->setDestHart(hart_id);
  nackMsg->setDestZCID(zcid);
  nackMsg->setDestPCID(m_zop_iface->getZoneID());
  nackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  nackMsg->encodeEvent();
  m_zop_iface->send(nackMsg, (zopCompID)zcid);
}

void ZEN::sendACKToZAP(uint64_t hart_id, uint64_t zcid) {
  std::vector<uint32_t> payload;
  // TODO: Update with ZAP id
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent();
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  ackMsg->setSrcZCID((uint8_t)zopCompID::Z_ZEN);
  ackMsg->setSrcPCID(m_zop_iface->getZoneID());
  ackMsg->setSrcPrec(m_zop_iface->getPrecinctID());
  ackMsg->setDestHart(hart_id);
  ackMsg->setDestZCID(zcid);
  ackMsg->setDestPCID(m_zop_iface->getZoneID());
  ackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  ackMsg->setDestPrec(0);
  ackMsg->encodeEvent();
  m_zop_iface->send(ackMsg, (zopCompID)zcid);
}

int ZEN::getRZATailQueue(uint64_t hart_id, uint64_t size) {
  // FIXME: Track if ahead of line blocking due to no clears
  if (!hart_tables[hart_id]) { /* TODO: Raise exception */
    output.verbose(CALL_INFO, 1, 0, "no table entry for %d\n", hart_id);
    return 0;
  }
  uint64_t cur_tail = hart_tables[hart_id]->mem_cur_tail;
  uint64_t cur_head = hart_tables[hart_id]->mem_cur_head;
  output.verbose(CALL_INFO, 1, 0, "RZA[%d]: cur_head: %lu, cur_tail: %lu, size %lu\n", hart_id, cur_head, cur_tail, size);
  output.verbose(CALL_INFO, 1, 0, "RZA[%d]: mem_head: %lu, mem_tail %lu\n", hart_id, hart_tables[hart_id]->mem_head, hart_tables[hart_id]->mem_tail);
  if (cur_tail >= cur_head) {
    if (cur_tail == cur_head && !hart_tables[hart_id]->empty) {
      output.verbose(CALL_INFO, 1, 0, "RZA[%d]: GOT -1!\n", hart_id);
      return -1;
    }
    if (cur_tail + size - 1 > hart_tables[hart_id]->mem_tail) {
      // TODO: Write padding
      hart_tables[hart_id]->mem_cur_tail = hart_tables[hart_id]->mem_head + size;
      hart_tables[hart_id]->empty = false;
      return hart_tables[hart_id]->mem_head;
    } else {
      hart_tables[hart_id]->mem_cur_tail += size;
      hart_tables[hart_id]->empty = false;
      if (hart_tables[hart_id]->mem_cur_tail > hart_tables[hart_id]->mem_tail) {
        hart_tables[hart_id]->mem_cur_tail = hart_tables[hart_id]->mem_head;
      }
      return cur_tail;
    }
  } else {
    // Wraparound
    if (cur_tail + size - 1 > cur_head) {
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
  uint64_t cur_processed = 0;
  for (int harts = 0; harts < m_num_harts; ++harts) {
    for (int i = 0; i < zen_queue[harts].size(); ++i) {
      if (zen_queue[harts][i]->status == ZENStatus::UNPROCESSED) {
        output.verbose(CALL_INFO, 1, 0, "progress status %lu\n", zen_queue[harts][i]->status);
        int rza_addr = getRZATailQueue(harts, (uint64_t)zen_queue[harts][i]->msg->getPayload().size());
        if (rza_addr < 0) {
          if (!zen_queue[harts][i]->from_zip) {
            sendNACKToZAP(zen_queue[harts][i]->msg->getSrcHart(), zen_queue[harts][i]->msg->getSrcZCID());
          } else {
            sendNACKToZIP(zen_queue[harts][i]->msg->getSrcHart(), zen_queue[harts][i]->msg->getSrcZCID());
          }
          output.verbose(CALL_INFO, 1, 0, "set queue loc %lu to status %lu\n", i, 5);
          zen_queue[harts][i]->status = ZENStatus::RZA_ADDR_ERROR;
          return;
        }
        zen_queue[harts][i]->status = ZENStatus::RZA_ADDR_ASSIGNED;
        zen_queue[harts][i]->rza_start_addr = rza_addr;
      }
      zen_queue[harts].erase(std::remove_if(
        zen_queue[harts].begin(), zen_queue[harts].end(),
        [](auto x) {
            return x->status == ZENStatus::RZA_ADDR_ERROR;
        }), zen_queue[harts].end());
      }
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
  }
}

void ZEN::prepSendRZAHZOP() {
  // Send with address and size
  uint64_t cur_processed = 0;
  for (int harts = 0; harts < m_num_harts; ++harts) {
    for (int i = 0; i < zen_queue[harts].size(); ++i) {
      if (zen_queue[harts][i]->status == ZENStatus::RZA_ADDR_ASSIGNED
          && zen_queue[harts][i]->msg->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDAS) {
        bool success = true;
        uint64_t rza_addr = zen_queue[harts][i]->rza_start_addr;
        std::vector<uint64_t> payload = zen_queue[harts][i]->msg->getPayload();
        uint64_t next_msg_id = findFirstUnsetBit(msg_id);
        if (next_msg_id > 256) {
          break;
        }
        msg_id[next_msg_id] = true;
        sendHZOPToRZA(getWriteACS(hart_tables[harts]->acs_pair), rza_addr, payload[0], payload[1], next_msg_id,  harts, i);
        zen_queue[harts][i]->tail = rza_addr;
        outstanding_mem_req[next_msg_id] = std::make_pair(harts, i);
        zen_queue[harts][i]->msg_ids.push_back(next_msg_id);
        zen_queue[harts][i]->status = MZOP_SENT;
        output.verbose(CALL_INFO, 1, 0, "set queue loc %lu to status %lu\n", i, 1);
      }
    }
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
  }
}

void ZEN::prepSendRZAStore() {
  // Send with payload
  uint64_t cur_processed = 0;
  for (int harts = 0; harts < m_num_harts; ++harts) {
    for (int i = 0; i < zen_queue[harts].size(); ++i) {
      if (zen_queue[harts][i]->status == ZENStatus::RZA_ADDR_ASSIGNED
          && zen_queue[harts][i]->msg->getOpcode() == SST::Forza::zopOpc::Z_MSG_SENDP) {
        bool success = true;
        uint64_t rza_addr = zen_queue[harts][i]->rza_start_addr;
        std::vector<uint64_t> payload = zen_queue[harts][i]->msg->getPayload();
        if (dma_enabled == 1) {
          // TODO: Fix msg id

          uint64_t next_msg_id = findFirstUnsetBit(msg_id);
          if (next_msg_id > 256) {
            break;
          }
          msg_id[next_msg_id] = true;
          sendMsgToRZA(getWriteACS(hart_tables[harts]->acs_pair), rza_addr, payload, next_msg_id, harts, i);
          zen_queue[harts][i]->tail = rza_addr;
          outstanding_mem_req[next_msg_id] = std::make_pair(harts, i);
          zen_queue[harts][i]->msg_ids.push_back(next_msg_id);
          zen_queue[harts][i]->status = MZOP_SENT;
        } else {
          std::vector<uint8_t> avail_msg_id;
          for (int i = 0; i < payload.size(); ++i) {
            uint64_t next_msg_id = findFirstUnsetBit(msg_id);
            output.verbose(CALL_INFO, 1, 0, "Free msg_id %lu\n", next_msg_id);
            if (next_msg_id > 256) {
              for (int j = 0; j < avail_msg_id.size(); ++j) {
                msg_id[avail_msg_id[j]] = false;
              }
              success = false;
              break;
            }
            msg_id[next_msg_id] = true;
            avail_msg_id.push_back(static_cast<uint8_t>(next_msg_id));
          }
          if (success) {
            for (int i = 0; i < payload.size(); ++i) {
              sendMsgToRZANonDMA(getWriteACS(hart_tables[harts]->acs_pair), rza_addr, payload[i], avail_msg_id[i],  harts, i);
              zen_queue[harts][i]->tail = rza_addr;
              outstanding_mem_req[avail_msg_id[i]] = std::make_pair(harts, i);
              zen_queue[harts][i]->msg_ids.push_back(avail_msg_id[i]);
              rza_addr += 1; // This assumes address at 64 byte offsets, multiply by 64
              if (rza_addr > hart_tables[harts]->mem_tail) {
                rza_addr = hart_tables[harts]->mem_head;
              }
            }
            zen_queue[harts][i]->status = MZOP_SENT;
          }
        }
        if (!success) {
          continue;
        }
        output.verbose(CALL_INFO, 1, 0, "set queue loc %lu to status %lu\n", i, 1);
      }
    }
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
  }
}

void ZEN::processZAPCredits() {
  uint64_t cur_processed = 0;
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
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
  }
  zap_credits.erase(std::remove_if(
    zap_credits.begin(), zap_credits.end(),
    [](auto x) {
        return !x;
    }), zap_credits.end());
}

void ZEN::processSetupMsgs() {
  uint64_t cur_processed = 0;
  //output.verbose(CALL_INFO, 1, 0, "Progress setup msg\n");
  for (int i = 0; i < setup_reqs.size(); ++i) {
    std::vector<uint64_t> payload = setup_reqs[i]->getPayload();
    uint64_t hart_id = setup_reqs[i]->getSrcHart();
    output.verbose(CALL_INFO, 1, 0, "setup pkt size %lu for hart %lu\n", payload.size(), hart_id);
    if (setup_reqs[i]->getPayload().size() < 4 || hart_tables.find(hart_id) != hart_tables.end()) {
      // From ZIP setup is assumed to be validated at ZIP
      sendNACKToZAP(setup_reqs[i]->getSrcHart(), setup_reqs[i]->getSrcZCID());
    }
    // TODO: Specify payload format
    uint64_t acs_pair = payload[0];
    uint64_t mem_start_addr = payload[1];
    uint64_t size = payload[3];
    uint64_t mem_end_addr = mem_start_addr + size - 1;
    uint64_t scratch_tail = payload[4];
    hart_tables[hart_id] = new ZENTableRow(acs_pair, mem_start_addr, mem_end_addr, size, scratch_tail, 500);
    //sendACKToZAP(hart_id);
    output.verbose(CALL_INFO, 1, 0, "setup hart table %d, start addr %lu, end addr %lu\n", hart_id, mem_start_addr, mem_end_addr);
    output.verbose(CALL_INFO, 1, 0, "setup hart table %d, start addr %lu, end addr %lu\n", hart_id, hart_tables[hart_id]->mem_head, hart_tables[hart_id]->mem_tail);
    delete setup_reqs[i];
    setup_reqs[i] = NULL;
    cur_processed++;
    if (cur_processed > process_per_cycle) {
      break;
    }
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
  prepSendRZAHZOP();
  prepSendRZAStore();
  processEgressQueue();
  processSetupMsgs();
  //printZenQueue();
  return false;
}

// EOF
