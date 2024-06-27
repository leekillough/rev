//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"

#define DW_OFFSET 8

namespace SST::Forza{

uint64_t ZenMailboxMetadata::getRzaWriteAddr(uint8_t size)
{
  uint64_t wr_ptr = mem_wr_ptr; // address to write
  uint64_t next_ptr = wr_ptr + (size * sizeof(uint64_t));
  if (next_ptr > mem_tail) //shouldn't happen
    return (uint64_t)-1;
  else if (next_ptr == mem_tail)
    next_ptr = mem_head;
  
  mem_wr_ptr = next_ptr; // update write ptr
  return wr_ptr;
}

// This is called AFTER we've written memory
uint64_t ZenMailboxMetadata::getSpTailAddr(uint8_t size)
{
  uint64_t wr_ptr = mem_cur_tail; //current tail; we wrote to this address
  uint64_t next_ptr = wr_ptr + (size * sizeof(uint64_t));
  if (next_ptr > mem_tail) //shouldn't happen
    return (uint64_t)-1;
  else if (next_ptr == mem_tail)
    next_ptr = mem_head;
  
  mem_cur_tail = next_ptr; // update to past where we've written
  return mem_cur_tail;
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
  zone_nic = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
  zone_nic->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingZOP));
  zone_nic->setEndpointType(zopCompID::Z_ZEN);
  zone_nic->setNumHarts(m_num_harts);
  zone_nic->setPrecinctID(Precinct);
  zone_nic->setZoneID(Zone);

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
  zone_nic->init(phase);
  if (precinct_nic_enabled) {
    m_prec_iface->init(phase);
  }
}

void ZEN::setup() {
  zone_nic->setup();
  if (precinct_nic_enabled) {
    m_prec_iface->setup();
  }
}

void ZEN::complete(unsigned int phase) {
  zone_nic->complete(phase);
  if (precinct_nic_enabled) {
    m_prec_iface->complete(phase);
  }
}

void ZEN::finish() {
  output.verbose(CALL_INFO, 10, 0, "Finish()\n");
}

/**
 * tdysart, 27-june-2024
 * Until we have (N)ACKs coming back from ZQMs, we shouldn't be receiving anything from
 * the precinct NOC. 
 */
void ZEN::handleIncomingPrecZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 9, 0, "[PRECINCT]: %s received msg type %s @ [hart:zcid:pcid:type]=[%d:%d:%d:%s]\n",
                 getName().c_str(),
                 zone_nic->msgTToStr(ev->getType()).c_str(),
                 ev->getSrcHart(), ev->getSrcZCID(), ev->getSrcPCID(),
                 zone_nic->endPToStr(zone_nic->getEndpointType()).c_str());

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
      //helper_handleFromZoneMsgZop(ev);
      output.fatal(CALL_INFO, -1, "ZEN %s: should not receive message packets from the precinct NOC\n",
                   getName().c_str());
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

#if 0
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
#endif

