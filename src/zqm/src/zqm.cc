//
// _zqm_cc_
//

#include <sst/core/sst_config.h>
#include "zqm.h"
#include "ZOPNET.h"

/**
 * NOTE: There are numerous places where the code uses extra variables, etc.
 * I don't care.  My goal is to make this easy to follow and ensure the spec matches since
 * it's being developed alongside the code.
 */

using namespace SST::Forza;

#if 0
// Comes from RevMem::ZOP_ThreadMigrate in forzarev/src/RevMem.cc
// Want to keep an even number of words "just because"
static const uint64_t ThreadLengthDblWords = 68;
static const uint64_t ThreadLengthBytes = (ThreadLengthDblWords * 8);

uint64_t ZqmMailboxMetadata::getRzaWriteAddr(uint8_t size)
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
uint64_t ZqmMailboxMetadata::getSpTailAddr(uint8_t size)
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
#endif

#if 0
uint64_t ZqmAidStateTableRow::getMemAddr(bool do_read, bool update_ptr)
{
    uint64_t addr_ptr = (do_read) ? mem_read_ptr : mem_write_ptr;
    if (!update_ptr)
        return addr_ptr;

    // Below here, we do an update
    // First, determine the updated ptr
    uint64_t next_ptr = addr_ptr + ThreadLengthBytes;
    if (next_ptr >= mem_buffer_high)
        next_ptr = mem_buffer_low;

    if (do_read){
        if (mem_read_ptr == mem_write_ptr) { // nothing to read
            return 0;
        } else { // can read; update rd_ptr
            mem_read_ptr = next_ptr;
            return addr_ptr;
        }
    } else {
        if (next_ptr == mem_read_ptr) { // no room to write
            return 0;
        } else {
            mem_write_ptr = next_ptr; // can write; update wr_ptr
            return addr_ptr;
        }
    }
}

bool ZqmAidStateTableRow::validateMemBuffSize() {
    uint64_t diff = (mem_buffer_high + 1) - (mem_buffer_low);
    if ( (diff % ThreadLengthBytes) == 0)
        return true;
    return false;
}
#endif

ZQM::ZQM(ComponentId_t id, Params& params)
        : Component(id)
{
    // Init the output handler
    const int Verbosity = params.find<int>("verbose", 7);
    output.init("ZQM[" + getName() + ":@p:@t]: ",
                Verbosity, 0, SST::Output::STDOUT);

    // read the remaining parameters
    clockFreq = params.find<std::string>("clockFreq", "1GHz");

    // register the clock handler
    registerClock(clockFreq, new Clock::Handler<ZQM>(this, &ZQM::clock));
    output.verbose(CALL_INFO, 7, 0, "ZQM[%s] Registering clock with frequency=%s\n",
                   getName().c_str(), clockFreq.c_str());

    // The parameter finds below are using the same names as RevCPU.h
    numCores = params.find<unsigned>("numCores", 1);
    numHarts = params.find<uint16_t>("numHarts", 4);
    PrecinctId = params.find<unsigned>("precinctId", 0);
    ZoneId = params.find<unsigned>("zoneId", 0);
    process_per_cycle = params.find<unsigned>("processPerCycle", 10);

    // setup the zone network
    bool zone_nic_enabled = true; // temporary use while developing ring code
    if (zone_nic_enabled){
        zone_nic = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
        zone_nic->setMsgHandler(new Event::Handler<ZEN>(this, &ZQM::handleIncomingZOP));
        zone_nic->setEndpointType(zopCompID::Z_ZQM);
        zone_nic->setNumHarts(numHarts);
        zone_nic->setPrecinctID(PrecinctId);
        zone_nic->setZoneID(ZoneId);
    }

    // Create and init matrix of HART status
    // zap_hart_status.resize(numCores);
    // for (auto &hart_vec: zap_hart_status)
    //  hart_vec.resize(numHarts, false);

    dma_enabled = true;

    my_name = "Precinct[" + std::to_string(PrecinctId) + "].Zone[" + std::to_string(ZoneId) + "].ZQM";
    output.verbose(CALL_INFO, 1, 0, "%s constructed\n", my_name.c_str());

    zoneMsgID = new zopMsgID();
    if( zoneMsgID->getNumFree() != Z_MAX_MSG_IDS ){
        output.fatal(CALL_INFO, -1,
                    "Insufficient message IDs allocated in constructor\n");
    }

    // Resize the CSR regs
    PerHartCSRs.resize( numCores );
    for ( auto &i : PerHartCSRs ){
        i.resize( numHarts );
    }

    // register with SST
    registerAsPrimaryComponent();
}

