//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"

#define DW_OFFSET 8

namespace SST::Forza{

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id),
    zip_credits(_ZEN_DEFAULT_ZIP_CREDITS_),
    msg_id(0) {

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 7);
  output.init("ZEN[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
  Precinct = params.find<unsigned>("precinctId", 0);
  Zone = params.find<unsigned>("zoneId", 0);
  m_num_harts = params.find<unsigned>("numHarts", 512);
  m_num_zaps = params.find<unsigned>("numZaps", 4);
  m_num_zones = params.find<unsigned>("numZones", 8);
  m_num_precincts = params.find<unsigned>("numPrecincts", 4);
  dma_enabled = params.find<bool>("enableDMA", false);
  zen_queue_size_limit = params.find<uint64_t>("zenQSizeLimit", 100000);
  process_per_cycle = params.find<uint64_t>("processPerCycle", 100000);
  precinct_nic_enabled = params.find<bool>("enablePrecinctNIC", true);

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZEN>(this, &ZEN::clock));
  output.output("ZEN[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  // setup the zone network
  m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
  m_zop_iface->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingZOP));
  m_zop_iface->setEndpointType(zopCompID::Z_ZEN);
  m_zop_iface->setNumHarts(m_num_harts);
  m_zop_iface->setPrecinctID(Precinct);
  m_zop_iface->setZoneID(Zone);

  // setup the precinct network
  if (precinct_nic_enabled) {
    m_prec_iface = loadUserSubComponent<SST::Forza::zopAPI>( "precinct_nic" );
    m_prec_iface->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingPrecZOP));
    m_prec_iface->setEndpointType(zopCompID::Z_ZEN);
    m_prec_iface->setNumHarts(m_num_harts);
    m_prec_iface->setPrecinctID(Precinct);
    m_prec_iface->setZoneID(Zone);
  }

  // complete SST registration
  registerAsPrimaryComponent();
}

ZEN::~ZEN(){
}

bool ZEN::findFirstUnsetBit(const std::bitset<256>& bv, uint8_t *bit) {
  for (unsigned i = 0; i < bv.size(); ++i) {
    if (!bv[i]) {
      *bit = i;
      return true;
    }
  }
  return false;
}

uint64_t ZEN::getWriteACS(uint64_t acs_pair) {
  return acs_pair & Z_ACS_WRITE;
}

uint64_t ZEN::getReadACS(uint64_t acs_pair) {
  return (acs_pair & Z_ACS_READ) >> 32;
}

void ZEN::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "ZEN ID %d\n", Zone);
  m_zop_iface->init(phase);
  if (precinct_nic_enabled) {
    m_prec_iface->init(phase);
  }
}

void ZEN::setup() {
  m_zop_iface->setup();
  if (precinct_nic_enabled) {
    m_prec_iface->setup();
  }
}

void ZEN::complete(unsigned int phase) {
  m_zop_iface->complete(phase);
  if (precinct_nic_enabled) {
    m_prec_iface->complete(phase);
  }
}

void ZEN::finish() {
  output.verbose(CALL_INFO, 10, 0, "Finish()\n");
}

void ZEN::handleIncomingPrecZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 9, 0, "[PRECINCT]: %s received msg type %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                 getName().c_str(),
                 m_zop_iface->msgTToStr(ev->getType()).c_str(),
                 ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                 m_zop_iface->endPToStr(m_zop_iface->getEndpointType()).c_str());

  if( (zopCompID)(ev->getSrcZCID()) == zopCompID::Z_PREC_ZIP ){
    if( (ev->getType() == SST::Forza::zopMsgT::Z_MSG) &&
        (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_CREDIT)){
      zip_credits += ev->getCredit();
    }
  }else if( ev->getSrcPrec() != m_prec_iface->getPrecinctID() ){
    zipQ.push(ev);
  }else{
    if(ev->getType() == SST::Forza::zopMsgT::Z_MSG &&
       ((ev->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDP) ||
        (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDAS))) {
      // incoming SEND from outside the zone
      std::pair<uint64_t, uint64_t> hart_zap_id =
        std::make_pair(ev->getDestZCID(), ev->getDestHart());
      if( zen_queue[hart_zap_id].size() < zen_queue_size_limit ){
        zen_queue[hart_zap_id].push_back(new ZENEntry(ev,
                                                      ZENStatus::UNPROCESSED,
                                                      true));
      } else {
        output.verbose(CALL_INFO, 7, 0,
                       "Destination %d queue full\n",
                       ev->getDestHart());
        // sendNACKToZIP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
        return;
      }
    }
  }
}

