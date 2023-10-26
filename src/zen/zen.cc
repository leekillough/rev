//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"
#include "ZOPEvent.h"
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
  m_linkControl = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 1 );
  m_num_harts = params.find<uint64_t>("num_harts", 4);
  assert( m_linkControl );
  msg_id = 0;
  //mem_acks.push_back(0);
  //mem_acks.push_back(1);
  //mem_acks.push_back(2);
  //m_linkControl->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<ZEN>(this,&ZEN::handleNetworkEvent) );
  // register with SST
  registerAsPrimaryComponent();
}

ZEN::~ZEN(){
}

void ZEN::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "Init id %lu\n", int_id);
  m_linkControl->init(phase);
}

void ZEN::setup() {
  output.verbose(CALL_INFO, 1, 0, "Setup id %lu\n", int_id);
  m_linkControl->setup();
  m_zop_iface->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingZOP));

}

void ZEN::complete(unsigned int phase) {
    m_linkControl->complete(phase);
}

void ZEN::finish() {
  output.verbose(CALL_INFO, 1, 0, "Finish()\n");
  m_linkControl->finish();
}

void ZEN::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = dynamic_cast<SST::Forza::zopEvent*>(event);

  if (ev->getType() != (uint8_t)SST::Forza::zopMsgT::Z_MSG || ev->getType() != (uint8_t)SST::Forza::zopMsgT::Z_RZART) {
    output.verbose(CALL_INFO, 1, 0, "Invalid msg type\n");
    // TODO: Send NACK
    return;
  }
  if (ev->getType() == (uint8_t)SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == (uint64_t)SST::Forza::zopOpc::Z_SEND) {
    // read harts/zone
    zen_queue[ev->getPacket()[1]].push_back(new ZENEntry(ev, 0));
  } else if (ev->getType() == (uint8_t)SST::Forza::zopMsgT::Z_RZART) {
    mem_acks.push_back(ev); // TODO: Add getMsgId() to ZOPNet
  } else if (ev->getType() == (uint8_t)SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == (uint64_t)SST::Forza::zopOpc::Z_ZENSETUP) {
    setup_reqs.push_back(ev);
  } else if (ev->getType() == (uint8_t)SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == (uint64_t)SST::Forza::zopOpc::Z_CREDIT) {
    zap_credits.push_back(ev);
  }
}

void ZEN::sendMsgToRZA(uint64_t addr) {
  // TODO: Find RZA address
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), 1);
  std::vector<uint32_t> pkt;
  pkt.push_back(((uint32_t)SST::Forza::zopMsgT::Z_MZOP << 28) + (uint32_t)SST::Forza::zopOpc::Z_RZA_STORE);
  //pkt.push_back(1);
  pkt.push_back(addr);
  pkt.push_back(m_zop_iface->getAddress());
  rzaMsg->setPacket(pkt);
  m_zop_iface->send(rzaMsg, 1);
}

void ZEN::sendMsgToScratchpad(uint64_t dest, uint64_t addr) {
  // TODO: Find RZA address
  SST::Forza::zopEvent *zapMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), dest);
  std::vector<uint32_t> pkt;
  pkt.push_back(((uint32_t)SST::Forza::zopMsgT::Z_MZOP << 28) + (uint32_t)SST::Forza::zopOpc::Z_SCR_STORE);
  //pkt.push_back(1);
  pkt.push_back(addr);
  pkt.push_back(m_zop_iface->getAddress());
  zapMsg->setPacket(pkt);
  m_zop_iface->send(zapMsg, dest);
}

void ZEN::notifyHARTScratchpad() {
  for (int hart = 0; hart < m_num_harts; ++hart) {
    for (int i = 0; i < zen_queue.size(); ++i) {
      if (zen_queue[hart][i]->status == 2) {
        output.verbose(CALL_INFO, 1, 0, "notify scratch status %lu\n", zen_queue[hart][i]->status);
        // TODO: issue memory request to scratchpad by looking up dest and addr
        uint64_t cur_tail = hart_tables[hart]->scratch_cur_tail;
        if (hart_tables[hart]->scratch_tail > hart_tables[hart]->scratch_cur_tail) {
          hart_tables[hart]->scratch_cur_tail += 10; // TODO: Scratchpad size
        }
        sendMsgToScratchpad(zen_queue[hart][i]->msg->getPacket()[1], cur_tail);
        zen_queue[hart][i]->status = 3;
      }
    }
  }
  for (int hart = 0; hart < m_num_harts; ++hart ) {
    zen_queue[hart].erase(std::remove_if(
      zen_queue[hart].begin(), zen_queue[hart].end(),
      [](auto x) {
          return x->status >= 3;
      }), zen_queue[hart].end());
  }
}