ZQM::~ZQM()
{
}

void ZQM::init(unsigned int phase) {
    output.verbose(CALL_INFO, 1, 0, "Init %s\n", my_name.c_str());
    if ( zone_nic )
        zone_nic->init(phase);
}

void ZQM::setup() {
    output.verbose(CALL_INFO, 1, 0, "Setup %s\n", my_name.c_str());
	if ( zone_nic )
        zone_nic->setup();
}

void ZQM::complete(unsigned int phase) {
    return;
}

void ZQM::finish() {
    output.verbose(CALL_INFO, 1, 0, "Finish %s\n", my_name.c_str());
}

void ZQM::handleRingMsg( SST::Event *event )
{
  SST::Forza::ringEvent *ev = static_cast<SST::Forza::ringEvent*>(event);

  if ( ev->getCSR() == R_ZQMSTAT ){
    handleRingStatus(ev);    
  } else if ( ev->getCSR() == R_ZQMMBOXREG ){
    handleRingMboxReg(ev);
  } else if ( ( ev->getCSR() >= R_ZQMDQ_0 ) && ( ev->getCSR() <= R_ZQMDQ_7) ){
    handleRingDq(ev);
  } else {
    output.fatal(CALL_INFO, -1, "[ZQM] %s unexpected ring message; CSR=0x%" PRIx16 "\n", getName().c_str(), ev->getCSR());
  }
  delete ev;
}

void ZQM::handleRingStatus( SST::Forza::ringEvent *ev )
{
  output.verbose(CALL_INFO, 7, 0, "[ZQM] %s handle ZENSTAT message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_READ )
    output.fatal(CALL_INFO, -1, "[ZQM] %s unexpected optype message; OpType=%u\n", getName().c_str(), ev->getOp());

  auto status = PerHartCSRs[ev->getZapId()][ev->getHartId()].status;
  sendRingResponse(ev, status);
  delete ev;
}

void ZQM::handleRingMboxReg( SST::Forza::ringEvent *ev )
{
    output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; handling a mailbox registration message\n", getName().c_str() );
    // Writing non-zero configures a mapping
    // Writing 0 clears a mapping - ignore for now
    auto x = ev->getData();
    if ( x == 0 ){
        output.verbose(CALL_INFO, 5, 0, "[ERROR] [ZQM] %s; mailbox unregister not handled\n", getName().c_str() )
        delete ev;
        return;
    }

    // But then we have to map that to some kind of state as to the status of the 
    // message buffers
    uint8_t aid = ( x >> R_SHIFT_AID ) & R_MASK_AID;
    uint8_t phys_zap = ( x >> R_SHIFT_PHYSZAP ) & R_MASK_PHYSZAP;
    uint16_t phys_hart = ( x >> R_SHIFT_PHYSHART ) & R_MASK_PHYSHART;
    uint16_t logic_pe = ( x >> R_SHIFT_LOGICALPE ) & R_MASK_LOGICALPE;
    uint8_t mbx_bitmap = ( x >> R_SHIFT_MBXSUSED ) & R_MASK_MBXSUSED;
    std::pair<uint8_t, uint16_t> p1(aid, logic_pe);
    auto iter = LogicalToPhysicalMap.find( p1 );
    if (iter != LogicalToPhysicalMap.end() ){
        output.fatal(CALL_INFO, -1, "[ZQM] %s; received mailbox reg for an alredy registered mbox; aid=%u, logic_pe=%u\n",
                     getName().c_str(), aid, logic_pe );
    }
    std::pair<uint8_t, uint16_t> p2(phys_zap, phys_hart);
    LogicalToPhysicalMap.insert( std::pair<std::pair<uint8_t, uint16_t>, std::pair<uint8_t, uint16_t>>(p1, p2) );
    auto regs = PerHartCSRs[phys_zap][phys_hart];
    regs.logical_pe = logic_pe;
    regs.aid = aid;
    for ( uint8_t i = 0; i < NUM_MBOXES, i++ ){
        regs.active_mboxes[i] = ( ( ( mbox_bitmap >> i ) & 1 ) == 1);
    }
}

void ZQM::handleRingDq( SST::Forza::ringEvent *ev )
{
    output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; handling a dequeue message\n", getName().c_str() );
    // Maybe use a ZEN like reading scheme for now to get something implemented
    // That would at least let us work on developing s/w
    auto regs = PerHartCSRs[ev->getZapId()][ev->getHartId()];
    auto mbox = regs.mbox_buffs[( ev->getCSR & R_MASK_ZQMDQMBOX )];

    if ( mbox.buff_state != msgBuffState::READY )
        output.fatal(CALL_INFO, -1, "[ZQM] %s; messager buffer not ready \n", getName().c_str() );

    // Get the data word to return
    auto ret_data = mbox.msg[mbox.msg_cur_word];

    // Use data field to do the proper read/write/update
    if ( ev->getData() == 0 ){
        if (msg_cur_word == 7) //sent last word; get next msg
            mbox.buff_state = msgBuffState::IDLE;
        else
            mbox.msg_cur_word++;
    } else if ( ev->getData() == 1 ) {
        mbox.buff_state = msgBuffState::IDLE;
    } else {
        output.fatal(CALL_INFO, -2, "[ZQM] %s; deque msg with unexpected data = %" PRIu64 "\n", getName().c_str(), ev->getData() );
    }

    sendRingResponse( ev, ret_data );
    delete ev;
}

void ZQM::sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data )
{
    output.fatal(CALL_INFO, -1, "[ZQM] %s function not yet implemented\n", getName().c_str());
#if 0
    auto resp = new ringEvent();
    // resp Hart = src Hart
    // resp CSR = src CSR
    // resp device = src device
    // resp cmd = src cmd
    // resp data = data argument

    // push response onto ring output (or structure that holds output events)
#endif
}