/* TODO: This function is now less broken.
    As we can have {h,m,r} zops that pass through we need to handle those
    Excption message types can be an error for now, but could be valid (need to do a bit more research)
    Thread Mgmt, syscall, and fence types are not expected to be seen here (as of now)
*/
void ZEN::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 7, 0, "[ZEN]: %s received msg_id=%u from %s to %s\n",
                 getName().c_str(),
                 ev->getID(),
                 ev->getSrcString().c_str(),
                 ev->getDestString().c_str());

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
        output.fatal(CALL_INFO, -2, "ZEN %s: received a memory zop with local dest\n",
                     getName().c_str());
      // TODO: Handle credits?
      // Put packet in outgoing queue
      // to_precinct_noc_q.push_back(ev);
      output.fatal(CALL_INFO, -3, "ZEN %s: received an outgoing memory zop packet (unhandled)\n",
                   getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_TMIG:
      // Thread migration type; for now, thrown an error
      //    Should be to a different zone (any precinct);
      //    and just pass through here
      output.fatal(CALL_INFO, -4, "ZEN %s: received a thread migration packet (unhandled)\n",
        getName().c_str());
      break;

    case SST::Forza::zopMsgT::Z_EXCP:
      // Exception type; for now, throw an error
      output.fatal(CALL_INFO, -5, "ZEN %s: received an exception packet (unhandled)\n",
        getName().c_str());
      break;

    default: // Z_TMGT, Z_SYSC, Z_FENCE: don't expect to see here (as of now)
      output.fatal(CALL_INFO, -6, "ZEN %s: received an unexpected packet type\n",
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
      output.verbose(CALL_INFO, 9, 0, "ZEN Handle SENDP\n");
      from_zone_messaging_queue.push(ev);
      break;

    case SST::Forza::zopOpc::Z_MSG_SENDAS:
      // handle messaging zop - data in memory
      output.fatal(CALL_INFO, -1, "ZEN %s: SENDAS not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_MBXDONE:
      // Might be able to handle the same as SENDP - don't see why we can't
      output.fatal(CALL_INFO, -1, "ZEN %s: MBXDONE not yet implemented\n",
        getName().c_str());
      break;

    case SST::Forza::zopOpc::Z_MSG_CREDIT:
      // Handle credit msg
      //output.fatal(CALL_INFO, -1, "ZEN %s: CREDIT not yet implemented\n",
      //  getName().c_str());
      output.verbose(CALL_INFO, 5, 0, "[ERROR] ZEN %s: CREDIT not yet implemented\n",
                     getName().c_str());
      //zap_credits.push_back(ev);
      break;

    case SST::Forza::zopOpc::Z_MSG_ZENSET:
      // Handle zen setup msg
      output.fatal(CALL_INFO, -1, "ZEN %s: received a setup packet - not yet implemented\n",
                 getName().c_str());
      //setup_reqs.push(ev);
      break;
    
    default:
      output.fatal(CALL_INFO, -1, "ZEN %s: received an unexpected messaging packet opcode from zone NoC\n",
                 getName().c_str());
      break;
  }
}

#if 0
void ZEN::sendSdmaToRza(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                        std::vector<uint64_t> store_payload, uint16_t msg_id,
                        uint64_t wr_addr)
{
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
  output.verbose(CALL_INFO, 9, 0, "[ZEN] %s: Send StoreDMA to RZA msg_id=%" PRIu16 ", payload length=%" PRIu32 "\n",
                 getName().c_str(), msg_id, (uint32_t)store_payload.size());
  zone_nic->send(rzaMsg, zopCompID::Z_RZA);
}
#endif

#if 0
// NOT USED 
void ZEN::sendSdmaToRzaAsSequence(ZenMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
                                  std::vector<uint64_t> store_payload, 
                                  std::vector<uint16_t> msg_ids, uint64_t wr_addr)
{
  output.fatal(CALL_INFO, -1, "Should NOT be calling Send StoreDMA to RZA as Sequence\n");

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
    zone_nic->send(rzaMsg, zopCompID::Z_RZA);
  }
}
#endif

#if 0
void ZEN::sendMsgToScratchpad(SST::Forza::zopEvent *ev, std::vector<uint64_t> payload, uint16_t msg_id, bool destsp_is_src)
{
  auto *spd_msg = new SST::Forza::zopEvent();
  spd_msg->setType(SST::Forza::zopMsgT::Z_MZOP);
  spd_msg->setOpc(SST::Forza::zopOpc::Z_MZOP_SCSD);
  spd_msg->setID(msg_id);
  setMeAsZopSrc(spd_msg);
  if (destsp_is_src)
    setDestFromSrcInfo(spd_msg, ev);
  else
    setDestFromDestInfo(spd_msg, ev);
  spd_msg->setPayload(payload);
  spd_msg->encodeEvent();
  output.verbose(CALL_INFO, 9, 0, "ZENSP Send message to scratchpad with msg_id=%" PRIu16 " and addr=0x%" PRIx64 "\n", 
                 msg_id, payload.at(1));
  for (auto i : payload)
    output.verbose(CALL_INFO, 9, 0, "\t ZENSP: Payload=0x%" PRIx64 "\n", i);
  zone_nic->send(spd_msg, (zopCompID)spd_msg->getDestZCID());
  // An ACK is expected - track it
  outstanding_spad_reqs.push_back(msg_id);
}
#endif

#if 0
void ZEN::notifyHARTScratchpad() {
  if (update_scratchpad_q.empty())
    return;

  for (unsigned i = 0; i < process_per_cycle; i++){
    // Get a message ID
    uint16_t msg_id = zoneMsgID->getMsgId();
    if (msg_id == Z_MAX_MSG_IDS)
      return; // no IDs available, can't send

    auto *ev = update_scratchpad_q.front();
    auto *mbox_info = getDestMboxEntry(ev);
    uint64_t new_tail_ptr = mbox_info->getSpTailAddr(ev->getLength() + Z_NUM_HEADER_FLITS);

    // Have the address to update; now need to create a ZOP to the scratchpad
    // Scratchpad address - Z_FLIT_ADDR
    // Contents to write to address - Z_FLIT_DATA
    std::vector<uint64_t> payload;
    payload.push_back(0); // ACS pair, not necesary for SPAD
    payload.push_back(mbox_info->scratch_tail);
    payload.push_back(new_tail_ptr);

    sendMsgToScratchpad(ev, payload, msg_id, false);

    // Can delete ev at this point.
    delete ev;
    update_scratchpad_q.pop();
    if (update_scratchpad_q.empty())
      return;
  }
}
#endif

