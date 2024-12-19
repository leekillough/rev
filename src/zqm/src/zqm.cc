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
    const uint32_t Verbosity = params.find<uint32_t>("verbose", 7);
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
        zone_nic->setMsgHandler(new Event::Handler<ZQM>(this, &ZQM::handleIncomingZOP));
        zone_nic->setEndpointType(zopCompID::Z_ZQM);
        zone_nic->setNumHarts(numHarts);
        zone_nic->setPrecinctID(PrecinctId);
        zone_nic->setZoneID(ZoneId);
    }

    zone_ring = loadUserSubComponent<SST::Forza::RingNetAPI>( "ring_nic" );
    if (zone_ring) {
        output.verbose( CALL_INFO, 4, 0, "ZQM[%s] create zone ring\n", getName().c_str() );
        zone_ring->setMsgHandler( new Event::Handler<ZQM>(this, &ZQM::handleRingMsg) );
        zone_ring->setEndpointType(zopCompID::Z_ZQM);
    }
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
    //primaryComponentDoNotEndSim();
}

ZQM::~ZQM()
{
}

void ZQM::init(unsigned int phase) {
    output.verbose(CALL_INFO, 1, 0, "Init %s\n", my_name.c_str());
    if ( zone_nic )
        zone_nic->init(phase);
    if ( zone_ring )
        zone_ring->init(phase);
}

