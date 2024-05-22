//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"

#define DW_OFFSET 8

namespace SST::Forza{

uint64_t ZenMailboxMetadata::getRzaWriteAddr(uint8_t size)
{
  uint64_t wr_ptr = mem_wr_ptr;
  uint64_t next_ptr = wr_ptr + (size * sizeof(uint64_t));
  if (next_ptr > mem_tail) //shouldn't happen
    output.fatal(CALL_INFO, -1, "Packet will be stored in non-contiguous memory");
  else if (next_ptr == mem_tail)
    next_ptr = mem_head;
  
  mem_wr_ptr = next_ptr;
  return wr_ptr;
}

uint64_t ZenMailboxMetadata::getSpTailAddr(uint8_t size)
{
  uint64_t wr_ptr = mem_cur_tail;
  uint64_t next_ptr = wr_ptr + (size * sizeof(uint64_t));
  if (next_ptr > mem_tail) //shouldn't happen
    output.fatal(CALL_INFO, -1, "Packet will be stored in non-contiguous memory");
  else if (next_ptr == mem_tail)
    next_ptr = mem_head;
  
  mem_cur_tail = next_ptr;
  return wr_ptr;
}

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id),
    zip_credits(_ZEN_DEFAULT_ZIP_CREDITS_){

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

  output.output("ZEN[%s] Forcing enableDMA to true.\n", getName().c_str());
  dma_enabled = true;

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

  zoneMsgID = new zopMsgID();
  if( zoneMsgID->getNumFree() != Z_MAX_MSG_IDS ){
    output.fatal(CALL_INFO, -1,
                 "Insufficient message IDs allocated in constructor\n");
  }

  // complete SST registration
  registerAsPrimaryComponent();
}