void ZEN::handleIncomingZOP(SST::Event *event) {
  bool from_zip = false;
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 9, 0, "[ZONE]: %s received msg type %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                 getName().c_str(),
                 m_zop_iface->msgTToStr(ev->getType()).c_str(),
                 ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                 m_zop_iface->endPToStr(m_zop_iface->getEndpointType()).c_str());

  if( (ev->getType() != SST::Forza::zopMsgT::Z_MSG) &&
      (ev->getType() != SST::Forza::zopMsgT::Z_RESP) ){
    output.verbose(CALL_INFO, 9, 0, "Invalid msg type %s, expected %s or %s\n",
                   m_zop_iface->msgTToStr(ev->getType()).c_str(),
                   m_zop_iface->msgTToStr(SST::Forza::zopMsgT::Z_MSG).c_str(),
                   m_zop_iface->msgTToStr(SST::Forza::zopMsgT::Z_RESP).c_str());
    sendNACK(ev->getSrcHart(),
             ev->getSrcZCID(),
             ev->getSrcPCID(),
             ev->getSrcPrec(),
             ev->getID(),
             m_zop_iface);
#if 0
    if (m_zop_iface->getPCID(ev->getSrcPCID()) == zopPrecID::Z_ZIP) {
      sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
    } else {
      // sendNACKToZIP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
    }
#endif
    return;
  }

  if( ev->getType() == SST::Forza::zopMsgT::Z_MSG &&
      (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDP ||
       ev->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDAS) ){
    if( (ev->getDestPCID() != m_zop_iface->getZoneID()) &&
        (ev->getDestPrec() == m_zop_iface->getPrecinctID()) ){
      if( zone_queue[ev->getDestPCID()].size() < zen_queue_size_limit ){
        zone_queue[ev->getDestPCID()].push_back(new ZENEntry(ev,
                                                             ZENStatus::UNPROCESSED,
                                                             false));
      }else{
        output.verbose(CALL_INFO, 1, 0, "Destination %d queue full\n", ev->getDestHart());
        sendNACK(ev->getSrcHart(),
                 ev->getSrcZCID(),
                 ev->getSrcPCID(),
                 ev->getSrcPrec(),
                 ev->getID(),
                 m_zop_iface);
        //sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
        return;
      }
    }else if( ev->getDestPrec() != m_zop_iface->getPrecinctID() ){
      if( precinct_queue[ev->getDestPCID()].size() < zen_queue_size_limit ){
        precinct_queue[ev->getDestPCID()].push_back(new ZENEntry(ev,
                                                                 ZENStatus::UNPROCESSED,
                                                                 false));
      }else{
        output.verbose(CALL_INFO, 1, 0, "Destination %d queue full\n", ev->getDestHart());
        sendNACK(ev->getSrcHart(),
                 ev->getSrcZCID(),
                 ev->getSrcPCID(),
                 ev->getSrcPrec(),
                 ev->getID(),
                 m_zop_iface);
        //sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
        return;
      }
    }else{
      if (m_zop_iface->getPCID(ev->getSrcPCID()) == zopPrecID::Z_ZIP) {
        from_zip = true;
      }

      std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(ev->getDestZCID(),
                                                                 ev->getDestHart());
      if( zen_queue[hart_zap_id].size() < zen_queue_size_limit){
        zen_queue[hart_zap_id].push_back(new ZENEntry(ev,
                                                      ZENStatus::UNPROCESSED,
                                                      from_zip));
      }else{
        output.verbose(CALL_INFO, 1, 0, "Destination %d queue full\n", ev->getDestHart());
        sendNACK(ev->getSrcHart(),
                 ev->getSrcZCID(),
                 ev->getSrcPCID(),
                 ev->getSrcPrec(),
                 ev->getID(),
                 m_zop_iface);
        //sendNACKToZAP(ev->getSrcHart(), ev->getSrcZCID(), ev->getID());
        return;
      }
    }
  }else if (ev->getType() == SST::Forza::zopMsgT::Z_RESP){
    mem_acks.push_back(ev);
  }else if((ev->getType() == SST::Forza::zopMsgT::Z_MSG) &&
           (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_ZENSET)){
    setup_reqs.push_back(ev);
  }else if((ev->getType() == SST::Forza::zopMsgT::Z_MSG) &&
           (ev->getOpc() == SST::Forza::zopOpc::Z_MSG_CREDIT)){
    zap_credits.push_back(ev);
  }
}

void ZEN::sendMsgToRZADMA(uint64_t acs, uint64_t addr,
                          std::vector<uint64_t> src_payload,
                          uint8_t cur_msg_id, uint64_t hart_id,
                          uint64_t queue_loc) {
  output.verbose(CALL_INFO, 9, 0, "Msg tgt %" PRIu64 ", msg id %" PRIu8 "\n",
                 addr, cur_msg_id);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(cur_msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SDMA);
  rzaMsg->setSrcHart((uint16_t)(zopCompID::Z_ZEN));
  rzaMsg->setSrcZCID((uint8_t)(m_zop_iface->getEndpointType()));
  rzaMsg->setSrcPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  rzaMsg->setSrcPrec((uint8_t)(m_zop_iface->getPrecinctID()));
  rzaMsg->setDestHart(Z_MZOP_PIPE_HART);
  rzaMsg->setDestZCID((uint8_t)(SST::Forza::zopCompID::Z_RZA));
  rzaMsg->setDestPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  rzaMsg->setDestPrec((uint8_t)(m_zop_iface->getPrecinctID()));

  payload.push_back(acs);
  payload.push_back(addr);
  payload.insert(std::end(payload),
                 std::begin(src_payload),
                 std::end(src_payload));
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
}

void ZEN::sendHZOPToRZA(uint64_t acs, uint64_t addr, uint64_t src_addr,
                        uint64_t size, uint8_t cur_msg_id, uint64_t hart_id,
                        uint64_t queue_loc) {
  // -------------------------------
  // FIXME
  // This method is 100% wrong
  // !!! Make this an LDMA request
  // -------------------------------
  output.fatal(CALL_INFO, -1, "This operation is badly broken.  Enjoy!");
  output.verbose(CALL_INFO, 9, 0, "Msg tgt %" PRIu64 ", msg id %" PRIu8 "\n", addr, cur_msg_id);

  // TODO: Fix HZOP format
  std::vector<uint64_t> payload;
  payload.push_back(acs);
  payload.push_back(src_addr);
  payload.push_back(addr);
  payload.push_back(size);
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_HZOPV);
  rzaMsg->setID(cur_msg_id);
  // TODO: Update with HZOP for memcpy
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  rzaMsg->setSrcZCID((uint8_t)m_zop_iface->getEndpointType());
  rzaMsg->setSrcPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));
  rzaMsg->setSrcPrec((uint8_t)m_zop_iface->getPrecinctID());
  rzaMsg->setDestHart(Z_MZOP_PIPE_HART);
  rzaMsg->setDestZCID((uint8_t)(SST::Forza::zopCompID::Z_RZA));
  rzaMsg->setDestPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  rzaMsg->setDestPrec((uint8_t)(m_zop_iface->getPrecinctID()));
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();

  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
}