void ZQM::updateMailboxes()
{
    // Note (tdysart, 27-aug-24) - I expect a part of this code will disappear once we are putting the 
    // zops into memory rather than just as objects here in the zqm

    if ( IncomingZopQueue.empty() )
        return;

    auto msg = IncomingZopQueue.front();
    auto dest_aid = msg->getAppId();
    auto dest_mbox = msg->getCredit();

    // To match the zen encoding, the dest_logical_pe is an 11b field; bottom 9b are dest_hart in msg
    // upper 2b are lower 2b of dest_zcid in msg
    auto dest_pe = msg->getDestHart();
    uint8_t pe_top = ( (uint8_t)msg->getZCID() & 0x3 ) << 9;
    dest_pe |= pe_top;

    std::pair<uint8_t, uint16_t> logic_dest(dest_aid, dest_pe);
    auto iter = LogicalToPhysicalMap.find(logic_dest);
    if ( iter == LogicalToPhysicalMap.end() )
        output.fatal(CALL_INFO, -1, "[ZQM] %s incoming msg zop - no mapping found. aid=%u, logical_pe=%u\n", 
                     getName().c_str(), dest_aid, dest_pe );
    output.verbose(CALL_INFO, 9, 0, "[ZQM] %s incoming msg zop - mapping found. aid=%u, logical_pe=%u, zap=%u, hart=%u\n", 
                   getName().c_str(), dest_aid, dest_pe, iter->second.first, iter->second.second );

    auto mbox = PerHartCSRs[iter->second.first][iter->second.second].mbox_buffs[dest_mbox];
    IncomingZopQueue.pop();
    if ( mbox.buff_state == msgBuffState::IDLE ){
        // fill the msg buffer, set to ready
        mbox.setMsg(msg->getPayload());
        mbox.msg_cur_word = 1;
        mbox.buff_state = msgBuffState::READY;
        // send the ack zop
        sendMessagingAck(msg);
        // delete the zop
        delete msg;
    } else {
        IncomingZopQueue.push(msg);
    }
}

/** Handles incoming ZOP events; set as handler in ZQM::setup()
 *  Valid messages into here should be the following:
 * - Messaging with ZQM Setup Opcode - KILL
 * - RZA Response - Yes
 * - Thread Migration - May need to expand since this handles the run queue
 *
 * * @param event
*/

void ZQM::handleIncomingZOP(SST::Event *event)
{
    SST::Forza::zopEvent* ev = dynamic_cast<SST::Forza::zopEvent*>(event);
    ev->decodeEvent();
    output.verbose(CALL_INFO, 1, 0, "%s Received ZOP: %s to %s with id=%u\n",
                   my_name.c_str(),
                   ev->getSrcString().c_str(), 
                   ev->getDestString().c_str(),
                   ev->getID());

    if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received rza response - not currently handling\n", getName.c_str());
        //rza_responses.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG) {
        msg_zop_q.push(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received thread - not currently handling\n", getName.c_str());
        //incoming_threads_vec.push_back(ev);
    } else{
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received invalid ZOP; id=%u\n", 
                     getName.c_str(),
                     ev->getID());
        return;
    }
}