ZEN::~ZEN(){
  if( zoneMsgID )
    delete zoneMsgID;
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

  if (!isDestLocal(ev))
    output.fatal(CALL_INFO, -1, "ZEN %s: received a packet from precinct NoC not for this zone.\n",
        getName().c_str());

  // TODO: Do all non-messaging packets need to send a credit back to the ZIP if the packet 
  //   is from a different precinct?
  // TODO: Also review how messaging packets are handled and if they need to return ZIP
  //  credits as well

  switch(ev->getType()){
    case SST::Forza::zopMsgT::Z_MSG:
      // Messaging type; destined for a HART in this zone
      helper_handleFromZoneMsgZop(ev);
      break;

    case SST::Forza::zopMsgT::Z_RESP:
      // RZA Response
      // TODO: Scrape credits if we're doing so
      // Put onto zone NoC
      output.fatal(CALL_INFO, -1, "ZEN %s: received an incoming rza response zop packet (unhandled)\n",
                   getName().c_str());
      //to_zone_noc_q.push_back(ev);
      break;

    case SST::Forza::zopMsgT::Z_MZOP: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPAC: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPV: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_RZOP:
      // Memory zop type - forward on to zone NoC
      //to_zone_noc_q.push_back(ev);
      output.fatal(CALL_INFO, -1, "ZEN %s: received an incoming memory zop packet (unhandled)\n",
                   getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_TMIG:
      // Thread migration type; for now, thrown an error
      //    Forward to ZQM
      output.fatal(CALL_INFO, -1, "ZEN %s: received a thread migration packet (unhandled)\n",
        getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_EXCP:
      // Exception type; for now, throw an error
      output.fatal(CALL_INFO, -1, "ZEN %s: received an exception packet (unhandled)\n",
        getName().c_str());
      break;

    default: // Z_TMGT, Z_SYSC, Z_FENCE: don't expect to see here (as of now)
      output.fatal(CALL_INFO, -1, "ZEN %s: received an unexpected packet type\n",
        getName().c_str());
      break;
  }
}

void ZEN::helper_handleFromPrecMsgZop(SST::Forza::zopEvent *ev)
{
  // There are several of these that the ZEN needs to handle;
  switch(ev->getOpc()){
    case SST::Forza::zopOpc::Z_MSG_SENDP: [[fallthrough]];
    case SST::Forza::zopOpc::Z_MSG_SENDAS:
      // handle messaging zop
      output.fatal(CALL_INFO, -1, "ZEN %s: not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_MBXDONE:
      // Might be able to handle the same as SENDP - don't see why we can't offhand
      output.fatal(CALL_INFO, -1, "ZEN %s: not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_CREDIT:
      // Handle credit msg
      output.verbose(CALL_INFO, 7, 0, "ZEN %s: received credit packet; deleting it\n",
                     getName().c_str());
      delete ev;
      //zap_credits.push_back(ev);
      break;

    default:
      output.fatal(CALL_INFO, -1, "ZEN %s: received an unexpected messaging packet opcode from zone NoC\n",
                 getName().c_str());
      break;
  }
}

/* TODO: This function is now less broken.
    As we can have {h,m,r} zops that pass through we need to handle those
    Excption message types can be an error for now, but could be valid (need to do a bit more research)
    Thread Mgmt, syscall, and fence types are not expected to be seen here (as of now)
*/
void ZEN::handleIncomingZOP(SST::Event *event) {
  bool from_zip = false;
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 8, 0, "[ZONE]: %s received msg type %s @ [hart:zcid:pcid:id:type]=[%d:%d:%d:%hu:%s]\n",
                 getName().c_str(),
                 m_zop_iface->msgTToStr(ev->getType()).c_str(),
                 ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(), ev->getID(),
                 m_zop_iface->endPToStr(m_zop_iface->getEndpointType()).c_str());

  // Sanity check for incoming packets
  if (!isSrcLocal(ev))
    output.fatal(CALL_INFO, -1, "ZEN %s: received a packet from zone NoC with non-local source.\n",
        getName().c_str());

  switch(ev->getType()){
    case SST::Forza::zopMsgT::Z_MSG:
      // Messaging type; can stay here or out to precinct
      helper_handleFromZoneMsgZop(ev);
      break;

    case SST::Forza::zopMsgT::Z_RESP:
      // These can from the RZA and the ZAP scratchpad
      mem_acks.push(ev);
      break;

    case SST::Forza::zopMsgT::Z_MZOP: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPAC: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPV: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_RZOP:
      // Memory zop type - should be strictly outgoing to precinct NoC
      if (isDestLocal(ev))
        output.fatal(CALL_INFO, -1, "ZEN %s: received a memory zop with local dest\n",
                     getName().c_str());
      // TODO: Handle credits?
      // Put packet in outgoing queue
      // to_precinct_noc_q.push_back(ev);
      output.fatal(CALL_INFO, -1, "ZEN %s: received an outgoing memory zop packet (unhandled)\n",
                   getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_TMIG:
      // Thread migration type; for now, thrown an error
      //    Should be to a different zone (any precinct);
      //    and just pass through here
      output.fatal(CALL_INFO, -1, "ZEN %s: received a thread migration packet (unhandled)\n",
        getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_EXCP:
      // Exception type; for now, throw an error
      output.fatal(CALL_INFO, -1, "ZEN %s: received an exception packet (unhandled)\n",
        getName().c_str());
      break;

    default: // Z_TMGT, Z_SYSC, Z_FENCE: don't expect to see here (as of now)
      output.fatal(CALL_INFO, -1, "ZEN %s: received an unexpected packet type\n",
        getName().c_str());
      break;
  }
}

void ZEN::helper_handleFromZoneMsgZop(SST::Forza::zopEvent *ev)
{
  // There are several of these that the ZEN needs to handle
  switch(ev->getOpc()){
    case SST::Forza::zopOpc::Z_MSG_SENDP:
      // handle messaging zop - data in packet
      from_zone_messaging_queue.push_back(ev);
      break;

    case SST::Forza::zopOpc::Z_MSG_SENDAS:
      // handle messaging zop - data in memory
      output.fatal(CALL_INFO, -1, "ZEN %s: not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_MBXDONE:
      // Might be able to handle the same as SENDP - don't see why we can't
      output.fatal(CALL_INFO, -1, "ZEN %s: not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_CREDIT:
      // Handle credit msg
      output.fatal(CALL_INFO, -1, "ZEN %s: not yet implemented\n",
        getName().c_str());
      //zap_credits.push_back(ev);
      break;

    case SST::Forza::zopOpc::Z_MSG_ZENSET:
      // Handle zen setup msg
      setup_reqs.push(ev);
      break;
    
    default:
      output.fatal(CALL_INFO, -1, "ZEN %s: received an unexpected messaging packet opcode from zone NoC\n",
                 getName().c_str());
      break;
  }
}

void ZEN::sendSdmaToRza(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                        std::vector<uint64_t> store_payload, uint16_t msg_id,
                        uint64_t wr_addr)
{
  output.verbose(CALL_INFO, 9, 0, "Send StoreDMA to RZA\n");
  auto *rzaMsg = new SST::Forza::zopEvent();
  // Set packet header info
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SDMA);
  setMeAsZopSrc(rzaMsg);
  setLocalRzaAsZopDest(rzaMsg);
  rzaMsg->setID(msg_id);
  rzaMsg->setAppID(mbox_info->app_id);
  rzaMsg->setPayload(store_payload);
  rzaMsg->encodeEvent();
  m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
}

// NOT USED 
void ZEN::sendSdmaToRzaAsSequence(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                                  std::vector<uint64_t> store_payload, 
                                  std::vector<uint16_t> msg_ids, uint64_t wr_addr)
{
  output.fatal(CALL_INFO, -1, "Should NOT be calling Send StoreDMA to RZA as Sequence\n");
#if 0
  output.verbose(CALL_INFO, 9, 0, "Send StoreDMA to RZA as Sequence\n");
  for (uint8_t i = 0; i < msg_ids.size(); i++){
    auto *rzaMsg = new SST::Forza::zopEvent();
    // Set packet header info
    rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
    rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
    setMeAsZopSrc(rzaMsg);
    setLocalRzaAsZopDest(rzaMsg);
    rzaMsg->setID(msg_ids.at(i));
    rzaMsg->setAppID(mbox_info->app_id);
    rzaMsg->setPayload(store_payload.at(i));
    rzaMsg->encodeEvent();
    m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
  }
#endif
}

#if 0
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

  setMeAsZopSrc(rzaMsg);
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
#endif

#if 0
void ZEN::sendHZOPToRZA(uint64_t acs, uint64_t addr, uint64_t src_addr,
                        uint64_t size, uint8_t cur_msg_id, uint64_t hart_id,
                        uint64_t queue_loc) {
  // -------------------------------
  // FIXME
  // This method is 100% wrong
  // !!! Make this an LDMA request
  // -------------------------------
  //output.fatal(CALL_INFO, -1, "This operation is badly broken.  Enjoy!");
  output.verbose(CALL_INFO, 9, 0, "Msg tgt %" PRIu64 ", msg id %" PRIu8 "\n", addr, cur_msg_id);

  std::vector<uint64_t> payload;
  payload.push_back(acs);
  payload.push_back(src_addr);
  payload.push_back(addr);
  //payload.push_back(size);
  SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setID(cur_msg_id);
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
#endif

#if 0
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
#endif

void ZEN:sendMsgToScatchpad(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload,
                            uint16_t msg_id)
{
  output.verbose(CALL_INFO, 9, 0, "Send message to scratchpad\n");
  auto *spd_msg = new SST::Forza::zopEvent();
  spd_msg->setType(SST::Forza::zopMsgT::Z_MZOP);
  spd_msg->setOpc(SST::Forza::zopOpc::Z_MZOP_SCSD);
  spd_msg->setID(msg_id);
  setMeAsZopSrc(spd_msg);
  setDestFromSrcInfo(spd_msg, ev);
  spd_msg->setPayload(payload);
  spd_msg->encodeEvent();
  m_zop_iface->send(spd_msg, ev->getSrcZCID());
  // An ACK is expected - track it
  outstanding_spad_reqs.push_back(msg_id);
}

#if 0
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
#endif

#if 0
void ZEN::forwardPktToZIP(Forza::zopEvent *ev) {
  ev->setSrcPCID(m_zop_iface->getZoneID());
  ev->encodeEvent();
  m_prec_iface->send(ev, zopCompID::Z_PREC_ZIP,
                     zopPrecID::Z_ZIP, m_prec_iface->getPrecinctID());
  zip_credits -= (ev->getLength()+Z_NUM_HEADER_FLITS);
}
#endif

#if 0
void ZEN::forwardPktToExtZEN(Forza::zopEvent *ev) {
  ev->setSrcPCID(m_zop_iface->getZoneID());
  ev->encodeEvent();
  m_prec_iface->send(ev, zopCompID::Z_ZEN,
                     (zopPrecID)(ev->getDestPCID()),
                     m_prec_iface->getPrecinctID());
}
#endif

void ZEN::notifyHARTScratchpad() {
  if (update_scratchpad_q.empty())
    return;

  for (unsigned i = 0; i < process_per_cycle; i++){
    // Get a message ID
    uint16_t msg_id = zoneMsgID->getMsgId();
    if (msg_id == Z_MAX_MSG_IDS)
      return; // no IDs available, can't send

    auto *ev = update_scratchpad_q.front();
    auto *mbox_info = getMboxEntry(ev);    
    uint64_t new_tail_ptr = mbox_info->getSpTailAddr(ev->getLength());

    // Have the address to update; now need to create a ZOP to the scratchpad
    // Scratchpad address - Z_FLIT_ADDR
    // Contents to write to address - Z_FLIT_DATA
    std::vector<uint64_t> payload;
    payload.push_back(0); // ACS pair, not necesary for SPAD
    payload.push_back(mbox_info->scratch_tail);
    payload.push_back(new_tail_ptr);

    sendMsgToScratchpad(ev, payload, msg_id);

    // Can delete ev at this point.
    delete ev;
    update_scratchpad_q.pop();
    if (update_scratchpad_q.empty())
      return;
  }
}

void ZEN::handleScratchpadAck(uint16_t msg_id)
{
  output.verbose(CALL_INFO, 9, 0, "ZEN %s processing scratchpad ack with id=%" PRIu16 "\n",
                 getName().c_str(), msg_id);
  size_t start_sz = outstanding_spad_reqs.size();
  outstanding_spad_reqs.erase(std::remove_if(outstanding_spad_reqs.begin(), 
                                             outstanding_spad_reqs.end(), 
                                             [](uint16_t x){x == msg_id}),
                              outstanding_spad_reqs.end());
  if (start_sz == outstanding_spad_reqs.size())
    output.fatal(CALL_INFO, -1, "Vector did not change size - no matching ack ID found\n");
}

/*
  Logic here takes a minute to process, so let's lay it out
  mem_acks -> this queue is just a simple FIFO of incoming mem_acks
  rza_ret_wait_map -> this map is used to track outstanding memory requests
  update_scratchpad_q -> store the original messaging zops so we can update
     their scratchpad tail ptr
*/
void ZEN::handleIncomingRZAMsg() {
  //std::cout << "handleIncomingRZAMsg(): num messages = " << mem_acks.size() << std::endl;
  if (mem_acks.empty())
    return;

  for (unsigned i = 0; i < process_per_cycle; ++i) {
    auto ack_zop = mem_acks.front();
    uint16_t inc_msg_id = ack_zop->getID();

    // TODO: These acks can come in from different sources 
    // we have scratchpad acks from the ZAPs
    // and then we have regular acks from the RZA...probably
    // want to create separate functions for there.  And maybe
    // do some renaming
    if (ack_zop->getSrcZCID() <= (uint8_t)SST::Forza::zopCompID::Z_ZAP7){
      handleScratchpadAck(inc_msg_id);
    } else {
      // Turn into a function?  Probably ought to.
      output.verbose(CALL_INFO, 9, 0, "ZEN %s Process incoming RZA message msg_id %hu\n",
                    getName().c_str(), inc_msg_id);
      // Find inc_msg_id in the rza_ret_wait_map
      auto ret_map_itr = rza_ret_wait_map.find(inc_msg_id);
      if (ret_map_itr = rza_ret_wait_map.end()){
        output.fatal(CALL_INFO, -1, "ZEN did not find inc_msg_id=%" PRIu16 " in map\n", inc_msg_id);
      }

      // Get my original ZOP
      auto *ev = ret_map_iter.second().msg;
      output.verbose(CALL_INFO, 9, 0, "ZEN %s dealing with rza return for zop msg_id=%" PRIu16 "\n",
                    getName().c_str(), ev->getID());
      // Push original zop so we can update the scratchpad
      update_scratchpad_q.push(ev);
      
      // Clean up the map components
      delete ret_map_itr.second(); // delete the MemReturnEntry*
      rza_ret_wait_map.erase(ret_map_itr);
    }

    // Clear the MsgID from the network interface
    zoneMsgID->clearMsgId(inc_msg_id);

    // Done with ACK Zop, delete it
    mem_acks.pop();
    delete ack_zop;

   // Should be done with this iteration
    if (mem_acks.empty())
      return;
  }
}

#if 0
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
#endif

void ZEN::sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc){
  SST::Forza::zopEvent *ack = new SST::Forza::zopEvent();
  ack->setType(SST::Forza::zopMsgT::Z_MSG);
  ack->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  setMeAsZopSrc(ack);
  setDestFromSrcInfo(ack, ev);
  ack->setID(ev->getID());
  ack->encodeEvent();
  auto iface = (to_zone_noc) ? m_zop_iface : m_prec_iface;
  iface->send(ack, ack->getDestZCID(), ack->getDestPCID(), ack->getDestPrec());
}

#if 0
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
#endif

// TODO: Update Me!  Do we even need this?
#if 0
int ZEN::getRZATailQueue(uint64_t zap_id,
                         uint64_t hart_id,
                         uint64_t size,
                         uint64_t *taddr ){
  std::pair<uint64_t, uint64_t> hart_zap_id = std::make_pair(zap_id, hart_id);
  output.verbose(CALL_INFO, 9, 0, "hart_zap_id: [%" PRIu64 ", %" PRIu64 "]\n",
                 hart_zap_id.first, hart_zap_id.second);
  if (!hart_metadata_table[hart_zap_id]) { /* TODO: Raise exception */
    output.fatal(CALL_INFO, -1,
                 "Error: could not find table entry for hart=%" PRIu64 "\n",
                 hart_id);
    return 0; // should never reach this
  }
  uint64_t cur_tail = hart_metadata_table[hart_zap_id]->mem_cur_tail;
  uint64_t cur_head = hart_metadata_table[hart_zap_id]->mem_cur_head;
  output.verbose(CALL_INFO, 9, 0,
                 "RZA[%" PRIu64 "]: cur_head: %" PRIu64 ", cur_tail: %" PRIu64 ", size %" PRIu64 "\n",
                 hart_id, cur_head, cur_tail, size);
  output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                 hart_id, hart_metadata_table[hart_zap_id]->mem_head,
                 hart_metadata_table[hart_zap_id]->mem_tail);
  if( cur_tail >= cur_head ){
    if( (cur_tail == cur_head) && (!hart_metadata_table[hart_zap_id]->empty) ){
      output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: GOT -1!\n", hart_id);
      return -1;
    }

    if (cur_tail + size > hart_metadata_table[hart_zap_id]->mem_tail) {
      uint64_t overflow_size = cur_tail + size - hart_metadata_table[hart_zap_id]->mem_tail;
      if( hart_metadata_table[hart_zap_id]->mem_head + overflow_size > cur_head ){
        return -1; // No space
      }else{
        // TODO: Write padding
        hart_metadata_table[hart_zap_id]->mem_cur_tail = hart_metadata_table[hart_zap_id]->mem_head + size;
        hart_metadata_table[hart_zap_id]->empty = false;
        *taddr = hart_metadata_table[hart_zap_id]->mem_head;
        return 0;
      }
    }else{
      hart_metadata_table[hart_zap_id]->mem_cur_tail += size;
      hart_metadata_table[hart_zap_id]->empty = false;
      if( hart_metadata_table[hart_zap_id]->mem_cur_tail >
          hart_metadata_table[hart_zap_id]->mem_tail ){
        output.verbose(CALL_INFO, 9, 0, "Mem addr unexpected wrap");
        hart_metadata_table[hart_zap_id]->mem_cur_tail =
          hart_metadata_table[hart_zap_id]->mem_head;
      }
      *taddr = cur_tail;
      return 0;
    }
  }else{
    output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                   hart_id, hart_metadata_table[hart_zap_id]->mem_head,
                   hart_metadata_table[hart_zap_id]->mem_tail);
    // Wraparound
    if( cur_tail + size > cur_head ){
      // Not enough space
      output.verbose(CALL_INFO, 9, 0, "RZA[%" PRIu64 "]: mem_head: %" PRIu64 ", mem_tail %" PRIu64 "\n",
                     hart_id, hart_metadata_table[hart_zap_id]->mem_head,
                     hart_metadata_table[hart_zap_id]->mem_tail);
      return -1;
    }else{
      hart_metadata_table[hart_zap_id]->empty = false;
      *taddr = cur_tail;
      return 0;
    }
  }
}
#endif

#if 0
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
#endif

#if 0
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
#endif

#if 0
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
#endif

#if 0
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
          if( zoneMsgID->getNumFree() > 0 ){
            next_msg_id = zoneMsgID->getMsgId();
          }else{
            // no message ID's available
            return ;
          }

          output.verbose(CALL_INFO, 9, 0, "prepSendRZAHZOP: allocated message id=%hu\n",
                         next_msg_id);
          sendHZOPToRZA(getWriteACS(hart_metadata_table[hart_zap_id]->acs_pair),
                        rza_addr, payload[0], payload[1], next_msg_id,
                        harts, i);
          zen_queue[hart_zap_id][i]->tail = rza_addr;
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
#endif

// NOTE: We save the full ZOP to memory; we will need the header info
// (especially the src location) to send credits back
// application s/w is going to have to handle some of that for now...
void ZEN::prepSendRZAStore() {
  if (to_rza_q.empty())
    return;

  for (uint64_t i = 0; i < process_per_cycle; i++){
    auto *ev = to_rza_q.front();
    // Need to add 2 words to length for acs and wr_addr
    uint8_t num_msg_ids_req = ( dma_enabled ) ? 1 : (ev->getLength() + 2);
    // Get a set of message ids
    std::vector<uint16_t> msg_ids = zoneMsgID->getSetOfMsgIds(num_msg_ids_req);
    if (msg_ids.empty()){
      // Not enough free message IDs to send the packet...done for now
      break;
    } else if (msg_ids.size() != num_msg_ids_req){
      output.fatal(CALL_INFO, -1, "Invalid number of msg_ids returned\n");
    }

    // Get destination address for memory zop(s)
    auto mbox_info = getMboxEntry(ev);
    uint64_t wr_addr = mbox_info.getRzaWriteAddr(ev->getLength());
    output.verbose(CALL_INFO, 9, 0, "Store messaging packet to RZA; packet_size=%" PRIu8 ", wr_addr=%"
                   PRIu64 "\n", ev->getLength(), wr_addr);

    // Send memory zops
    std::vector<uint64_t> store_payload;
    store_payload.push_back(ev->acs_pair);
    store_payload.push_back(wr_addr);
    store_payload.insert(std::end(store_payload),
                         std::begin(ev->getPacket()),
                         std::end(ev->getPacket()));
    if ( dma_enabled )
      sendSdmaToRza(mbox_info, ev, store_payload, msg_ids[0], wr_addr);
    else
      sendSdmaToRzaAsSequence(mbox_info, ev, store_payload, msg_ids, wr_addr);

    // Create MemReturnEntry and put into data struct to await the RZA return
    // TODO: This need substantial improvement if we're turning a store into a 
    // full sequence of ZOPs
    auto *mem_retentry = new MemReturnEntry(ev, msg_ids);
    rza_ret_wait_map.insert(std::pair<uint16_t, MemReturnEntry*>(msg_ids[0], mem_retentry));

    // Finished with this packet
    to_rza_q.pop();
    if (to_rza_q.empty())
      break;
  }
}

#if 0
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
    uint64_t cur_tail = hart_metadata_table[hart_zap_id]->mem_cur_tail;
    uint64_t cur_head = hart_metadata_table[hart_zap_id]->mem_cur_head;

    if( cur_tail > cur_head ){
      if( (cur_head + credits) > cur_tail ){
        hart_metadata_table[hart_zap_id]->mem_cur_head = cur_tail;
      }else{
        hart_metadata_table[hart_zap_id]->mem_cur_head += credits;
        if( hart_metadata_table[hart_zap_id]->mem_cur_head == cur_tail ){
          hart_metadata_table[hart_zap_id]->empty = true;
        }
      }
    }else if( cur_tail < cur_head ){
      cur_head = (cur_head + credits);
      if( cur_head > hart_metadata_table[hart_zap_id]->mem_tail ){
        cur_head = hart_metadata_table[hart_zap_id]->mem_head +
                   cur_head -
                   hart_metadata_table[hart_zap_id]->mem_tail;
        if( cur_head > cur_tail ){
          cur_head = cur_tail;    // Do not accept credits beyond queue size
        }
      }

      hart_metadata_table[hart_zap_id]->mem_cur_head = cur_head;

      if( hart_metadata_table[hart_zap_id]->mem_cur_head == cur_tail ){
        hart_metadata_table[hart_zap_id]->empty = true;
      }
    }

    output.verbose(CALL_INFO, 9, 0, "set queue %" PRIu64 " to [%" PRIu64 ", %" PRIu64 "]\n",
                   hart_id, hart_metadata_table[hart_zap_id]->mem_cur_head,
                   hart_metadata_table[hart_zap_id]->mem_cur_tail);
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
#endif

void ZEN::processSetupMsgs(){
  if (setup_reqs.empty())
    return;
  
  //output.verbose(CALL_INFO, 9, 0, "Process Zen Setup Packet\n");
  for( unsigned i = 0; i < process_per_cycle; ++i ){
    auto *ev = setup_reqs.front();
    uint64_t hart_id = ev->getSrcHart();
    uint64_t zap_id = ev->getSrcZCID();
    uint64_t hdr_app_id = ev->getAppID();
    std::vector<uint64_t> payload = ev->getPayload();

    // Sanity check that payload length is correct
    if( (payload.size() < 5) )
      output.fatal(CALL_INFO, -1, "Invalid ZEN setup packet");

    // Changed the sanity checks to fatal errors, so no need for a NACK right now
    // Keeping in case we change them from fatal errors
      /*
      sendNACK(ev->getSrcHart(),
               ev->getSrcZCID(),
               ev->getSrcPCID(),
               ev->getSrcPrec(),
               ev->getID(),
               m_zop_iface);
      */

    // TODO: Specify payload format
    uint64_t acs_pair = payload[0];
    uint64_t mem_start_addr = payload[1];
    uint64_t size = payload[2];
    // uint64_t mem_end_addr = mem_start_addr + size - 1;
    uint64_t mem_end_addr = mem_start_addr + size;
    uint64_t scratch_tail = payload[3];
    uint8_t app_id = (uint8_t)((payload[4] >> 60) & Z_MASK_APPID);
    uint8_t mbx_id = (uint8_t)(payload[4] & Z_MASK_PKTRES);
    uint64_t credits = 1000;
 
    // Need to have zap, hart, logical hart, app id, mbox id
    // Let's make a pair that is {AppID, Zap, Hart}, MboxId
    uint64_t pair1 = getMetadataHash(ev);
    std::pair<uint64_t, uint64_t> hart_mbox_id = std::make_pair(pair1, mbx_id);
    // Ensure we don't already have this pair
    if (hart_metadata_table.find(hart_mbox_id) != hart_metadata_table.end())
      output.fatal(CALL_INFO, -1, "Found a matching Hart/MBox pair");


    hart_metadata_table[hart_mbox_id] = new ZenMailboxMetadata(acs_pair, mem_start_addr,
                                                       mem_end_addr, size,
                                                       scratch_tail, credits,
                                                       app_id, mbx_id);

    sendACK(ev, m_zop_iface);
    
    output.verbose(CALL_INFO, 9, 0, "ZEN Setup packet; payload size %zu for zap, hart, mbox %" PRIu64 ", %" PRIu64 ", %" PRIu8 "\n",
                   payload.size(), zap_id, hart_id, mbx_id);

    // Need to set the scratch tail to the start of the memory buffer
    // going to assume we can get a msg_id
    uint16_t msg_id = zopMsgID()->getMsgId();
    std::vector<uint64_t> payload;
    payload.push_back(0);
    payload.push_back(scratch_tail);
    payload.push_back(mem_start_addr);
    //sendMsgToScratchpad(hart_id, zcid, scratch_tail, 1, mem_start_addr); 
    sendMsgToScratchpad(ev, payload, msg_id);

    output.verbose(CALL_INFO, 9, 0, "Zen Setup, HartID %" PRIu64 ", start addr 0x%" PRIx64 ", end addr 0x%" PRIx64 "\n",
                   hart_id, mem_start_addr, mem_end_addr);
    output.verbose(CALL_INFO, 9, 0, "Zen Setup-Metadata table %" PRIu64 ", start addr 0x%" PRIx64 ", end addr 0x%" PRIx64 "\n",
                   hart_id, hart_metadata_table[hart_zap_id]->mem_head,
                   hart_metadata_table[hart_zap_id]->mem_tail);

    setup_reqs.pop();
    if (setup_reqs.empty())
      break;
  }
}

#if 0
void ZEN::processZIPQueue() {
  uint64_t cur_processed = 0;

  unsigned loop_size = zipQ.size();
  for (unsigned i=0; i<loop_size; i++) {
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
#endif

void ZEN::processFromZoneMsgQueue(){
  if(from_zone_messaging_queue.empty())
    return;

  auto *ev = from_zone_messaging_queue.front();
  
  // Couple of assumptions are being made right now:
  // 1. Not dealing with credits (infinite amount)
  // 2. We have the payload in the packet

  if (isDestLocal(ev)){
    // Determine mailbox
    // See if credits available from metadata table
    // if no credits, send nack
    // else, continue processing
    // TODO: This may need to wait until we get an ACK from 
    //  the RZA (only if we need to return an error code)
     sendAck(ev, true);  

    // This packet is going to a local mailbox
    to_rza_q.push(ev);

    // Update scratchpad tail pointer for mailbox - must wait until write is done
  
  } else if ( (ev->getDestPCID() == Zone) && 
              (ev->getDestPrec() != Precinct) ) {
    // Same precinct, different zone
    to_precinct_noc_q.push_back(ev);
  } else {
    // Different precinct (don't care on zone)
    to_precinct_noc_q.push_back(ev);
  }

  from_messaging_zone_queue.pop();
}


bool ZEN::clock(Cycle_t cycle){
  //processZAPCredits();
  notifyHARTScratchpad();
  handleIncomingRZAMsg();
  //prepSendRZAHZOP();  // fills the outstanding_queue with content
  prepSendRZAStore();
  //processEgressQueue();
  //processZoneEgressQueue();
  //processPrecinctEgressQueue();
  processSetupMsgs();
  //processZIPQueue();    // FIX THIS: NOT CURRENTLY ALLOCATING MESSAGE IDs

  return false;
}

} // namespace SST::Forza
// EOF