void ZEN::sendMsgToRZANonDMA(uint64_t acs, uint64_t addr, uint64_t src_payload,
                             uint8_t cur_msg_id, uint64_t hart_id,
                             uint64_t queue_loc) {
  output.verbose(CALL_INFO, 9, 0,
                 "Msg tgt %" PRIu64 ", msg id %" PRIu8 "\n", addr, cur_msg_id);

  std::vector<uint64_t> payload;
  payload.push_back(acs);
  payload.push_back(addr);
  payload.push_back(src_payload);
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(cur_msg_id);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
  rzaMsg->setSrcHart((uint16_t)(zopCompID::Z_ZEN));
  rzaMsg->setSrcZCID((uint8_t)(m_zop_iface->getEndpointType()));
  rzaMsg->setSrcPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  rzaMsg->setSrcPrec((uint8_t)(m_zop_iface->getPrecinctID()));
  rzaMsg->setDestHart(Z_MZOP_PIPE_HART);
  rzaMsg->setDestZCID((uint8_t)(SST::Forza::zopCompID::Z_RZA));
  rzaMsg->setDestPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  rzaMsg->setDestPrec((uint8_t)(m_zop_iface->getPrecinctID()));
  rzaMsg->setPayload(payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
}

void ZEN::sendMsgToScratchpad(uint64_t dest, uint64_t zcid,
                              uint64_t scratch_addr, uint64_t size,
                              uint64_t addr){
  output.verbose(CALL_INFO, 9, 0,
                 "hart %" PRIu64 ", scratch a %" PRIu64 ", size %" PRIu64 ", addr %" PRIu64 "\n",
                 dest, scratch_addr, size, addr);
  std::vector<uint64_t> payload;
  SST::Forza::zopEvent *zapMsg = new SST::Forza::zopEvent();
  zapMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  zapMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SCSD);
  zapMsg->setSrcHart((uint16_t)(zopCompID::Z_ZEN));
  zapMsg->setSrcZCID((uint8_t)(m_zop_iface->getEndpointType()));
  zapMsg->setSrcPCID((uint8_t)(m_zop_iface->getPCID(m_zop_iface->getZoneID())));
  zapMsg->setSrcPrec((uint8_t)(m_zop_iface->getPrecinctID()));
  zapMsg->setDestHart(dest);
  zapMsg->setDestZCID(zcid);
  zapMsg->setDestPCID(m_zop_iface->getZoneID());
  zapMsg->setDestPrec(m_zop_iface->getPrecinctID());

  payload.push_back(0x00);        // ACS: FIXME
  payload.push_back(scratch_addr);// target address
  payload.push_back(addr);        // address is the data?

  zapMsg->setPayload(payload);
  zapMsg->encodeEvent();
  m_zop_iface->send(zapMsg, (zopCompID)(zcid));
  output.verbose(CALL_INFO, 9, 0,
                 "Progress HART scratchpad opcode %" PRIu8 "\n",
                 (uint8_t)(zapMsg->getOpc()));
}

void ZEN::forwardPktToZIP(Forza::zopEvent *ev) {
  ev->setSrcPCID(m_zop_iface->getZoneID());
  ev->encodeEvent();
  m_prec_iface->send(ev, zopCompID::Z_PREC_ZIP,
                     zopPrecID::Z_ZIP, m_prec_iface->getPrecinctID());
  zip_credits -= (ev->getLength()+Z_NUM_HEADER_FLITS);
}

void ZEN::forwardPktToExtZEN(Forza::zopEvent *ev) {
  ev->setSrcPCID(m_zop_iface->getZoneID());
  ev->encodeEvent();
  m_prec_iface->send(ev, zopCompID::Z_ZEN,
                     (zopPrecID)(ev->getDestPCID()),
                     m_prec_iface->getPrecinctID());
}

void ZEN::notifyHARTScratchpad() {
  uint64_t cur_processed = 0;
 // output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad\n");

  for (unsigned zap_id = 0; zap_id < m_num_zaps; ++zap_id) {
    for (unsigned hart = 0; hart < m_num_harts; ++hart) {
      uint64_t next_tail = 0;
      std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, hart);
      bool tail_set = false;
      if (zen_queue[hart_zap_id].size() == 0) {
        continue;
      }
      for (unsigned i = 0; i < zen_queue[hart_zap_id].size(); ++i) {
        //output.verbose(CALL_INFO, 1, 0, "Progress HART scratchpad id %d for msg payload %llu with status %d\n",
        //              hart, zen_queue[hart_zap_id][i]->msg->getPayload()[0], zen_queue[hart_zap_id][i]->status);
        std::vector<uint64_t> payload = zen_queue[hart_zap_id][i]->msg->getPayload();
        if( zen_queue[hart_zap_id][i]->status ==
            ZENStatus::MZOP_ACK_PROCESSED ){
          zen_queue[hart_zap_id][i]->status = ZENStatus::DONE;
          if( !hart_tables[hart_zap_id] ){
            output.verbose(CALL_INFO, 9, 0,
                           "Progress HART scratchpad id %u fail\n", hart);
            break;
          }else{
            next_tail = zen_queue[hart_zap_id][i]->tail;
            sendMsgToScratchpad(hart,
                                zen_queue[hart_zap_id][i]->msg->getDestZCID(),
                                hart_tables[hart_zap_id]->scratch_tail,
                                payload.size(),
                                next_tail);
            tail_set = true;
          }
        }else if( zen_queue[hart_zap_id][i]->status <
                  ZENStatus::MZOP_ACK_PROCESSED ){
          if( tail_set ){
            output.verbose(CALL_INFO, 9, 0,
                                      "break! %u -> %d\n", i,
                                      zen_queue[hart_zap_id][i]->status );
          }
          break;
        }
        cur_processed++;
        if( cur_processed > process_per_cycle ){
          break;
        }
      }
      uint64_t zq_sz = (uint64_t)(zen_queue[hart_zap_id].size());
      zen_queue[hart_zap_id].erase(std::remove_if(
        zen_queue[hart_zap_id].begin(), zen_queue[hart_zap_id].end(),
        [](auto x) {
            return x->status == ZENStatus::DONE;
        }), zen_queue[hart_zap_id].end());

      if( zq_sz != zen_queue[hart_zap_id].size() ){
        output.verbose(CALL_INFO, 9, 0, "zenq size %zu\n",
                       zen_queue[hart_zap_id].size());
      }
    }
  }
}