#if 0
void ZQM::sendThreadToRza(SST::Forza::zopEvent *thread)
{
    // Let's start by getting the state buffer entry for this AID
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());

    // Going to need to get an address to write
    uint64_t addr_ptr = aid_state->getMemAddr(false, true);
    if (addr_ptr == 0){
        //TODO: What's the workaround here?  Basically have to be able to
        // backpressure somewhere
        output.fatal(CALL_INFO, 1, "%s can't write thread to RZA\n", my_name.c_str());
    }

    // Create a new Zop (Store DMA type)
    SST::Forza::zopEvent *store_thread_zop = new SST::Forza::zopEvent(zopMsgT::Z_MZOP,
                                                                      zopOpc::Z_MZOP_SDMA);

    // Fill in Zop src/dest info
    store_thread_zop->setSrcZCID(zopCompID::Z_ZQM);
    store_thread_zop->setSrcPrec(PrecinctId);
    store_thread_zop->setSrcPCID(ZoneId);
    store_thread_zop->setDestZCID(zopCompID::Z_RZA);
    store_thread_zop->setDestPrec(PrecinctId);
    store_thread_zop->setDestPCID(ZoneId);
    store_thread_zop->setAppID(thread->getAppID());
    store_thread_zop->setID(msg_id++);

    // Create payload; Copy ENTIRE thread (header and payload) into ZOP
    std::vector<uint64_t> thread_packet = thread->getPacket();
    std::vector<uint64_t> zop_payload;
    zop_payload.push_back(0); // TODO: Fill in ACS
    zop_payload.push_back(addr_ptr);
    for (auto i : thread_packet)
        zop_payload.push_back(i);

    store_thread_zop->setPayload(zop_payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "%s: Sending SDMA Zop to RZA; msg_id=%u\n",
                   my_name.c_str(), (uint32_t)store_thread_zop->getID());
    zone_nic->send(store_thread_zop, zopCompID::Z_RZA);
    auto iter = outstanding_rza_reqs.find(store_thread_zop->getID());
    if (iter == outstanding_rza_reqs.end()) {
        outstanding_rza_reqs.insert(std::pair<uint8_t, std::pair<uint64_t,uint64_t>>(store_thread_zop->getID(),
                std::pair<uint64_t, uint64_t>(0, 0))); // TODO: Fix second pair
    } else {
        output.fatal(CALL_INFO, 1, "%s Duplicate msg_id going out to RZA\n", my_name.c_str());
    }
    aid_state->run_queue_depth++;

    // Delete thread
    delete thread;
}
#endif

#if 0
void ZQM::getThreadFromRza(uint32_t app_id)
{
    // Let's start by getting the state buffer entry for this AID
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(app_id);

    // Need an address to write
    uint64_t addr_ptr = aid_state->getMemAddr(true, true);
    if (addr_ptr == 0){
        output.verbose(CALL_INFO, 5, 0, "%s: No threads to read from RZA\n", my_name.c_str());
        return;
    }

    if (aid_state->run_queue_depth == 0)
        output.fatal(CALL_INFO, 1,
                     "%s: app_id.run_queue_depth = 0, pointers found valid read. rptr=0x%lx, wptr=0x%lx\n",
                     my_name.c_str(), aid_state->mem_read_ptr, aid_state->mem_write_ptr);

    // Create a new Zop (Load DMA type)
    SST::Forza::zopEvent *load_thread_zop = new SST::Forza::zopEvent(zopMsgT::Z_MZOP, zopOpc::Z_MZOP_LDMA);

    // Fill in Zop src/dest info
    load_thread_zop->setSrcZCID(zopCompID::Z_ZQM);
    load_thread_zop->setSrcPrec(PrecinctId);
    load_thread_zop->setSrcPCID(ZoneId);
    load_thread_zop->setDestZCID(zopCompID::Z_RZA);
    load_thread_zop->setDestPrec(PrecinctId);
    load_thread_zop->setDestPCID(ZoneId);
    load_thread_zop->setAppID(app_id);
    load_thread_zop->setID(msg_id++);

    // Zop Payload
    uint64_t load_acs = 0;
    std::vector<uint64_t> payload;// (load_acs, addr_ptr, aid_state->ThreadLengthDblWords);
    payload.push_back(load_acs);
    payload.push_back(addr_ptr);
    payload.push_back(ThreadLengthDblWords); //TODO: Ensure that this is the proper length
    load_thread_zop->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "%s: Sending LDMA Zop to RZA; msg_id=%u\n",
                   my_name.c_str(), (uint32_t)load_thread_zop->getID());
    zone_nic->send(load_thread_zop, zopCompID::Z_RZA);
    auto iter = outstanding_rza_reqs.find(load_thread_zop->getID());
    if (iter == outstanding_rza_reqs.end()){
        outstanding_rza_reqs.insert(std::pair<uint8_t,std::pair<uint64_t,uint64_t>>(load_thread_zop->getID(),
                std::pair<uint64_t, uint64_t>(0,0))); // TODO: Fix pair
        output.verbose(CALL_INFO, 1, 0, "%s: Inserted rzq_reqs key=%u\n", my_name.c_str(), load_thread_zop->getID());
    } else
        output.fatal(CALL_INFO, 1, "%s: Duplicate msg_id going out to RZA\n", my_name.c_str());

    aid_state->run_queue_depth--;
    aid_state->outstanding_fills++;
}
#endif