void ZEN::handleIncomingRZAMsg() {
  // This is supposed to look at the RZA msgs and start progressing status of whatever
  // got an ACK
  for (int i = 0; i < mem_acks.size(); ++i) {
      if (outstanding_mem_req.count(mem_acks[i]->getPacket()[0])) {
        output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu\n", mem_acks[i]);
        uint64_t hart_id = outstanding_mem_req[mem_acks[i]->getPacket()[0]].first;
        uint64_t queue_loc = outstanding_mem_req[mem_acks[i]->getPacket()[0]].second;
        sendACKToZAP(hart_id);
        zen_queue[hart_id][queue_loc]->status = 2;
        mem_acks[i] = NULL;
        delete mem_acks[i];
      }
  }
  mem_acks.erase(std::remove_if(
    mem_acks.begin(), mem_acks.end(),
    [](auto x) {
        return !x;
    }), mem_acks.end());
}

void ZEN::sendNACKToZAP(uint64_t hart_id) {
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), hart_id);
  std::vector<uint32_t> pkt;
  pkt.push_back(((uint32_t)SST::Forza::zopMsgT::Z_MSG << 28) + (uint32_t)SST::Forza::zopOpc::Z_ZEN_NACK);
  pkt.push_back(hart_id);
  pkt.push_back(m_zop_iface->getAddress());
  nackMsg->setPacket(pkt);
  m_zop_iface->send(nackMsg, 1);

}

void ZEN::sendACKToZAP(uint64_t hart_id) {
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), 1);
  std::vector<uint32_t> pkt;
  pkt.push_back(((uint32_t)SST::Forza::zopMsgT::Z_MSG << 28) + (uint32_t)SST::Forza::zopOpc::Z_ZEN_ACK);
  pkt.push_back(hart_id);
  pkt.push_back(m_zop_iface->getAddress());
  ackMsg->setPacket(pkt);
  m_zop_iface->send(ackMsg, 1);
}

uint64_t ZEN::getRZATailQueue(uint64_t hart_id, uint64_t size) {
  // FIXME: Track if ahead of line blocking due to no clears
  // FIXME: Track if current tail and final tail space not enough to store
  uint64_t cur_tail = hart_tables[hart_id]->mem_cur_tail;
  uint64_t cur_head = hart_tables[hart_id]->mem_cur_head;
  if (cur_tail < cur_head && cur_head - cur_tail > size) {
    hart_tables[hart_id]->mem_cur_tail += size;
    return cur_tail;
  } else if (cur_tail > cur_head && cur_tail + size > hart_tables[hart_id]->mem_tail) {
    hart_tables[hart_id]->mem_cur_tail += size;
    // TODO: Wrap around
    return cur_tail;
  } else {
    // Not enough space
    return -1;
  }
}

void ZEN::processEgressQueue() {
  for (int harts = 0; harts < m_num_harts; ++harts) {
    for (int i = 0; i < zen_queue[harts].size(); ++i) {
      if (zen_queue[harts][i]->status == 0) {
        output.verbose(CALL_INFO, 1, 0, "progress status %lu\n", zen_queue[harts][i]->status);
        // issue memory request
        // TODO: lookup table to find addr
        uint64_t rza_addr = getRZATailQueue(harts, (uint64_t)zen_queue[harts][i]->msg->getPacket().size());
        if (rza_addr < 0) {
          sendNACKToZAP(harts);
          zen_queue[harts][i]->status = 5;
          return;
        }
        sendMsgToRZA(0);
        zen_queue[harts][i]->status = 1;
        outstanding_mem_req[msg_id++] = std::make_pair(harts, i);
      }
    }
  }
}

void ZEN::processSetupMsgs() {
  for (int i = 0; i < zap_credits.size(); ++i) {
    // TODO: Figure out HART id from this value
    // Top 3 bits are not used for dest
    uint64_t hart_id = zap_credits[i]->getPacket()[2] % (1 << 29);
    uint64_t credits = setup_reqs[i]->getPacket()[3];
    // TODO: Credits
    // hart_tables[hart_id]->credits += size;
    // TODO: Rounding
    hart_tables[hart_id]->mem_cur_tail -= credits;
    delete zap_credits[i];
    zap_credits[i] = NULL;
  }
  zap_credits.erase(std::remove_if(
    zap_credits.begin(), zap_credits.end(),
    [](auto x) {
        return !x;
    }), zap_credits.end());
}

void ZEN::processZAPCredits() {
  for (int i = 0; i < setup_reqs.size(); ++i) {
    // TODO: Figure out HART id from this value
    // Top 3 bits are not used for dest
    uint64_t hart_id = setup_reqs[i]->getPacket()[1] % (1 << 29);
    if (setup_reqs[i]->getPacket().size() < 8 || hart_tables.find(hart_id) == hart_tables.end()) {
      sendNACKToZAP(hart_id);
    }
    uint64_t mem_start_addr = setup_reqs[i]->getPacket()[4];
    uint64_t mem_end_addr = setup_reqs[i]->getPacket()[5];
    uint64_t size = setup_reqs[i]->getPacket()[6];
    uint64_t scratch_tail = setup_reqs[i]->getPacket()[7];
    uint64_t scratch_head = setup_reqs[i]->getPacket()[8];
    hart_tables[hart_id] = new ZENTableRow(mem_start_addr, mem_end_addr, size, false, scratch_tail, scratch_head, 500);
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
  notifyHARTScratchpad();
  handleIncomingRZAMsg();
  processEgressQueue();
  processZAPCredits();
  processSetupMsgs();
  return false;
}

// EOF