void ZQM::setup() {
    output.verbose(CALL_INFO, 1, 0, "Setup %s\n", my_name.c_str());
	if ( zone_nic )
        zone_nic->setup();
    if ( zone_ring )
        zone_ring->setup();
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

  if ( ev->getDestComp() != zopCompID::Z_ZQM ){
    int64_t next_addr = zone_ring->getNextAddress();
    zone_ring->send( ev, next_addr );
    output.verbose(CALL_INFO, 5, 0, "[ZQM] %s forwarding ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "; data=0x%" PRIx64 "\n", 
                   getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp(), ev->getDatum());
    return;
  }

  if ( ev->getSrcComp() == zopCompID::Z_ZQM ){
    output.fatal(CALL_INFO, -1, "[ZQM] %s unexpected ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n", getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp());
  }

  output.verbose(CALL_INFO, 5, 0, "[ZQM] %s handling ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n", getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp());
  output.flush();
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
  if ( ev->getOp() != SST::Forza::ringMsgT::R_READ )
    output.fatal(CALL_INFO, -1, "[ZQM] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto status = PerHartCSRs[ev->getSrcZap()][ev->getHart()].status;
  output.verbose(CALL_INFO, 7, 0, "[ZQM] %s handle ZQMSTAT message; return status=0x%" PRIx64 "\n", getName().c_str(), status);
  sendRingResponse(ev, status);
}

void ZQM::handleRingMboxReg( SST::Forza::ringEvent *ev )
{
    // Writing non-zero configures a mapping
    // Writing 0 clears a mapping - ignore for now
    auto x = ev->getDatum();
    if ( x == 0 ){
        output.verbose(CALL_INFO, 5, 0, "[ERROR] [ZQM] %s; mailbox unregister not handled\n", getName().c_str() );
        delete ev;
        return;
    }

    // But then we have to map that to some kind of state as to the status of the 
    // message buffers
    uint8_t aid = ( x >> ZQMMBOXREG_SHIFT_AID ) & ZQMMBOXREG_MASK_AID;
    uint8_t phys_zap = ( x >> ZQMMBOXREG_SHIFT_PHYSZAP ) & ZQMMBOXREG_MASK_PHYSZAP;
    uint16_t phys_hart = ( x >> ZQMMBOXREG_SHIFT_PHYSHART ) & ZQMMBOXREG_MASK_PHYSHART;
    uint16_t logic_pe = ( x >> ZQMMBOXREG_SHIFT_LOGICALPE ) & ZQMMBOXREG_MASK_LOGICALPE;
    uint8_t mbx_bitmap = ( x >> ZQMMBOXREG_SHIFT_MBXSUSED ) & ZQMMBOXREG_MASK_MBXSUSED;
    output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; handling a mailbox registration message; datum = 0x%" PRIx64 "\n", getName().c_str(), x );
    output.flush();

    std::pair<uint8_t, uint16_t> p1(aid, logic_pe);
    auto iter = LogicalToPhysicalMap.find( p1 );
    if (iter != LogicalToPhysicalMap.end() ){
        output.fatal(CALL_INFO, -1, "[ZQM] %s; received mailbox reg for an alredy registered mbox; aid=%u, logic_pe=%u\n",
                     getName().c_str(), aid, logic_pe );
    }
    std::pair<uint8_t, uint16_t> p2(phys_zap, phys_hart);
    LogicalToPhysicalMap.insert( std::pair<std::pair<uint8_t, uint16_t>, std::pair<uint8_t, uint16_t>>(p1, p2) );
    output.verbose(CALL_INFO, 7, 0, "[ZQM] A, phys_zap=%u, hart=%u\n", phys_zap, phys_hart);
    auto &regs = PerHartCSRs[phys_zap][phys_hart];
    output.verbose(CALL_INFO, 7, 0, "[ZQM] B, aid=%u, pe=%u=0x%x, mboxes=%u=0x%x\n", aid, logic_pe, logic_pe, mbx_bitmap, mbx_bitmap);
    regs.logical_pe = logic_pe;
    regs.aid = aid;
    for ( uint8_t i = 0; i < NUM_MBOXES; i++ ){
        regs.active_mboxes[i] = ( ( ( mbx_bitmap >> i ) & 1 ) == 1);
    }
    //output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; return from handling a mailbox registration message; datum = 0x%lx\n", getName().c_str(), x );
    //output.flush();
}

void ZQM::handleRingDq( SST::Forza::ringEvent *ev )
{
    output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; handling a dequeue message\n", getName().c_str() );
    // Maybe use a ZEN like reading scheme for now to get something implemented
    // That would at least let us work on developing s/w
    auto &regs = PerHartCSRs[ev->getSrcZap()][ev->getHart()];
    auto &mbox = regs.mbox_buffs[( ev->getCSR() & R_MASK_ZQMDQMBOX )];

    if ( mbox.buff_state != msgBuffState::READY )
        output.fatal(CALL_INFO, -1, "[ZQM] %s; messager buffer for [Zap:Hart:Mbox]=[%u:%u:%u] not ready \n", 
                    getName().c_str(), ev->getSrcZap(), ev->getHart(), (ev->getCSR() & R_MASK_ZQMDQMBOX) );

    // Get the data word to return
    auto ret_data = mbox.msg[mbox.msg_cur_word];
    output.verbose(CALL_INFO, 7, 0, "[ZQM] %s; handling a dequeue message; msg_cur_word=%u\n", getName().c_str(), mbox.msg_cur_word );

    // Use data field to do the proper read/write/update
    if ( ev->getDatum() == 0 ){
        if (mbox.msg_cur_word == 7){ //sent last word; get next msg
            mbox.buff_state = msgBuffState::IDLE;
            // Update status -- clear out the mailbox ready bit in the status
            uint64_t mbox_id = ( ev->getCSR() & R_MASK_ZQMDQMBOX );
            uint64_t mask = 1UL << mbox_id;
            uint64_t inv_mask = ~mask;
            regs.status &= inv_mask;
            output.verbose(CALL_INFO, 9, 0, "[ZQM] %s; handling a dequeue message; set state to IDLE\n", getName().c_str() );
        } else
            mbox.msg_cur_word++;
    } else if ( ev->getDatum() == 1 ) {
        mbox.buff_state = msgBuffState::IDLE;
        // Update status -- clear out the mailbox ready bit in the status
        uint64_t mbox_id = ( ev->getCSR() & R_MASK_ZQMDQMBOX );
        uint64_t mask = 1UL << mbox_id; // sets the ready bit
        uint64_t inv_mask = ~mask; // invert the mask
        regs.status &= inv_mask;
        output.verbose(CALL_INFO, 9, 0, "[ZQM] %s; handling a dequeue message; datum=1; set state to IDLE\n", getName().c_str() );
    } else {
        output.fatal(CALL_INFO, -2, "[ZQM] %s; deque msg with unexpected data = %" PRIu64 "\n", getName().c_str(), ev->getDatum() );
    }

    sendRingResponse( ev, ret_data );
}

void ZQM::sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data )
{
    //output.fatal(CALL_INFO, -1, "[ZQM] %s function not yet implemented\n", getName().c_str());
#if 1
    auto resp = new ringEvent(zopCompID::Z_ZQM, ev->getHart(), ev->getSrcComp(), ringMsgT::R_RETDATA, ev->getCSR(), data );
    int64_t next_dest = zone_ring->getNextAddress();
    output.verbose(
      CALL_INFO,
      5,
      0,
      "[ZQM] sending ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n",
      resp->getCSR(),
      (uint8_t) resp->getOp()
    );
    zone_ring->send( resp, next_dest );
#endif
}