#if 0
void ZQM::prepSendRZAStore() {
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
    output.verbose(CALL_INFO, 9, 0, "[ZQM] Store messaging packet to RZA; wr_addr=0x%"
                   PRIx64 "\n", wr_addr);

    // Send memory zops
    std::vector<uint64_t> store_payload;
    store_payload.push_back(mbox_info->acs_pair);
    store_payload.push_back(wr_addr);
    ev->encodeEvent();
    std::vector<uint64_t> pkt = ev->getPacket();
    if (pkt.size() != 0){
        store_payload.insert(std::end(store_payload),
                             std::begin(pkt),
                             std::end(pkt));
    }
    if ( dma_enabled ) //dma_enabled set to true in constructor
      sendSdmaToRza(mbox_info, ev, store_payload, msg_ids[0], wr_addr);
    //else
    //  sendSdmaToRzaAsSequence(mbox_info, ev, store_payload, msg_ids, wr_addr);

    // Create MemReturnEntry and put into data struct to await the RZA return
    // TODO: This need substantial improvement if we're turning a store into a 
    // full sequence of ZOPs
    auto *mem_retentry = new MemReturnEntry(ev, msg_ids);
    rza_ret_wait_map.insert(std::pair<uint16_t, MemReturnEntry*>(msg_ids[0], mem_retentry));
    output.verbose(CALL_INFO, 9, 0, "[ZQM] Put entry into rza_ret_wait_map with id=%u\n", msg_ids[0]);

    // Finished with this packet
    to_rza_q.pop();
    if (to_rza_q.empty())
      break;
  }
}
#endif

#if 0
void ZQM::sendSdmaToRza(ZqmMailboxMetadata *mbox_info, SST::Forza::zopEvent *ev,
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
  output.verbose(CALL_INFO, 9, 0, "[ZQM] %s: Send StoreDMA to RZA msg_id=%" PRIu16 ", payload length=%" PRIu32 "\n",
                 getName().c_str(), msg_id, (uint32_t)store_payload.size());
  zone_nic->send(rzaMsg, zopCompID::Z_RZA);
}
#endif

#if 0
void ZQM::processRzaMsgs() {
    /* Assumes that the Zop.Type field has already been checked via handleIncomingZop */
    for (auto &resp: rza_responses) {
        uint16_t inc_msg_id = resp->getID();
        if (resp->getSrcZCID() <= (uint8_t)SST::Forza::zopCompID::Z_ZAP7){
            handleScratchpadAck(inc_msg_id);
        } else {
            /*
            output.fatal(CALL_INFO, -1, "RZA Resp %s to %s; ID=%u\n", 
                resp->getSrcString().c_str(),
                resp->getDestString().c_str(),
                inc_msg_id);
            */
            // Let's make sure the response was expected first....
            auto iter = rza_ret_wait_map.find(resp->getID());
            if (iter == rza_ret_wait_map.end()){
                output.fatal(CALL_INFO, 1, "%s: Received RZA response with an invalid ID; id=%u\n",
                            my_name.c_str(), (uint32_t) resp->getID());
            }

            // Should have 4 valid types
            switch (resp->getOpc()) {
                case zopOpc::Z_RESP_LR: { // valid data (should be a load dma response)
                    //output.verbose(CALL_INFO, 1, 0, "%s: Found msgId=%u (load response) in outstanding_rza_reqs map\n",
                    //            my_name.c_str(), (uint32_t) resp->getID());
                    //processRzaThreadDataReturn(resp);
                    output.fatal(CALL_INFO, -2, "unexpected load data return\n");
                    break;
                }
                case zopOpc::Z_RESP_LEXCP: // load exception
                    output.output("[ERROR] %s: Received a RZA load exception\n", my_name.c_str());
                    // TODO: Make fatal?
                    break;
                case zopOpc::Z_RESP_SACK: { // store ack (should be a store dma ack)
                    output.verbose(CALL_INFO, 9, 0, "[ZQM] Found msgId=%u (store ack) in rza_ret_wait_map map\n",
                                inc_msg_id);
                    auto *ev = iter->second->msg;
                    // Push zop so we can update the scratchpad
                    update_scratchpad_q.push(ev);
                    delete iter->second;
                    rza_ret_wait_map.erase(iter);
                    break;
                }
                case zopOpc::Z_RESP_SEXCP: // store exception
                    output.output("[ERROR] %s: Received a RZA store exception\n", my_name.c_str());
                    // TODO: Make fatal?
                    break;
                default:
                    output.fatal(CALL_INFO, 1, "%s: Received an invalid RZA response type; opcode=%u\n",
                                my_name.c_str(), (uint32_t) resp->getOpc());
            }
        }
        zoneMsgID->clearMsgId(inc_msg_id);
        delete resp;
    }
    rza_responses.clear();
}
#endif

