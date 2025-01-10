//
// _zqm_cc_
//

#include "zqm.h"
#include "ZOPNET.h"
#include <RevMem.h>

using namespace SST::Forza;

static constexpr uint16_t ZAP_TO_HART_SHIFT = 9;

ZQM::ZQM(ComponentId_t id, Params& params)
        : Component(id)
{
    // Init the output handler
    const uint32_t Verbosity = params.find<uint32_t>("verbose", 7);
    output.init("ZQM[" + getName() + ":@p:@t]: ",
                Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  clockFreq = params.find<std::string>( "clockFreq", "1GHz" );

  // register the clock handler
  registerClock( clockFreq, new Clock::Handler<ZQM>( this, &ZQM::clock ) );
  output.verbose( CALL_INFO, 7, 0, "ZQM[%s] Registering clock with frequency=%s\n", getName().c_str(), clockFreq.c_str() );

  // The parameter finds below are using the same names as RevCPU.h
  numCores              = params.find<unsigned>( "numCores", 1 );
  numHarts              = params.find<uint16_t>( "numHarts", 1 );
  PrecinctId            = params.find<unsigned>( "precinctId", 0 );
  ZoneId                = params.find<unsigned>( "zoneId", 0 );
  processPerCycle       = params.find<unsigned>( "processPerCycle", 10 );
  msgQueueDepth         = params.find<uint32_t>( "msgQueueDepth", 512 );
  memStartAddr          = params.find<uint64_t>( "memStartAddr", 0x100000UL );
#if 1 // "Typical" values
  msgsPerMbox           = params.find<uint32_t>( "msgsPerMbox", 2 );
  cyclesPerRecycle      = params.find<uint32_t>( "cyclesPerRecycle", 1024 );
  recyclesToNack        = params.find<uint32_t>( "recyclesToNack", 64 );
#else // "Force nack" values
  msgsPerMbox           = params.find<uint32_t>( "msgsPerMbox", 1 );
  cyclesPerRecycle      = params.find<uint32_t>( "cyclesPerRecycle", 10 );
  recyclesToNack        = params.find<uint32_t>( "recyclesToNack", 1 );
#endif

  // Validate parameters
  if ( ( memStartAddr % 0x400 ) != 0 ) {
    output.fatal( CALL_INFO, -1, "ZQM: memStartAddr is not aligned to 0x400!\n" );
  }

  // set up the zone network
  zone_nic              = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
  zone_nic->setMsgHandler( new Event::Handler<ZQM>( this, &ZQM::handleIncomingZOP ) );
  zone_nic->setEndpointType( zopCompID::Z_ZQM );
  zone_nic->setNumHarts( numHarts );
  zone_nic->setPrecinctID( PrecinctId );
  zone_nic->setZoneID( ZoneId );

  zone_ring = loadUserSubComponent<SST::Forza::RingNetAPI>( "ring_nic" );
  if( zone_ring ) {
    output.verbose( CALL_INFO, 4, 0, "ZQM[%s] create zone ring\n", getName().c_str() );
    zone_ring->setMsgHandler( new Event::Handler<ZQM>( this, &ZQM::handleRingMsg ) );
    zone_ring->setEndpointType( zopCompID::Z_ZQM );
  } else {
    output.fatal( CALL_INFO, -1, "ZQM requires a zone ring network\n" );
  }
  dma_enabled = true;

  my_name     = "Precinct[" + std::to_string( PrecinctId ) + "].Zone[" + std::to_string( ZoneId ) + "].ZQM";
  output.verbose( CALL_INFO, 1, 0, "%s constructed\n", my_name.c_str() );

  zoneMsgID = new zopMsgID();
  if( zoneMsgID->getNumFree() != Z_MAX_MSG_IDS ) {
    output.fatal( CALL_INFO, -1, "Insufficient message IDs allocated in constructor\n" );
  }

  // Resize the IncomingMessage vector
  IncomingMsgQueues.resize( NUM_MBOXES );

  // Resize the CSR regs
  PerHartCSRs.resize( numCores );
  for( auto& i : PerHartCSRs ) {
    i.resize( numHarts );
  }

  // Init message buffer addresses
  uint64_t base_addr_increment = NUM_MBOXES * msgsPerMbox * ACTOR_MSG_BYTES;
  uint64_t base_addr = memStartAddr;
  for (auto &i : PerHartCSRs) {
    for (auto &j : i) {
      j.msgs_per_mbox = msgsPerMbox;
      j.mem_base_addr = base_addr;
      base_addr += base_addr_increment;
      for (auto &k : j.mbox_buff_state) {
        k.buff_state.resize( msgsPerMbox );
      }
    }
  }

  uint64_t total_msg_mem = numCores * numHarts * base_addr_increment;
  output.verbose(
    CALL_INFO, 3, 0, "%s constructed; using 0x%" PRIx64 " bytes for actor msg memory\n", my_name.c_str(), total_msg_mem
  );

  // Register stats
  MsgsRecd = registerStatistic<uint64_t>( "MsgsReceived" );
  AcksSent = registerStatistic<uint64_t>( "AcksSent" );
  NacksSent = registerStatistic<uint64_t>( "NacksSent" );
  NumRecycles = registerStatistic<uint64_t>( "NumRecycles" );

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

  // Recompute status
  uint64_t status = 0;
  auto &csr_buff_state = PerHartCSRs[ev->getSrcZap()][ev->getHart()].mbox_buff_state;
  for ( uint8_t i = 0; i < NUM_MBOXES; i++ ) {
    for ( auto &state : csr_buff_state.at(i).buff_state ) {
      if ( state == msgBuffState::READY ) {
        status |= ( 1UL << i );
        break;
      }
    }
  }

  output.verbose(CALL_INFO, 7, 0, "[ZQM] %s handle ZQMSTAT message; return status=0x%" PRIx64 "\n", getName().c_str(), status);
  sendRingResponse(ev, status);
}

void ZQM::handleRingMboxReg( SST::Forza::ringEvent *ev )
{
    // Writing non-zero configures a mapping
    // Writing 0 clears a mapping - ignore for now
    auto x = ev->getDatum();
    if ( x == 0 ){
        output.verbose(CALL_INFO, 5, 0, "[WARNING] [ZQM] %s; mailbox unregister not handled\n", getName().c_str() );
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

void ZQM::handleRingDq( SST::Forza::ringEvent* ev ) {
  output.verbose( CALL_INFO, 7, 0, "[ZQM] %s; handling a dequeue message\n", getName().c_str() );
  auto& regs = PerHartCSRs[ev->getSrcZap()][ev->getHart()];
  auto& mbox = regs.mbox_buff_state[( ev->getCSR() & R_MASK_ZQMDQMBOX )];

  if( mbox.getCurRdState() != msgBuffState::READY )
    output.fatal(
      CALL_INFO,
      -1,
      "[ZQM] %s; message buffer for [Zap:Hart:Mbox]=[%u:%u:%u] not ready \n",
      getName().c_str(),
      ev->getSrcZap(),
      ev->getHart(),
      ( ev->getCSR() & R_MASK_ZQMDQMBOX )
    );

  // Get the pointer for the current read buffer; HW uses ring level 3, segment 0xF for physical addressing to the
  // ZQM memory
  auto ret_data = regs.getMsgBuffAddr( ev->getCSR() & R_MASK_ZQMDQMBOX, mbox.cur_rd_entry );
  ret_data |= ( ( 0x0fUL & Z_SEG_MASK ) << Z_SEG_SHIFT );
  //output.verbose( CALL_INFO, 7, 0, "[ZQM] %s; handling a dequeue message; msg_ptr=0x%" PRIx64 "\n", getName().c_str(), ret_data );

  // If this was a read request, don't do anything else
  // If this is an update request, update the buffer state
  if( ev->getOp() == ringMsgT::R_UPDATE ) {
    ret_data                           = 0;
    mbox.buff_state[mbox.cur_rd_entry] = msgBuffState::IDLE;
    //output.verbose( CALL_INFO, 5, 0, "ZQM Deque[%u][%u].mbox[%u].buff[%u]\n",
    //  ev->getSrcZap(), ev->getHart(), ( ev->getCSR() & R_MASK_ZQMDQMBOX ), mbox.cur_rd_entry );
    mbox.updateRdEntry();
  }

  sendRingResponse( ev, ret_data );
}

void ZQM::sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data )
{
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
}

void ZQM::selectInMboxQueue()
{
  if ( num_incoming_msg_queues_ready == 0 )
    return;

  uint8_t winning_mbox = UINT8_MAX;
  for (uint8_t i = 0; i < NUM_MBOXES; i++) {
    if (IncomingMsgQueues[process_incoming_msg_queue].head_ready) {
      winning_mbox = process_incoming_msg_queue;
      updateProcessIncomingMsgQueue();
      break;
    }
    updateProcessIncomingMsgQueue();
  }

  if (winning_mbox == UINT8_MAX) {
    output.fatal( CALL_INFO, -1, "Did not find a ready mailbox\n" );
  }

  // Get the msg, update the MboxInQueue
  auto &in_mbox = IncomingMsgQueues[winning_mbox];
  auto msg = in_mbox.mbox_queue.front().first;
  in_mbox.mbox_queue.pop();
  in_mbox.head_ready = false;
  in_mbox.head_check_cycle_cntr = 0;
  // Per hart csr stuff
  if (msg->getMbxID() != winning_mbox) {
    output.fatal( CALL_INFO, -1, "ZQM: mboxId=%" PRIu8 "; does not match winner=%" PRIu8 "\n",
      msg->getMbxID(), winning_mbox );
  }
  auto  dest_mbox = msg->getMbxID(); // this should match winning_mbox - should probably check
  auto& mbox      = PerHartCSRs[msg->getDestZCID()][msg->getDestHart()].mbox_buff_state[dest_mbox];
  auto wr_addr = PerHartCSRs[msg->getDestZCID()][msg->getDestHart()].getMsgBuffAddr( dest_mbox, mbox.cur_wr_entry );

  //output.verbose(CALL_INFO, 5, 0, "Winner: [Z:H:MB:Buf]=[%u:%u:%u:%u]\n", msg->getDestZCID(), msg->getDestHart(), dest_mbox, mbox.cur_wr_entry );

  sendMsgToMemory( msg, wr_addr, mbox.cur_wr_entry );
  sendZopAck(msg, zopMsgT::Z_MSG, zopOpc::Z_MSG_ACK);
  num_incoming_msg_queues_ready--;
  delete msg;

  // Update CSR state
  mbox.setCurWrState( msgBuffState::FILLING );
  mbox.updateWrEntry();
}

void ZQM::updateInMboxQueue()
{
  auto& in_mbox = IncomingMsgQueues[cur_incoming_msg_queue];
  updateCurIncomingMsgQueue();

  if( in_mbox.mbox_queue.empty() || in_mbox.head_ready )
    return;

  auto qentry = in_mbox.mbox_queue.front();
  auto msg        = qentry.first;
  auto dest_mbox = msg->getMbxID();
  auto& mbox      = PerHartCSRs[msg->getDestZCID()][msg->getDestHart()].mbox_buff_state[dest_mbox];
  if (mbox.getCurWrState() == msgBuffState::IDLE) {
    in_mbox.head_ready = true;
    num_incoming_msg_queues_ready++;
  } else {
    in_mbox.head_check_cycle_cntr++;
    if (in_mbox.head_check_cycle_cntr == cyclesPerRecycle) {

      in_mbox.mbox_queue.pop();
      if (in_mbox.mbox_queue.front().second == recyclesToNack) {
        // NACK goes to where it came from; MUST have msg_id so the sender ZEN can retry
        sendZopAck( msg, zopMsgT::Z_MSG, zopOpc::Z_MSG_NACK );
        delete msg;
      } else {
        // Recycle this message
        qentry.second++;
        in_mbox.mbox_queue.push( qentry );
        in_mbox.head_check_cycle_cntr = 0;
        NumRecycles->addData( 1 );
        //output.verbose( CALL_INFO, 5, 0, "ZQM: recycle num=%" PRIu16 "; msg %s to %s, ID=%" PRIu16 "\n",
         // qentry.second, msg->getSrcString().c_str(), msg->getDestString().c_str(), msg->getID() );
      }
    } // no else needed; just had to increment the counter
  }
}

void ZQM::handleIncomingZOP(SST::Event *event)
{
    SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);
    ev->decodeEvent();
#if 0
    output.verbose(CALL_INFO, 7, 0, "%s Received ZOP: %s to %s with id=%u\n",
                   my_name.c_str(),
                   ev->getSrcString().c_str(), 
                   ev->getDestString().c_str(),
                   ev->getID());
    output.flush();
#endif
    if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
        rza_response_q.push(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG) {
        msg_zop_q.push(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received Z_TMIG - not currently handling\n", getName().c_str());
        tmig_zop_q.push(ev);
    } else{
        output.fatal(CALL_INFO, -2, "ZQM [%s]: Received invalid ZOP; id=%u\n", 
                     getName().c_str(),
                     ev->getID());
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

void ZQM::sendMsgToMemory( SST::Forza::zopEvent* ev, uint64_t wr_addr, uint8_t wr_entry )
{
  auto msg_id = zoneMsgID->getMsgId();
  if ( msg_id == Z_MAX_MSG_IDS ) {
    output.fatal( CALL_INFO, -1, "[ZQM] Invalid message id\n" );
    // Note, if this starts to show up, we can move this to before we call this function and
    // just skip trying to send if we can't get a msg id
  }

  auto *rzaMsg = new SST::Forza::zopEvent();
  // Set packet header info
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SDMA);
  // Need to set DestHart to be a combo of physical hart and zap
  uint16_t dest_hart = ( static_cast<uint16_t>( ev->getDestZCID() ) << ZAP_TO_HART_SHIFT ) + ev->getDestHart();
  rzaMsg->setFullSrc( dest_hart, zopCompID::Z_ZQM, rzaMsg->getPCID( ZoneId ), PrecinctId );
  setLocalRzaAsZopDest(rzaMsg);
  rzaMsg->setID(msg_id);
  rzaMsg->setAppID(ev->getAppID());
  rzaMsg->setResZero( wr_entry );
  rzaMsg->setAddr( wr_addr );
  rzaMsg->setPayload(ev->getPayload());
  rzaMsg->setMboxID( ev->getMbxID() );
  rzaMsg->encodeEvent();
  output.verbose(CALL_INFO, 9, 0, "[ZQM] %s: Send StoreDMA from ZoneId=%u with %s to %s to RZA1 msg_id=%" PRIu16 ", payload length=%" PRIu32 ", Addr=0x%" PRIx64 "\n",
                 getName().c_str(), ZoneId, rzaMsg->getSrcString().c_str(), rzaMsg->getDestString().c_str(),
                 msg_id, (uint32_t)ev->getPayload().size(), wr_addr );
  zone_nic->send(rzaMsg, zopCompID::Z_MSGRZA);
}

void ZQM::processRzaMsgs() {

  if ( rza_response_q.empty() )
    return;

  // Process a single one per clock tick
  auto resp = rza_response_q.front();
  rza_response_q.pop();

  output.verbose( CALL_INFO, 5, 0, "[ZQM] %s: Received RZA Ack from %s\n", getName().c_str(), resp->getSrcString().c_str() );

  if( resp->getSrcZCID() != RevCPU::safe_static_cast<uint8_t>( zopCompID::Z_MSGRZA ) ) {
    output.fatal( CALL_INFO, -1, "[ZQM] RZA response not from RZA1\n" );
  }

  switch( resp->getOpc() ) {
  case SST::Forza::zopOpc::Z_RESP_SACK: {
    auto  dest_mbox = resp->getMbxID();
    uint16_t dest_hart = resp->getDestHart() & 0x1FF;
    uint16_t dest_zap = ( resp->getDestHart() >> ZAP_TO_HART_SHIFT ) & 0x3;
    auto& mbox      = PerHartCSRs[dest_zap][dest_hart].mbox_buff_state[dest_mbox];
    //output.verbose(CALL_INFO, 5, 0, "WinnerResp: [Z:H:MB:Buf]=[%u:%u:%u:%u]\n", dest_zap, dest_hart, dest_mbox, resp->getResZero() );
    if( mbox.buff_state.at( resp->getResZero() ) != msgBuffState::FILLING ) {
      output.fatal( CALL_INFO, -1, "[ZQM] Buffer state not FILLING\n" );
    }
    mbox.buff_state.at( resp->getResZero() ) = msgBuffState::READY;
    break;
  }
  default:
    output.fatal(
      CALL_INFO, 1, "%s: Received an invalid RZA response type; opcode=%u\n", my_name.c_str(), (uint32_t) resp->getOpc()
    );
  }

  zoneMsgID->clearMsgId( resp->getID() );
  delete resp;

  // Keep the stuff below until either a) used/replaced or b) determined as not needed and safe to delete
#if 0
    switch( resp->getOpc() ) {
    case zopOpc::Z_RESP_LR: {  // valid data (should be a load dma response)
      //output.verbose(CALL_INFO, 1, 0, "%s: Found msgId=%u (load response) in outstanding_rza_reqs map\n",
      //            my_name.c_str(), (uint32_t) resp->getID());
      //processRzaThreadDataReturn(resp);
      output.fatal( CALL_INFO, -2, "unexpected load data return\n" );
      break;
    }
    case zopOpc::Z_RESP_LEXCP:  // load exception
      output.output( "[ERROR] %s: Received a RZA load exception\n", my_name.c_str() );
      // TODO: Make fatal?
      break;
    case zopOpc::Z_RESP_SEXCP:  // store exception
      output.output( "[ERROR] %s: Received a RZA store exception\n", my_name.c_str() );
      // TODO: Make fatal?
      break;
    }
#endif
}

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

void ZQM::convertLogicPEToPhysPE( zopEvent* msg ) {
  auto dest_aid = msg->getAppID();
  auto logical_pe = msg->getDestHart();

  std::pair logic_dest(dest_aid, logical_pe);
  auto iter = LogicalToPhysicalMap.find(logic_dest);
  if ( iter == LogicalToPhysicalMap.end() )
    output.fatal(CALL_INFO, -1, "[ZQM] %s incoming msg zop - no mapping found. aid=%" PRIu8 ", logical_pe=%" PRIu16 "\n",
                 getName().c_str(), dest_aid, logical_pe );

  output.verbose(CALL_INFO, 9, 0, "[ZQM] %s incoming msg zop - mapping found. aid=%" PRIu8 ", logical_pe=%" PRIu16 ", zap=%" PRIu8 ", hart=%" PRIu16 "\n",
                 getName().c_str(), dest_aid, logical_pe, iter->second.first, iter->second.second );

  // Update the zop to be the physical zap and hart
  msg->setDestZCID(iter->second.first);
  msg->setDestHart(iter->second.second);
}

void ZQM::processMessagingMsgs()
{
    if (msg_zop_q.empty())
        return;

    for ( unsigned i = 0; i < processPerCycle; i++ ) {
        auto *event = msg_zop_q.front();
        output.verbose(CALL_INFO, 9, 0, "%s: Processing messaging packet id=%u for ZQM\n", my_name.c_str(), event->getID() );
        switch(event->getOpc()){
            case zopOpc::Z_MSG_SENDP:{
                convertLogicPEToPhysPE(event);
                IncomingMsgQueues[event->getMbxID()].mbox_queue.push({event, 0});
                MsgsRecd->addData( 1 );
                break; }
            default:
                output.fatal(CALL_INFO, -1, "%s: Received an invalid messaging packet; opcode = 0x%x, id=%u\n",
                             my_name.c_str(), (uint32_t) event->getOpc(), (uint32_t)event->getID());
        }
        msg_zop_q.pop();
        if ( msg_zop_q.empty() )
            break;
    }
}

void ZQM::sendZopAck(SST::Forza::zopEvent *event, zopMsgT msg_type, zopOpc msg_opc)
{
    // Note: Caller handles what happens to event
    SST::Forza::zopEvent *ack_msg = new SST::Forza::zopEvent(msg_type, msg_opc);

    if ( msg_opc == zopOpc::Z_MSG_ACK )
      AcksSent->addData( 1 );
    else
      NacksSent->addData( 1 );

    // Set src/dest info
    setMeAsZopSrc(ack_msg);
    setDestFromSrcInfo(ack_msg, event);
    ack_msg->setID(event->getID());
    ack_msg->setAppID(event->getAppID());
    ack_msg->setMboxID(event->getMbxID());
    ack_msg->encodeEvent();

    zone_nic->send(ack_msg, static_cast<zopCompID>(ack_msg->getDestZCID()));
    std::string str = ack_msg->msgTToStr(msg_type);
    output.verbose(CALL_INFO, 9, 0, "ZQM [%s] sending (N)ACK %s:%u with msg_id=%" PRIu16 " from %s to %s\n",
                    getName().c_str(), str.c_str(), (uint8_t)msg_opc, event->getID(), ack_msg->getSrcString().c_str(), 
                    ack_msg->getDestString().c_str());
}

void ZQM::processTMigMsgs()
{
    if ( tmig_zop_q.empty() )
        return;

    for ( unsigned i = 0; i < processPerCycle; i++ ) {
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
    selectInMboxQueue();
    updateInMboxQueue();

    sendThreadToZap();
    //processIncomingThreadsMsgs();
    processRzaMsgs();
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