#if 0
void ZEN::handleScratchpadAck(uint16_t msg_id)
{
  output.verbose(CALL_INFO, 9, 0, "ZENSP %s processing scratchpad ack with id=%" PRIu16 "\n",
                 getName().c_str(), msg_id);
  size_t start_sz = outstanding_spad_reqs.size();
  outstanding_spad_reqs.erase(std::remove_if(outstanding_spad_reqs.begin(), 
                                             outstanding_spad_reqs.end(), 
                                             [msg_id](uint16_t x){return x == msg_id;}),
                              outstanding_spad_reqs.end());
  if (start_sz == outstanding_spad_reqs.size())
    //output.fatal(CALL_INFO, -1, "Vector did not change size - no matching ack ID found\n");
    output.verbose(CALL_INFO, 9, 0, "[ERROR] Vector did not change size - no matching ack ID found\n");
}
#endif

/*
  Logic here takes a minute to process, so let's lay it out
  mem_acks -> this queue is just a simple FIFO of incoming mem_acks
  rza_ret_wait_map -> this map is used to track outstanding memory requests
  update_scratchpad_q -> store the original messaging zops so we can update
     their scratchpad tail ptr
*/
#if 0
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
      output.verbose(CALL_INFO, 9, 0, "ZENRZA %s Process incoming RZA message msg_id %hu\n",
                    getName().c_str(), inc_msg_id);
      // Find inc_msg_id in the rza_ret_wait_map
      auto ret_map_itr = rza_ret_wait_map.find(inc_msg_id);
      if (ret_map_itr == rza_ret_wait_map.end()){
        output.fatal(CALL_INFO, -1, "ZEN did not find inc_msg_id=%" PRIu16 " in map\n", inc_msg_id);
      }

      // Get my original ZOP
      auto *ev = ret_map_itr->second->msg;
      output.verbose(CALL_INFO, 9, 0, "ZEN %s dealing with rza return for zop msg_id=%" PRIu16 "\n",
                    getName().c_str(), ev->getID());
      // Push original zop so we can update the scratchpad
      update_scratchpad_q.push(ev);
      
      // Clean up the map components
      delete ret_map_itr->second; // delete the MemReturnEntry*
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
#endif