void ZEN::handleIncomingRZAMsg() {
  uint64_t cur_processed = 0;
  for (unsigned i = 0; i < mem_acks.size(); ++i) {
    uint8_t inc_msg_id = mem_acks.at(i)->getID();
    //output.verbose(CALL_INFO, 1, 0, "progress status msg_id %llu\n", inc_msg_id);
    if( outstanding_mem_req.count(inc_msg_id) ){
      //uint64_t hart_id = outstanding_mem_req[inc_msg_id].first;
      //uint64_t queue_loc = outstanding_mem_req[inc_msg_id].second;
      /*for (int k = 0; k < zen_queue[hart_zap_id][queue_loc]->msg_ids.size(); ++k) {
        output.verbose(CALL_INFO, 1, 0, "%"PRIu8"\n", zen_queue[hart_zap_id][queue_loc]->msg_ids[k]);
      }*/
      auto zqloc = outstanding_mem_req[inc_msg_id];
      auto it = std::find(zqloc->msg_ids.begin(),
                          zqloc->msg_ids.end(),
                          inc_msg_id);
      if( it != zqloc->msg_ids.end() ){
        zqloc->msg_ids.erase(it);
        output.verbose(CALL_INFO, 9, 0,
                       "progress status msg_id erased to size %zu\n",
                       zqloc->msg_ids.size());
      }else{
        // TODO: Why wouldn't this be an error?
        continue;
      }

      msg_id[inc_msg_id] = false;
      outstanding_mem_req.erase(inc_msg_id);
      if( zqloc->msg_ids.size() == 0 ){
        //output.verbose(CALL_INFO, 1, 0, "set queue loc %llu to status %llu\n", queue_loc, 2);
        zqloc->status = ZENStatus::MZOP_ACK_PROCESSED;
        if( !zqloc->from_zip ){
          output.verbose(CALL_INFO, 9, 0, "Send ACK to ZCID, HARTID %d %d\n",
                         zqloc->msg->getSrcHart(), zqloc->msg->getSrcZCID());
          //sendACKToZAP(zqloc->msg->getSrcHart(), zqloc->msg->getSrcZCID(), zqloc->msg->getID());
          sendACK(zqloc->msg->getSrcHart(),
                   zqloc->msg->getSrcZCID(),
                   zqloc->msg->getSrcPCID(),
                   zqloc->msg->getSrcPrec(),
                   zqloc->msg->getID(),
                   m_zop_iface);
        } else {
          // sendACKToZIP(zqloc->msg->getSrcHart(), zqloc->msg->getSrcZCID(), zqloc->msg->getID());
        }
      }
      delete mem_acks[i];
      mem_acks[i] = NULL;
    }else{
      // TODO: This should probably be a fatal - unexpected
      output.verbose(CALL_INFO, 9, 0,
                     "progress status msg_id %d not found\n",
                     inc_msg_id);
    }
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
  mem_acks.erase(std::remove_if(
    mem_acks.begin(), mem_acks.end(),
    [](auto x) {
        return !x;
    }), mem_acks.end());
}


void ZEN::sendACK(uint16_t hart, uint8_t zcid,
                  uint8_t pcid, uint16_t prec, uint8_t id,
                  SST::Forza::zopAPI *iface){
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent();
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart((uint16_t)(zopCompID::Z_ZEN));
  ackMsg->setSrcZCID((uint8_t)(iface->getEndpointType()));
  ackMsg->setSrcPCID((uint8_t)(iface->getPCID(m_zop_iface->getZoneID())));
  ackMsg->setSrcPrec((uint8_t)(iface->getPrecinctID()));
  ackMsg->setDestHart(hart);
  ackMsg->setDestZCID(zcid);
  ackMsg->setDestPCID(pcid);
  ackMsg->setID(id);
  ackMsg->encodeEvent();
  iface->send(ackMsg,
              iface->getZCID(zcid, false),  //this assumes the src was not an RZA
              iface->getPCID(pcid),
              prec);
}

void ZEN::sendNACK(uint16_t hart, uint8_t zcid,
                   uint8_t pcid, uint16_t prec, uint8_t id,
                   SST::Forza::zopAPI *iface){
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent();
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  nackMsg->setSrcHart((uint16_t)(zopCompID::Z_ZEN));
  nackMsg->setSrcZCID((uint8_t)(iface->getEndpointType()));
  nackMsg->setSrcPCID((uint8_t)(iface->getPCID(m_zop_iface->getZoneID())));
  nackMsg->setSrcPrec((uint8_t)(iface->getPrecinctID()));
  nackMsg->setDestHart(hart);
  nackMsg->setDestZCID(zcid);
  nackMsg->setDestPCID(pcid);
  nackMsg->setID(id);
  nackMsg->encodeEvent();
  iface->send(nackMsg,
              iface->getZCID(zcid, false),  //this assumes the src was not an RZA
              iface->getPCID(pcid),
              prec);
}

#if 0
void ZEN::sendNACKToZIP(uint64_t hart_id, uint64_t zcid, uint8_t cur_msg_id) {
  std::vector<uint32_t> payload;
  // TODO: Update with precinct NW ZOP call
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent();
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  // This should not matter, we could use this later to augment msg_id
  nackMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);
  nackMsg->setSrcZCID((uint8_t)m_zop_iface->getEndpointType());  //FIXME
  nackMsg->setSrcPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));   //FIXME
  nackMsg->setSrcPrec((uint8_t)m_zop_iface->getPrecinctID());
  nackMsg->setDestHart(hart_id);
  nackMsg->setDestZCID(zcid);
  nackMsg->setDestPCID((uint8_t)zopPrecID::Z_ZIP);
  nackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  nackMsg->setID(cur_msg_id);
  nackMsg->encodeEvent();
  m_prec_iface->send(nackMsg, zopCompID::Z_PREC_ZIP, zopPrecID::Z_ZIP, m_prec_iface->getPrecinctID());
}