void ZQM::updateMailboxes()
{
    // Note (tdysart, 27-aug-24) - I expect a part of this code will disappear once we are putting the 
    // zops into memory rather than just as objects here in the zqm

    if ( IncomingMsgQueue.empty() )
        return;

    auto msg = IncomingMsgQueue.front();
    IncomingMsgQueue.pop();
    auto dest_aid = msg->getAppID();
    auto dest_mbox = msg->getMbxID();

    // To match the zen encoding, the dest_logical_pe is an 11b field; bottom 9b are dest_hart in msg
    // upper 2b are lower 2b of dest_zcid in msg
    auto dest_pe = msg->getDestHart();
    uint8_t pe_top = ( (uint8_t)msg->getDestZCID() & 0x3 ) << 9;
    dest_pe |= pe_top;

    std::pair<uint8_t, uint16_t> logic_dest(dest_aid, dest_pe);
    auto iter = LogicalToPhysicalMap.find(logic_dest);
    if ( iter == LogicalToPhysicalMap.end() )
        output.fatal(CALL_INFO, -1, "[ZQM] %s incoming msg zop - no mapping found. aid=%u, logical_pe=%u\n", 
                     getName().c_str(), dest_aid, dest_pe );
    output.verbose(CALL_INFO, 9, 0, "[ZQM] %s incoming msg zop - mapping found. aid=%u, logical_pe=%u, mbox=%u, zap=%u, hart=%u\n", 
                   getName().c_str(), dest_aid, dest_pe, dest_mbox, iter->second.first, iter->second.second );

    auto &mbox = PerHartCSRs[iter->second.first][iter->second.second].mbox_buffs[dest_mbox];
    if ( mbox.buff_state == msgBuffState::IDLE ){
        // fill the msg buffer, set to ready
        mbox.setMsg(msg->getPayload());
        mbox.msg_cur_word = 0;
        mbox.buff_state = msgBuffState::READY;
        // send the ack zop
        sendZopAck(msg, zopMsgT::Z_MSG, zopOpc::Z_MSG_ACK);
        // delete the zop
        delete msg;
        //output.verbose( CALL_INFO, 9, 0, "Msg Buf sz=%zu\n", mbox.msg.size() );
        //for ( auto i : mbox.msg )
        //    output.verbose(CALL_INFO, 9, 0, "ZQM Msg = 0x%" PRIx64 "\n", i);
        //output.flush();
        // Update status
        PerHartCSRs[iter->second.first][iter->second.second].status |= (1UL << dest_mbox);
    } else {
        // rather than leave the msg at the front of the queue (where it's blocking), we recycle
        // it to the end of the queue
        IncomingMsgQueue.push(msg);
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
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received rza response - not currently handling\n", getName().c_str());
        //rza_responses.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG) {
        msg_zop_q.push(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received Z_TMIG - not currently handling\n", getName().c_str());
        tmig_zop_q.push(ev);
    } else{
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received invalid ZOP; id=%u\n", 
                     getName().c_str(),
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

void ZQM::processMessagingMsgs()
{
    if (msg_zop_q.empty())
        return;

    for ( unsigned i = 0; i < process_per_cycle; i++ ) {
        auto *event = msg_zop_q.front();
        output.verbose(CALL_INFO, 9, 0, "%s: Processing messaging packet id=%u for ZQM\n", my_name.c_str(), event->getID() );
        switch(event->getOpc()){
            case zopOpc::Z_MSG_SENDP:{
                IncomingMsgQueue.push(event);
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

void ZQM::sendZopAck(SST::Forza::zopEvent *event, zopMsgT msg_type, zopOpc msg_opc)
{
    // Note: Caller handles what happens to event
    // Create zop
    SST::Forza::zopEvent *ack_msg = new SST::Forza::zopEvent(msg_type, msg_opc);

    // Set src/dest info
    event->decodeEvent();
    setMeAsZopSrc(ack_msg);
    setDestFromSrcInfo(ack_msg, event);
    ack_msg->setID(event->getID());
    ack_msg->setAppID(event->getAppID());
    ack_msg->setMboxID(event->getMbxID());
    ack_msg->setID(event->getID());

    zone_nic->send(ack_msg, static_cast<zopCompID>(ack_msg->getDestZCID()));
    std::string str = ack_msg->msgTToStr(msg_type);
    output.verbose(CALL_INFO, 9, 0, "ZQM [%s] sending ack %s:%u with msg_id=%" PRIu16 " from %s to %s\n",
                    getName().c_str(), str.c_str(), (uint8_t)msg_opc, event->getID(), ack_msg->getSrcString().c_str(), 
                    ack_msg->getDestString().c_str());
}

void ZQM::processTMigMsgs()
{
    if ( tmig_zop_q.empty() )
        return;

    for ( unsigned i = 0; i < process_per_cycle; i++ ) {
        auto *event = tmig_zop_q.front();
        tmig_zop_q.pop();
        output.verbose(CALL_INFO, 7, 0, "ZQM [%s]: Processing tmig packet id=%u for ZQM\n", getName().c_str(), event->getID() );
        if ( event->getOpc() == SST::Forza::zopOpc::Z_TMIG_REQUEST ){
            AwaitingThreadsQueue.push( event->getSrcZCID() );
        } else { 
            RunQueue.push( event );
        }
        if ( tmig_zop_q.empty() )
            return;
    }
}

void ZQM::sendThreadToZap()
{
    // Need both an empty zap and a thread
    if ( ( AwaitingThreadsQueue.empty() ) || ( RunQueue.empty() ) )
        return;

    uint8_t dest_zap = AwaitingThreadsQueue.front();
    zopEvent *thread = RunQueue.front();
    AwaitingThreadsQueue.pop();
    RunQueue.pop();

    setMeAsZopSrc(thread);
    thread->setDestHart(0);
    thread->setDestZCID(dest_zap);
    thread->setDestPCID(ZoneId);
    thread->setDestPrec(PrecinctId);

    //TODO: Get zone message id - probably need it to make zopnet work
    // but then we have to handle acks (or figure out how else to clear)
    thread->encodeEvent();
    zone_nic->send(thread, zone_nic->getZCID(dest_zap, false) );
}

bool ZQM::clock(Cycle_t cycle)
{
    // Ring related
    updateMailboxes();

    sendThreadToZap();
    //processIncomingThreadsMsgs();
    //notifyHARTScratchpad();
    //processRzaMsgs();
    processMessagingMsgs();
    processTMigMsgs();

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