// TODO: This needs further testing with actual data.
#if 0
void ZQM::processRzaThreadDataReturn(SST::Forza::zopEvent *ev)
{
    output.output("[WARNING] %s: need to fix thread repackaging in %s\n", my_name.c_str(), __func__);
    // Have to convert load data return to a thread zop
    SST::Forza::zopEvent *thread = new SST::Forza::zopEvent(zopMsgT::Z_TMIG, zopOpc::Z_TMIG_FIXED);
    std::vector rd_payload = ev->getPayload();
    thread->setPacket(rd_payload);
    thread->decodeEvent();

    // Get a destination HART & ship the thread
    if (!selectRandomDestHart(thread))
        output.fatal(CALL_INFO, 1, "%s: Returned thread didn't have a HART to go into...\n", my_name.c_str());
    sendThreadToZap(thread);
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
    aid_state->outstanding_fills--;
}
#endif

#if 0
void ZQM::sendThreadToZap(SST::Forza::zopEvent *thread)
{
    uint8_t dest_zap = thread->getDestZCID();
    uint16_t dest_hart = thread->getDestHart();
    if (zap_hart_status.at(dest_zap).at(dest_hart)){
        output.fatal(CALL_INFO, 1, "%s: TMIG Dest already occupied; ZAP=%u, HART=%u\n",
                     my_name.c_str(), (uint32_t) dest_zap, (uint32_t) dest_hart);
    } else {
        zap_hart_status.at(dest_zap).at(dest_hart) = true;
        ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
        aid_state->harts_available--;
    }
    output.verbose(CALL_INFO, 1, 0, "%s: Sending thread to ZAP=%u, HART=%u\n",
                   my_name.c_str(), dest_zap, dest_hart);
    zone_nic->send(thread, static_cast<SST::Forza::zopCompID>(dest_zap)); // TODO: UNCOMMENT IN FULL ZONE SIM
}
#endif

void ZQM::processMessagingMsgs()
{
    if (msg_zop_q.empty())
        return;

    for ( unsigned i = 0; i < process_per_cycle; i++ ) {
        auto *event = msg_zop_q.front();
        output.verbose(CALL_INFO, 1, 0, "%s: Processing messaging packet id=%u for ZQM\n", my_name.c_str(), event->getID() );
        switch(event->getOpc()){
            case SST::Forza::zopOpc::Z_MSG_ZQMSET:
                //processMessagingZqmSet(event);
                output.fatal(CALL_INFO, -2, "%s: Received an old ZQM setup message\n", my_name.c_str());
                // sendMessagingAck(event);
                break;
            case SST::Forza::zopOpc::Z_MSG_ZQMHARTDONE:
                processMessagingHartDone(event);
                //sendMessagingAck(event);
                break;
            case SST::Forza::zopOpc::Z_MSG_ZQMMBOXSET:
                output.fatal(CALL_INFO, -2, "%s: Received an old ZQM mailbox setup message\n", my_name.c_str());
                //processMessagingZqmMboxSet(event);
                break;
            case SST::Forza::zopOpc::Z_MSG_SENDP:{
                output.verbose(CALL_INFO, 9, 0, "ZQM RECV MSG_SENDP\n");
                //to_rza_q.push(event);
                IncomingZopQueue.push(event);
                break;}
                // TODO: Add ZQM Free AID (or equivalent)
                // TODO: Add ZQM Set HART (needed for initial program thread)
            default:
                output.fatal(CALL_INFO, -1, "%s: Received an invalid messaging packet; opcode = 0x%x, id=%u\n",
                             my_name.c_str(), (uint32_t) event->getOpc(), (uint32_t)event->getID());
        }
        msg_zop_q.pop();
        if ( msg_zop_q.empty() )
            break;
    }
}