void ZEN::sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc){
  SST::Forza::zopEvent *ack = new SST::Forza::zopEvent();
  ack->setType(SST::Forza::zopMsgT::Z_MSG);
  ack->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
  setMeAsZopSrc(ack);
  setDestFromSrcInfo(ack, ev);
  ack->setID(ev->getID());
  ack->encodeEvent();
  auto iface = (to_zone_noc) ? zone_nic : m_prec_iface;
  iface->send(ack, (zopCompID)ack->getDestZCID(), (zopPrecID)ack->getDestPCID(), (uint16_t)ack->getDestPrec());
  output.verbose(CALL_INFO, 9, 0, "ZEN %s sending ACK with msg_id=%" PRIu16 " to ZCID=%" PRIu8 "\n",
                 getName().c_str(), ev->getID(), ack->getDestZCID());
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
  nackMsg->setSrcPCID((uint8_t)(iface->getPCID(zone_nic->getZoneID())));
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

// NOTE: We save the full ZOP to memory; we will need the header info
// (especially the src location) to send credits back
// application s/w is going to have to handle some of that for now...
#if 0
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
    auto mbox_info = getDestMboxEntry(ev);
    uint64_t wr_addr = mbox_info->getRzaWriteAddr(ev->getLength() + Z_NUM_HEADER_FLITS);
    output.verbose(CALL_INFO, 9, 0, "Store messaging packet to RZA; packet_size=%" PRIu8 ", wr_addr=0x%"
                   PRIx64 "\n", ev->getLength(), wr_addr);

    // Send memory zops
    std::vector<uint64_t> store_payload;
    store_payload.push_back(mbox_info->acs_pair);
    store_payload.push_back(wr_addr);
    std::vector<uint64_t> pkt = ev->getPacket();
    store_payload.insert(std::end(store_payload),
                         std::begin(pkt),
                         std::end(pkt));
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
#endif

#if 0
void ZEN::processSetupMsgs(){
  if (setup_reqs.empty())
    return;
  
  //output.verbose(CALL_INFO, 9, 0, "Process Zen Setup Packet\n");
  for( unsigned i = 0; i < process_per_cycle; ++i ){
    auto *ev = setup_reqs.front();
    uint64_t hart_id = ev->getSrcHart();
    uint64_t zap_id = ev->getSrcZCID();
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
               zone_nic);
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
 
    // Need to have zap, hart, logical hart, app id, mbox id
    // Let's make a pair that is {AppID, Zap, Hart}, MboxId
    uint64_t pair1 = getMetadataHash(ev, false);
    std::pair<uint64_t, uint64_t> hart_mbox_id = std::make_pair(pair1, mbx_id);
    // Ensure we don't already have this pair
    if (hart_metadata_table.find(hart_mbox_id) != hart_metadata_table.end())
      output.fatal(CALL_INFO, -1, "Found a matching Hart/MBox pair");


    hart_metadata_table[hart_mbox_id] = new ZenMailboxMetadata(acs_pair, mem_start_addr,
                                                       mem_end_addr, size,
                                                       scratch_tail, app_id, mbx_id);

    sendACK(ev, zone_nic);
    
    output.verbose(CALL_INFO, 9, 0, "ZEN Setup packet; payload size %zu for zap, hart, mbox %" PRIu64 ", %" PRIu64 ", %" PRIu8 "\n",
                   payload.size(), zap_id, hart_id, mbx_id);

    // Need to set the scratch tail to the start of the memory buffer
    // going to assume we can get a msg_id
    uint16_t msg_id = zoneMsgID->getMsgId();
    std::vector<uint64_t> out_payload;
    out_payload.push_back(0);
    out_payload.push_back(scratch_tail);
    out_payload.push_back(mem_start_addr);
    sendMsgToScratchpad(ev, out_payload, msg_id, true);

    output.verbose(CALL_INFO, 9, 0, "Zen Setup, HartID %" PRIu64 ", start addr 0x%" PRIx64 ", end addr 0x%" PRIx64 "\n",
                   hart_id, mem_start_addr, mem_end_addr);
    output.verbose(CALL_INFO, 9, 0, "Zen Setup-Metadata table %" PRIu64 ", start addr 0x%" PRIx64 ", end addr 0x%" PRIx64 "\n",
                   hart_id, hart_metadata_table[hart_mbox_id]->mem_head,
                   hart_metadata_table[hart_mbox_id]->mem_tail);

    setup_reqs.pop();
    if (setup_reqs.empty())
      break;
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
    output.verbose(CALL_INFO, 9, 0, "ZEN: Local messaging packet, send ACK\n");
    sendACK(ev, true);  // this should release the sender MsgID

    // This packet is going to the local ZQM, so we just need to forward it
    output.verbose(CALL_INFO, 9, 0, "ZEN: Local messaging packet, forward packet to ZQM\n");
    //to_rza_q.push(ev); // will need this when we add in the retry buffer

    uint16_t new_msg_id = zoneMsgID->getMsgId();
    if ( new_msg_id == Z_MAX_MSG_IDS ){
      output.fatal(CALL_INFO, -2, "ZEN: Out of msg ids");
    }

    // Update msg_id and destination
    ev->setID(new_msg_id);
    ev->setDestZCID(SST::Forza::zopCompID::Z_ZQM);
    ev->encodeEvent();

    // Forward this on to the zone noc
    zone_nic->send( ev, SST::Forza::zopCompID::Z_ZQM );

  } else if ( (ev->getDestPCID() == Zone) && 
              (ev->getDestPrec() != Precinct) ) {
    // Same precinct, different zone
    //to_precinct_noc_q.push(ev);
    output.fatal(CALL_INFO, -2, "ZEN: Not yet implemented");
  } else {
    // Different precinct (don't care on zone)
    //to_precinct_noc_q.push(ev);
    output.fatal(CALL_INFO, -3, "ZEN: Not yet implemented");
  }

  from_zone_messaging_queue.pop();
}


bool ZEN::clock(Cycle_t cycle){
  //notifyHARTScratchpad();
  //handleIncomingRZAMsg();
  //prepSendRZAStore();
  //processSetupMsgs();
  processFromZoneMsgQueue();
  return false;
}

} // namespace SST::Forza
// EOF