void ZEN::sendACKToZIP(uint64_t hart_id, uint64_t zcid, uint8_t cur_msg_id) {
  std::vector<uint32_t> payload;
  // TODO: Update with precinct NW ZOP call
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent();
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart((uint16_t)zopCompID::Z_ZEN);    //FIXME
  ackMsg->setSrcZCID((uint8_t)m_zop_iface->getEndpointType());  //FIXME
  ackMsg->setSrcPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));   //FIXME
  ackMsg->setSrcPrec((uint8_t)m_zop_iface->getPrecinctID());
  ackMsg->setDestHart(hart_id);
  ackMsg->setDestZCID(zcid);
  ackMsg->setDestPCID((uint8_t)zopPrecID::Z_ZIP);
  ackMsg->setDestPrec(m_zop_iface->getPrecinctID());
  ackMsg->setID(cur_msg_id);
  ackMsg->encodeEvent();
  m_prec_iface->send(ackMsg, zopCompID::Z_PREC_ZIP, zopPrecID::Z_ZIP, m_prec_iface->getPrecinctID());
}

void ZEN::sendNACKToZAP(uint64_t hart_id, uint64_t zcid, uint8_t cur_msg_id) {
  std::vector<uint32_t> payload;
  SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent();
  nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
  // Match source and destination hart id
  nackMsg->setSrcHart(hart_id);    //FIXME: all of these!!
  nackMsg->setSrcZCID((uint8_t)m_zop_iface->getEndpointType());  //FIXME
  nackMsg->setSrcPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));   //FIXME
  nackMsg->setSrcPrec((uint8_t)m_zop_iface->getPrecinctID());
  nackMsg->setDestHart(hart_id);
  nackMsg->setDestZCID(zcid);
  nackMsg->setDestPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));
  nackMsg->setDestPrec((uint8_t)m_zop_iface->getPrecinctID());
  nackMsg->setID(cur_msg_id);
  nackMsg->encodeEvent();
  m_zop_iface->send(nackMsg, (zopCompID)zcid);
}

void ZEN::sendACKToZAP(uint64_t hart_id, uint64_t zcid, uint8_t cur_msg_id) {
  std::vector<uint32_t> payload;
  // TODO: Update with ZAP id
  SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent();
  ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
  ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  ackMsg->setSrcHart(hart_id);
  ackMsg->setSrcZCID((uint8_t)m_zop_iface->getEndpointType());  //FIXME
  ackMsg->setSrcPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));   //FIXME
  ackMsg->setSrcPrec((uint8_t)m_zop_iface->getPrecinctID());
  ackMsg->setDestHart(hart_id);
  ackMsg->setDestZCID(zcid);
  ackMsg->setDestPCID((uint8_t)m_zop_iface->getPCID(m_zop_iface->getZoneID()));
  ackMsg->setDestPrec((uint8_t)m_zop_iface->getPrecinctID());
  ackMsg->setDestPrec(0);
  ackMsg->setID(cur_msg_id);
  ackMsg->encodeEvent();
  m_zop_iface->send(ackMsg, (zopCompID)zcid);
}
#endif

int ZEN::getRZATailQueue(uint64_t zap_id,
                         uint64_t hart_id,
                         uint64_t size,
                         uint64_t *taddr ){
  std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, hart_id);
  output.verbose(CALL_INFO, 9, 0, "hart_zap_id: [%" PRIu64 ", %" PRIu64 "]\n",
                 hart_zap_id.first, hart_zap_id.second);
  if (!hart_tables[hart_zap_id]) { /* TODO: Raise exception */
    output.fatal(CALL_INFO, -1,
                 "Error: could not find table entry for hart=%" PRIu64 "\n",
                 hart_id);
    return 0; // should never reach this
  }
  uint64_t cur_tail = hart_tables[hart_zap_id]->mem_cur_tail;
  uint64_t cur_head = hart_tables[hart_zap_id]->mem_cur_head;
  output.verbose(CALL_INFO, 9, 0,
                 "RZA[%" PRIu64 "]: cur_head: %" PRIu64 ", cur_tail: %" PRIu64 ", size %" PRIu64 "\n",
                 hart_id, cur_head, cur_tail, size);
  output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                 hart_id, hart_tables[hart_zap_id]->mem_head,
                 hart_tables[hart_zap_id]->mem_tail);
  if( cur_tail >= cur_head ){
    if( (cur_tail == cur_head) && (!hart_tables[hart_zap_id]->empty) ){
      output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: GOT -1!\n", hart_id);
      return -1;
    }

    if (cur_tail + size > hart_tables[hart_zap_id]->mem_tail) {
      uint64_t overflow_size = cur_tail + size - hart_tables[hart_zap_id]->mem_tail;
      if( hart_tables[hart_zap_id]->mem_head + overflow_size > cur_head ){
        return -1; // No space
      }else{
        // TODO: Write padding
        hart_tables[hart_zap_id]->mem_cur_tail = hart_tables[hart_zap_id]->mem_head + size;
        hart_tables[hart_zap_id]->empty = false;
        *taddr = hart_tables[hart_zap_id]->mem_head;
        return 0;
      }
    }else{
      hart_tables[hart_zap_id]->mem_cur_tail += size;
      hart_tables[hart_zap_id]->empty = false;
      if( hart_tables[hart_zap_id]->mem_cur_tail >
          hart_tables[hart_zap_id]->mem_tail ){
        output.verbose(CALL_INFO, 9, 0, "Mem addr unexpected wrap");
        hart_tables[hart_zap_id]->mem_cur_tail =
          hart_tables[hart_zap_id]->mem_head;
      }
      *taddr = cur_tail;
      return 0;
    }
  }else{
    output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                   hart_id, hart_tables[hart_zap_id]->mem_head,
                   hart_tables[hart_zap_id]->mem_tail);
    // Wraparound
    if( cur_tail + size > cur_head ){
      // Not enough space
      output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                     hart_id, hart_tables[hart_zap_id]->mem_head,
                     hart_tables[hart_zap_id]->mem_tail);
      return -1;
    }else{
      hart_tables[hart_zap_id]->empty = false;
      *taddr = cur_tail;
      return 0;
    }
  }
}