void ZQM::processMessagingHartDone(SST::Forza::zopEvent *event)
{
    // No payload required; source information is sufficient
    uint8_t src_zap = event->getSrcZCID(); // this will be the zap
    uint16_t src_hart = event->getSrcHart();

    output.verbose(CALL_INFO, 1, 0, "[ERROR] ZQM [%s]: MsgHartDone does nothing, appID=%u, zap=%u, hart=%u\n",
                   my_name.c_str(), event->getAppID(), src_zap, src_hart);
#if 0
    if (zap_hart_status.at(src_zap).at(src_hart)) {
        zap_hart_status.at(src_zap).at(src_hart) = false;
        ZqmAidStateTableRow *aid_state = getAidStateTableRow(event->getAppID());
        aid_state->harts_available++;
    } else {
        output.fatal(CALL_INFO, 1, "%s: Received a HART done notification for an unused HART; ZAP=%u, HART=%u\n",
                     my_name.c_str(), (uint32_t) src_zap, (uint32_t) src_hart);
    }
#endif
    delete event;
}

#if 0
void ZQM::sendACK(SST::Forza::zopEvent *ev, bool to_zone_noc)
{
    SST::Forza::zopEvent *ack = new SST::Forza::zopEvent();
    ack->setType(SST::Forza::zopMsgT::Z_MSG);
    ack->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
    setMeAsZopSrc(ack);
    setDestFromSrcInfo(ack, ev);
    ack->setID(ev->getID());
    ack->encodeEvent();
    //auto iface = (to_zone_noc) ? zone_nic : m_prec_iface;
    auto iface = zone_nic;
    iface->send(ack, (zopCompID)ack->getDestZCID(), (zopPrecID)ack->getDestPCID(), (uint16_t)ack->getDestPrec());
    output.verbose(CALL_INFO, 9, 0, "[ZQM] %s sending ACK with msg_id=%" PRIu16 " to %s\n",
                    getName().c_str(), ev->getID(), ack->getDestString().c_str());
}
#endif

void ZQM::sendMessagingAck(SST::Forza::zopEvent *event)
{
    // Create Messaging ACK zop
    SST::Forza::zopEvent *ack_msg = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_ACK);

    // Set src/dest info
    setMeAsZopSrc(ack_msg);
    setDestFromSrcInfo(ack_msg, event);
    ack_msg->setID(event->getID());
    ack_msg->setAppID(event->getAppID());

    zone_nic->send(ack_msg, static_cast<zopCompID>(ack_msg->getDestZCID()));
    output.verbose(CALL_INFO, 9, 0, "[ZQM] %s sending ACK with msg_id=%" PRIu16 " to %s\n",
                    getName().c_str(), ev->getID(), ack->getDestString().c_str());
    // Caller is responsible for deleting the event
}

// Going to use early returns in this function - its ugly.
#if 0
void ZQM::processIncomingThreadsMsgs()
{
    // Sanity check
    if (outstanding_rza_reqs.size() == UINT8_MAX) {
        output.verbose(CALL_INFO, 1, 0, "%s: Too many outstanding RZA requests\n", my_name.c_str());
        return;
    }
    for (auto &thread : incoming_threads_vec){
        if (thread->getOpc() == zopOpc::Z_TMIG_FIXED){
            output.verbose(CALL_INFO, 1, 0, "%s: Handling TMIG_FIXED ZOP\n", my_name.c_str());
            // Always assumed to have the hart available, but the sendThreadToZap checks
            sendThreadToZap(thread);
        } else if (thread->getOpc() == zopOpc::Z_TMIG_SELECT){
            output.verbose(CALL_INFO, 1, 0, "%s: Handling TMIG_SELECT ZOP\n", my_name.c_str());
            ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
            if (aid_state->sequential_hart_assignment){
                selectSequentialDestHart(thread, aid_state);
                sendThreadToZap(thread);
            } else {
                // If we're pulling something from the RunQueue, this thread has to go there
                // to stay FIFO ordered
                if ( (aid_state->run_queue_depth > 0) || (aid_state->outstanding_fills > 0) ){
                    sendThreadToRza(thread);
                    return;
                }

                if (selectRandomDestHart(thread)) {
                    sendThreadToZap(thread);
                } else {
                    output.verbose(CALL_INFO, 1, 0, "%s: Failed to select a HART (all in use)\n", my_name.c_str());
                    sendThreadToRza(thread);
                }
            }
        } else {
            output.fatal(CALL_INFO, 1, "%s: Invalid TMIG Opcode=%u\n", my_name.c_str(), (uint32_t)thread->getOpc());
        }
    }
    incoming_threads_vec.clear();
}
#endif


#if 0
bool ZQM::selectRandomDestHart(SST::Forza::zopEvent *thread)
{
    // These should generally be migrating thread code...try to balance use of ZAPs...
    // Want to keep the logic sane so we can actually do it in verilog...however, we'll do the
    // optimal choice for now (for a given AID)
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
    std::vector<uint32_t> num_free_harts(zap_hart_status.size(), 0);

    // Number of free harts per zap for this AID
    for (size_t i = 0; i < zap_hart_status.size(); i++){
        for (auto j = aid_state->min_zap_hart; j <= aid_state->max_zap_hart; j++)
            num_free_harts[i] += (zap_hart_status[i][j]) ? 0 : 1;
    }

    // Check if all zaps are fully occupied/find lowest occupancy
    // There's probably a more c++-ish way of doing this
    uint16_t max_free_harts = 0;
    int max_zap = -1;
    for (size_t i = 0; i < num_free_harts.size(); i++){
        if (max_free_harts < num_free_harts[i]){
            max_free_harts = num_free_harts[i];
            max_zap = i;
        }
    }

    // If nothing free, return false
    if (max_zap == -1)
        return false;

    // Set destination HART to first available hart in zap we just found
    for (uint32_t i = aid_state->min_zap_hart; i <= aid_state->max_zap_hart; i++){
        if (!zap_hart_status[max_zap][i]){
            thread->setDestZCID(max_zap);
            thread->setDestHart(i);
            thread->setOpc(zopOpc::Z_TMIG_FIXED);
            break;
        }
    }
    return true;
}
#endif


/**
 * @param thread : zop being sent out to ZAP
 * @param aid_state : pointer to state info for this AppID
 *
 * I'm sure there are more efficient ways of doing this (and probably more that allow for better
 * error checking), however, this is operating on the following assumptions:
 * - actor based program
 * - threads arrive to ZQM in expected order
 * - threads are all created "at once" and then will all die "at once"
 */
#if 0
void ZQM::selectSequentialDestHart(SST::Forza::zopEvent *thread, ZqmAidStateTableRow *aid_state)
{
    for (unsigned i = 0; i < numCores; i++){
        for (uint32_t j = aid_state->min_zap_hart; j <= aid_state->max_zap_hart; j++){
            if (!zap_hart_status.at(i).at(j)){
                thread->setDestZCID(i);
                thread->setDestHart(j);
                thread->setOpc(zopOpc::Z_TMIG_FIXED);
                return;
            }
        }
    }
    // If I reach this, something bad has happened
    output.fatal(CALL_INFO, 1, "%s: Couldn't find an empty HART for AID=%u\n",
                 my_name.c_str(), thread->getAppID());
}
#endif

#if 0
void ZQM::fillEmptyHart()
{
    // Sanity check
    if (outstanding_rza_reqs.size() == UINT8_MAX)
        return;

    // TODO: Should this run less frequently? What are my rate limiters (probably the zopNIC)?
    // For all AIDs (or maybe just a simple round-robin?) see if there are any empty harts that can be filled
    for (auto &i : aid_state_table){
        ZqmAidStateTableRow *row = i.second;
        if ( (row->harts_available != 0) && (row->run_queue_depth != 0) )
            getThreadFromRza(i.first);
    }
}
#endif

bool ZQM::clock(Cycle_t cycle)
{
    // Ring related
    updateMailboxes();

    //fillEmptyHart();
    //processIncomingThreadsMsgs();
    //notifyHARTScratchpad();
    //processRzaMsgs();
    processMessagingMsgs();
    //prepSendRZAStore();


#if 0
    if ( (cycle % 100) == 0 ){
        output.verbose(CALL_INFO, 1, 0, "Clock cycles: %" PRIu64 ", Sim Cycles: %" PRIu64 ", Sim ns: %" PRIu64 "\n",
                cycle, getCurrentSimCycle(), getCurrentSimTimeNano());
    }
#endif

    return false;
}



// EOF