void ZEN::processZoneEgressQueue() {
  uint64_t cur_processed = 0;
  for( unsigned zones = 0; zones < m_num_zones; ++zones ){
    for( unsigned i = 0; i < zone_queue[zones].size(); ++i ){
      if( zone_queue[zones][i]->status == ZENStatus::UNPROCESSED ){
        // TODO: Credit check for dest zone
        forwardPktToExtZEN(zone_queue[zones][i]->msg);
        zone_queue[zones][i]->status = ZENStatus::DONE;
      }
      zone_queue[zones].erase(std::remove_if(
        zone_queue[zones].begin(), zone_queue[zones].end(),
        [](auto x) {
            return x->status == ZENStatus::DONE;
        }), zone_queue[zones].end());
      }
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

void ZEN::processPrecinctEgressQueue() {
  uint64_t cur_processed = 0;
  for( unsigned precincts = 0; precincts < m_num_precincts; ++precincts ){
    for( unsigned i = 0; i < precinct_queue[precincts].size(); ++i ){
      if( (precinct_queue[precincts][i]->status == ZENStatus::UNPROCESSED) &&
          ((uint64_t)(precinct_queue[precincts][i]->msg->getLength()+Z_NUM_HEADER_FLITS) <=
           zip_credits) ){
          //((int64_t)(zip_credits) >=
          // (precinct_queue[precincts][i]->msg->getLength()+Z_NUM_HEADER_FLITS)) ){
        // Note: Revised this logic
        // used (int64_t)A >= B instead of A - B >= 0 to fix compiler error:
        // comparison of unsigned expression in ‘>= 0’ is always true
        // [-Werror=type-limits] and compiler error  error: comparison of
        // integer expressions of different signedness: ‘uint64_t’
        // {aka ‘long unsigned int’} and ‘int’ [-Werror=sign-compare]
        // TODO: Credit check for dest zone
        forwardPktToZIP(precinct_queue[precincts][i]->msg);
        precinct_queue[precincts][i]->status = ZENStatus::DONE;
      }
      precinct_queue[precincts].erase(std::remove_if(
        precinct_queue[precincts].begin(), precinct_queue[precincts].end(),
        [](auto x) {
            return x->status == ZENStatus::DONE;
        }), precinct_queue[precincts].end());
    }
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

void ZEN::processEgressQueue() {
  uint64_t cur_processed = 0;
  for( unsigned zap_id = 0; zap_id < m_num_zaps; ++zap_id ){
    for( unsigned harts = 0; harts < m_num_harts; ++harts ){
      std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, harts);
      for( unsigned i = 0; i < zen_queue[hart_zap_id].size(); ++i ){
        if( zen_queue[hart_zap_id][i]->status == ZENStatus::UNPROCESSED ){

          // TODO: Credits check. This will still be functional since entries beyond what the RZA queue
          // is provisioned for will be NACKed.
          output.verbose(CALL_INFO, 9, 0, "progress status %d\n",
                         zen_queue[hart_zap_id][i]->status);

          uint64_t rza_addr = 0x00ull;

          if(getRZATailQueue(zap_id, harts,
                             (uint64_t)zen_queue[hart_zap_id][i]->msg->getPayload().size() * DW_OFFSET,
                             &rza_addr) == -1) {
            if( !zen_queue[hart_zap_id][i]->from_zip ){
              auto *ev = zen_queue[hart_zap_id][i]->msg;
              sendNACK(ev->getSrcHart(),
                       ev->getSrcZCID(),
                       ev->getSrcPCID(),
                       ev->getSrcPrec(),
                       ev->getID(),
                       m_zop_iface);
              //sendNACKToZAP(zen_queue[hart_zap_id][i]->msg->getSrcHart(), zen_queue[hart_zap_id][i]->msg->getSrcZCID(),
              //              zen_queue[hart_zap_id][i]->msg->getID());
            }else{
              // FIXME: What are we doing here?  Is this just dead code?
              // sendNACKToZIP(zen_queue[hart_zap_id][i]->msg->getSrcHart(), zen_queue[hart_zap_id][i]->msg->getSrcZCID(),
              //               zen_queue[hart_zap_id][i]->msg->getID());
            }
            zen_queue[hart_zap_id][i]->status = ZENStatus::RZA_ADDR_ERROR;
            return;
          }
          zen_queue[hart_zap_id][i]->status = ZENStatus::RZA_ADDR_ASSIGNED;
          zen_queue[hart_zap_id][i]->rza_start_addr = rza_addr;
        }
        zen_queue[hart_zap_id].erase(std::remove_if(
          zen_queue[hart_zap_id].begin(), zen_queue[hart_zap_id].end(),
          [](auto x) {
              return x->status == ZENStatus::RZA_ADDR_ERROR;
          }), zen_queue[hart_zap_id].end());
        }
      cur_processed++;
      if( cur_processed > process_per_cycle ){
        break;
      }
    }
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

void ZEN::prepSendRZAHZOP() {
  // Send with address and size
  uint64_t cur_processed = 0;
  for( unsigned zap_id = 0; zap_id < m_num_zaps; ++zap_id ){
    for( unsigned harts = 0; harts < m_num_harts; ++harts ){
      std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, harts);
      for( unsigned i = 0; i < zen_queue[hart_zap_id].size(); ++i ){
        if( (zen_queue[hart_zap_id][i]->status == ZENStatus::RZA_ADDR_ASSIGNED) &&
            (zen_queue[hart_zap_id][i]->msg->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDAS)){
          uint64_t rza_addr = zen_queue[hart_zap_id][i]->rza_start_addr;
          std::vector<uint64_t> payload = zen_queue[hart_zap_id][i]->msg->getPayload();

          uint8_t next_msg_id = 0;
          if( !findFirstUnsetBit(msg_id, &next_msg_id) ){
            break;
          }
          msg_id[next_msg_id] = true;
          sendHZOPToRZA(getWriteACS(hart_tables[hart_zap_id]->acs_pair),
                        rza_addr, payload[0], payload[1], next_msg_id,
                        harts, i);
          zen_queue[hart_zap_id][i]->tail = rza_addr;
          //outstanding_mem_req[next_msg_id] = std::make_pair(harts, i);
          outstanding_mem_req[next_msg_id] = zen_queue[hart_zap_id][i];
          zen_queue[hart_zap_id][i]->msg_ids.push_back(next_msg_id);
          zen_queue[hart_zap_id][i]->status = MZOP_SENT;
        }
      }
      cur_processed++;
      if( cur_processed > process_per_cycle ){
        break;
      }
    }
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

void ZEN::prepSendRZAStore() {
  // Send with payload
  uint64_t cur_processed = 0;
  for( unsigned zap_id = 0; zap_id < m_num_zaps; ++zap_id ){
    for( unsigned harts = 0; harts < m_num_harts; ++harts ){
      std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, harts);
      for( unsigned i = 0; i < zen_queue[hart_zap_id].size(); ++i ){
        bool success = true;
        if( (zen_queue[hart_zap_id][i]->status == ZENStatus::RZA_ADDR_ASSIGNED) &&
            (zen_queue[hart_zap_id][i]->msg->getOpc() == SST::Forza::zopOpc::Z_MSG_SENDP)) {
          uint64_t rza_addr = zen_queue[hart_zap_id][i]->rza_start_addr;
          std::vector<uint64_t> payload = zen_queue[hart_zap_id][i]->msg->getPayload();
          if( dma_enabled ){
            uint8_t next_msg_id = 0;
            if( !findFirstUnsetBit(msg_id, &next_msg_id) ){
              break;
            }
            msg_id[next_msg_id] = true;
            sendMsgToRZADMA(getWriteACS(hart_tables[hart_zap_id]->acs_pair),
                            rza_addr, payload, next_msg_id, harts, i);
            zen_queue[hart_zap_id][i]->tail = rza_addr;
            outstanding_mem_req[next_msg_id] = zen_queue[hart_zap_id][i];
            output.verbose(CALL_INFO, 9, 0,
                           "Free msg_id %d in [%u,%u]\n",
                           next_msg_id, harts, i);
            zen_queue[hart_zap_id][i]->msg_ids.push_back(next_msg_id);
            zen_queue[hart_zap_id][i]->status = MZOP_SENT;
          }else{
            std::vector<uint8_t> avail_msg_id;
            for( unsigned j = 0; j < payload.size(); ++j ){
              uint8_t next_msg_id = 0;
              output.verbose(CALL_INFO, 9, 0, "Free msg_id %d\n",
                             next_msg_id);
              if( !findFirstUnsetBit( msg_id, &next_msg_id) ){
                // TODO: If you're here, that means all of the msg_id bits are set...
                // why would you want to unset it?
                for( unsigned k = 0; j < avail_msg_id.size(); ++k ){
                  msg_id[avail_msg_id[k]] = false;
                }
                success = false;
                break;
              }
              msg_id[next_msg_id] = true;
              avail_msg_id.push_back(static_cast<uint8_t>(next_msg_id));
            }

            if( success ){
              for( unsigned j = 0; j < payload.size(); ++j ){
                sendMsgToRZANonDMA(getWriteACS(hart_tables[hart_zap_id]->acs_pair),
                                   rza_addr, payload[j],
                                   avail_msg_id[j],  harts, i);
                zen_queue[hart_zap_id][i]->tail = rza_addr;
                outstanding_mem_req[avail_msg_id[i]] = zen_queue[hart_zap_id][i];
                zen_queue[hart_zap_id][i]->msg_ids.push_back(avail_msg_id[j]);
                rza_addr += DW_OFFSET;
                if( rza_addr > hart_tables[hart_zap_id]->mem_tail ){
                  rza_addr = hart_tables[hart_zap_id]->mem_head;
                }
              }
              zen_queue[hart_zap_id][i]->status = MZOP_SENT;
            }
          }
          if( !success ){
            continue;
          }
        }
      }
      cur_processed++;
      if( cur_processed > process_per_cycle ){
        break;
      }
    }
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

void ZEN::processZAPCredits() {
  uint64_t cur_processed = 0;
  for( unsigned i = 0; i < zap_credits.size(); ++i ){
    output.verbose(CALL_INFO, 9, 0, "queue loc %d\n", i);

    if( !zap_credits[i] ){
      continue;
    }

    uint64_t hart_id = zap_credits[i]->getSrcHart();
    uint64_t zap_id = zap_credits[i]->getSrcZCID();
    std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, hart_id);
    output.verbose(CALL_INFO, 9, 0, "hart_zap_id: [%" PRIu64 ", %" PRIu64 "]\n",
                   hart_zap_id.first, hart_zap_id.second);

    uint64_t credits = zap_credits[i]->getCredit();
    uint64_t cur_tail = hart_tables[hart_zap_id]->mem_cur_tail;
    uint64_t cur_head = hart_tables[hart_zap_id]->mem_cur_head;

    if( cur_tail > cur_head ){
      if( (cur_head + credits) > cur_tail ){
        hart_tables[hart_zap_id]->mem_cur_head = cur_tail;
      }else{
        hart_tables[hart_zap_id]->mem_cur_head += credits;
        if( hart_tables[hart_zap_id]->mem_cur_head == cur_tail ){
          hart_tables[hart_zap_id]->empty = true;
        }
      }
    }else if( cur_tail < cur_head ){
      cur_head = (cur_head + credits);
      if( cur_head > hart_tables[hart_zap_id]->mem_tail ){
        cur_head = hart_tables[hart_zap_id]->mem_head +
                   cur_head -
                   hart_tables[hart_zap_id]->mem_tail;
        if( cur_head > cur_tail ){
          cur_head = cur_tail;    // Do not accept credits beyond queue size
        }
      }

      hart_tables[hart_zap_id]->mem_cur_head = cur_head;

      if( hart_tables[hart_zap_id]->mem_cur_head == cur_tail ){
        hart_tables[hart_zap_id]->empty = true;
      }
    }

    output.verbose(CALL_INFO, 9, 0, "set queue %" PRIu64 " to [%" PRIu64 ", %" PRIu64 "]\n",
                   hart_id, hart_tables[hart_zap_id]->mem_cur_head,
                   hart_tables[hart_zap_id]->mem_cur_tail);
    delete zap_credits[i];
    zap_credits[i] = NULL;
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
  zap_credits.erase(std::remove_if(
    zap_credits.begin(), zap_credits.end(),
    [](auto x) {
        return !x;
    }), zap_credits.end());
}

void ZEN::processSetupMsgs(){
  uint64_t cur_processed = 0;
  //output.verbose(CALL_INFO, 9, 0, "Progress setup msg\n");
  for( unsigned i = 0; i < setup_reqs.size(); ++i ){

    std::vector<uint64_t> payload = setup_reqs[i]->getPayload();
    uint64_t hart_id = setup_reqs[i]->getSrcHart();
    uint64_t zap_id = setup_reqs[i]->getSrcZCID();
    std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, hart_id);
    auto *ev = setup_reqs[i];

    output.verbose(CALL_INFO, 9, 0, "setup pkt size %zu for hart %" PRIu64 "\n",
                   payload.size(), hart_id);
    if( (setup_reqs[i]->getPayload().size() < 4) ||
        (hart_tables.find(hart_zap_id) != hart_tables.end()) ){
      // From ZIP setup is assumed to be validated at ZIP
      //sendNACKToZAP(hart_id, zap_id, setup_reqs[i]->getID());
      sendNACK(ev->getSrcHart(),
               ev->getSrcZCID(),
               ev->getSrcPCID(),
               ev->getSrcPrec(),
               ev->getID(),
               m_zop_iface);
    }

    // TODO: Specify payload format
    uint64_t acs_pair = payload[0];
    uint64_t mem_start_addr = payload[1];
    uint64_t size = payload[2];
    uint64_t mem_end_addr = mem_start_addr + size - 1;
    uint64_t scratch_tail = payload[3];
    hart_tables[hart_zap_id] = new ZENTableRow(acs_pair, mem_start_addr,
                                               mem_end_addr, size,
                                               scratch_tail, 500);
    //sendACKToZAP(hart_id, zap_id, setup_reqs[i]->getID());
    sendACK(ev->getSrcHart(),
            ev->getSrcZCID(),
            ev->getSrcPCID(),
            ev->getSrcPrec(),
            ev->getID(),
            m_zop_iface);

    output.verbose(CALL_INFO, 9, 0, "setup hart table %" PRIu64 ", start addr %" PRIu64 ", end addr %" PRIu64 "\n",
                   hart_id, mem_start_addr, mem_end_addr);
    output.verbose(CALL_INFO, 9, 0, "setup hart table %" PRIu64 ", start addr %" PRIu64 ", end addr %" PRIu64 "\n",
                   hart_id, hart_tables[hart_zap_id]->mem_head,
                   hart_tables[hart_zap_id]->mem_tail);
    delete setup_reqs[i];
    setup_reqs[i] = NULL;
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
  setup_reqs.erase(std::remove_if(
    setup_reqs.begin(), setup_reqs.end(),
    [](auto x) {
        return !x;
    }), setup_reqs.end());
}

void ZEN::processZIPQueue() {
  uint64_t cur_processed = 0;

  for( unsigned i=0; i<zipQ.size(); i++ ){
    SST::Forza::zopEvent* ev = zipQ.front();

    std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(ev->getDestZCID(),
                                                               ev->getDestHart());
    if( zen_queue[hart_zap_id].size() < zen_queue_size_limit ){
      output.verbose(CALL_INFO, 9, 0, "Entry in ZIP queue processed\n");
      zen_queue[hart_zap_id].push_back(new ZENEntry(ev,
                                                    ZENStatus::UNPROCESSED,
                                                    true));

      zopEvent* creditZop = new zopEvent();
      creditZop->setType(zopMsgT::Z_MSG);
      creditZop->setOpc(zopOpc::Z_MSG_CREDIT);
      creditZop->setSrcZCID(zopCompID::Z_ZEN);
      creditZop->setSrcPCID(m_zop_iface->getZoneID());
      creditZop->setSrcPrec(m_zop_iface->getPrecinctID());
      creditZop->setCredit(ev->getLength()+Z_NUM_HEADER_FLITS);
      creditZop->setDestHart(0);    // going to the ZIP, only one hart
      creditZop->setDestZCID((uint8_t)(SST::Forza::zopCompID::Z_PREC_ZIP));
      creditZop->setDestPCID((uint8_t)(SST::Forza::zopPrecID::Z_ZIP));
      creditZop->setDestPrec((uint8_t)(m_zop_iface->getPrecinctID()));

      creditZop->encodeEvent();

      m_prec_iface->send(creditZop, zopCompID::Z_PREC_ZIP,
                         zopPrecID::Z_ZIP, m_prec_iface->getPrecinctID());

      zipQ.pop();
    }else{
      output.verbose(CALL_INFO, 1, 0, "Destination %d queue full\n", ev->getDestHart());
      zipQ.pop();
      zipQ.push(ev);
    }
    cur_processed++;
    if( cur_processed > process_per_cycle ){
      break;
    }
  }
}

bool ZEN::clock(Cycle_t cycle){
  processZAPCredits();
  notifyHARTScratchpad();
  handleIncomingRZAMsg();
  prepSendRZAHZOP();
  prepSendRZAStore();
  processEgressQueue();
  processZoneEgressQueue();
  processPrecinctEgressQueue();
  processSetupMsgs();
  processZIPQueue();

  return false;
}

} // namespace SST::Forza
// EOF
