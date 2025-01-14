//
// _zen_cc_
//

#include "zen.h"

#include "../zqm/src/zqm.h"

namespace SST::Forza{

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id)/*,
    zip_credits(_ZEN_DEFAULT_ZIP_CREDITS_)*/{

  // Init the output handler
  const uint32_t Verbosity = params.find<uint32_t>("verbose", 7);
  output.init("ZEN[" + getName() + ":@p:@t]: ",
              Verbosity, 0, SST::Output::STDOUT);

  // read the remaining parameters
  const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
  PrecinctId = params.find<unsigned>("precinctId", 0);
  ZoneId = params.find<unsigned>("zoneId", 0);
  numHarts = params.find<unsigned>("numHarts", 512);
  numZaps = params.find<unsigned>("numZaps", 4);
  numZones = params.find<unsigned>("numZones", 8);
  numPrecincts = params.find<unsigned>("numPrecincts", 4);
  dma_enabled = params.find<bool>("enableDMA", false);
  zen_queue_size_limit = params.find<uint64_t>("zenQSizeLimit", 100000);
  process_per_cycle = params.find<uint64_t>("processPerCycle", 100000);
  bool precinct_nic_enabled = params.find<bool>("enablePrecinctNIC", true);
  memStartAddr = params.find<uint64_t>("memStartAddr", 0x1000UL);

  output.output("ZEN[%s] Forcing enableDMA to true.\n", getName().c_str());
  dma_enabled = true;

  // register the clock handler
  registerClock(cpuFreq, new Clock::Handler<ZEN>(this, &ZEN::clock));
  output.output("ZEN[%s] Registering clock with frequency=%s\n",
                getName().c_str(), cpuFreq.c_str());

  // setup the zone network
    zNic = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
    zNic->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingZOP));
    zNic->setEndpointType(zopCompID::Z_ZEN);
    zNic->setNumHarts(numHarts);
    zNic->setPrecinctID(PrecinctId);
    zNic->setZoneID(ZoneId);

  // setup the precinct network
  if (precinct_nic_enabled) {
    precNic = loadUserSubComponent<SST::Forza::zopAPI>( "precinct_nic" );
    precNic->setMsgHandler(new Event::Handler<ZEN>(this, &ZEN::handleIncomingPrecZOP));
    precNic->setEndpointType(zopCompID::Z_ZEN);
    precNic->setNumHarts(numHarts);
    precNic->setPrecinctID(PrecinctId);
    precNic->setZoneID(ZoneId);
  }

  zoneMsgID = new zopMsgID();
  if( zoneMsgID->getNumFree() != Z_MAX_MSG_IDS ){
    output.fatal(CALL_INFO, -1,
                 "Insufficient message IDs allocated in constructor\n");
  }

  zone_ring = loadUserSubComponent<SST::Forza::RingNetAPI>( "ring_nic" );
  if (zone_ring){
    output.verbose( CALL_INFO, 4, 0, "ZEN[%s] create zone ring\n", getName().c_str() );
    zone_ring->setMsgHandler( new Event::Handler<ZEN>(this, &ZEN::handleRingMsg) );
    zone_ring->setEndpointType(zopCompID::Z_ZEN);
  }

  // Size internal data structures
  SeqNumMgrList.resize( ZenSeqNumMgrDepth );
  PerHartCSRs.resize( numZaps );
  for ( auto &i : PerHartCSRs ){
    i.resize( numHarts );
  }

  // Other configuration
  seqNumsAvail = ZenSeqNumMgrDepth;

  // Register Stats
  //ActorMsgsRecd = registerStatistic<uint64_t>("ActorMsgsReceived");
  //AcksRecd = registerStatistic<uint64_t>("AcksReceived");
  //NacksRecd = registerStatistic<uint64_t>("NacksReceived");

  // complete SST registration
  registerAsPrimaryComponent();
  //primaryComponentDoNotEndSim();
}

ZEN::~ZEN(){
    delete zoneMsgID;
}

void ZEN::init(unsigned int phase) {
  output.verbose(CALL_INFO, 1, 0, "ZEN ID %d\n", ZoneId);
  if ( zNic )
    zNic->init(phase);
  if ( precNic ) {
    precNic->init(phase);
  }
  if ( zone_ring )
    zone_ring->init(phase);
}

void ZEN::setup() {
  if ( zNic )
    zNic->setup();
  if ( precNic ) {
    precNic->setup();
  }
  if ( zone_ring )
    zone_ring->setup();
}

void ZEN::complete(unsigned int phase) {
  if ( zNic )
    zNic->complete(phase);
  if ( precNic ) {
    precNic->complete(phase);
  }
  if ( zone_ring )
    zone_ring->complete(phase);
}

void ZEN::finish() {
  output.verbose(CALL_INFO, 10, 0, "Finish()\n");
}

void ZEN::handleRingMsg( SST::Event *event )
{
  auto *ev = static_cast<ringEvent*>(event);
  if ( ev->getDestComp() != zopCompID::Z_ZEN ){
    int64_t next_addr = zone_ring->getNextAddress();
    zone_ring->send( ev, next_addr );
    output.verbose(CALL_INFO, 5, 0, "[ZEN] %s forwarding ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n", getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp());
    return;
  }

  // Do some sanity checks
  // PerHartCSRs[ev->getSrcZap()][ev->getHart()]
#if 0
  if (ev->getSrcZap() >= numZaps ) {
    output.fatal(CALL_INFO, -1, "ev with srcZap=%" PRIu8 "\n", ev->getSrcZap());
  }
  if (ev->getHart() >= numHarts ) {
    output.fatal(CALL_INFO, -1, "ev with Hart=%" PRIu16 "\n", ev->getHart());
  }
#endif

  if ( ev->getSrcComp() == zopCompID::Z_ZEN ){
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n", getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp());
  }

  switch( ev->getCSR() ){
    case R_ZENSTAT:
      handleRingStatus(ev);
      break;
    case R_ZENEQD:
      handleRingEqData(ev);
      break;
    case R_ZENEQC:
      handleRingEqCtrl(ev);
      break;
    case R_ZENOMC:
      handleRingOmc(ev);
      break;
    case R_ZENEQS:
      handleRingSpawn(ev);
      break;
    default:
      output.fatal(CALL_INFO, -2, "[ZEN] %s unexpected ring message; CSR=0x%" PRIx16 "\n", getName().c_str(), ev->getCSR());
      break;
  }
  delete ev;
}

void ZEN::handleRingOmc( SST::Forza::ringEvent *ev )
{
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENOMC (Outstanding Msg Cntr) message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_READ )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto &cnts = PerHartCSRs[ev->getSrcZap()][ev->getHart()].mbox_cntrs;
  uint64_t full_cnt = 0;
  for (uint64_t i = 0; i < NUM_MBOXES; i++){
    full_cnt |= ( ( (uint64_t)cnts[i] ) << i*8 );
    //output.verbose(CALL_INFO, 7, 0, "[ZEN] %s ZENOMC; fullcnt=0x%" PRIx64 "; cnt[]=0x%" PRIx8 "\n", getName().c_str(), full_cnt, cnts[i]);
  }

  sendRingResponse(ev, full_cnt);
}

void ZEN::handleRingStatus( SST::Forza::ringEvent *ev )
{
  // Currently, this only supports the mailbox busy and msg seq nums avail fields

  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENSTAT message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_READ )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto status = PerHartCSRs[ev->getSrcZap()][ev->getHart()].status;
  status |= ( PerHartCSRs[ev->getSrcZap()][ev->getHart()].is_sending ) ? 0x0ffUL : 0;
  uint64_t seq_num_avail = ( seqNumsAvail & ZENSTAT_MASK_SEQNUMAVAIL ) << ZENSTAT_SHIFT_SEQNUMAVAIL;
  status |= seq_num_avail;
  sendRingResponse(ev, status);
}

void ZEN::handleRingEqData( SST::Forza::ringEvent *ev )
{
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENEQData message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_UPDATE )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto &regs = PerHartCSRs[ev->getSrcZap()][ev->getHart()];
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s ZENEQData src[Zap:Hart]=[%u:%u], cur_wd=%u, data=0x%" PRIx64 "\n",
                 getName().c_str(), ev->getSrcZap(), ev->getHart(), regs.msg_cur_word, ev->getDatum());
  // sanity check
  if (regs.msg_cur_word >= 8)
    output.fatal(CALL_INFO, -2, "[ZEN] %s msg_cur_word exceeded max; [zap%u][hart%u].msg_cur_word=%u\n", 
                 getName().c_str(), ev->getSrcZap(), ev->getHart(), regs.msg_cur_word );
  regs.msg[regs.msg_cur_word] = ev->getDatum();
  regs.msg_cur_word++;

  // Update status
  regs.is_sending = true;

  // TODO: DOES THIS NEED A RING RESPONSE?
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s completed ZENEQData message\n", getName().c_str());
}

void ZEN::handleRingEqCtrl( SST::Forza::ringEvent *ev )
{
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENEQCtrl message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_UPDATE )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto &regs = PerHartCSRs[ev->getSrcZap()][ev->getHart()];
  regs.msg[0] = ev->getDatum();
  uint64_t msg_clr = ( ev->getDatum() >> ZENEQC_SHIFT_MSGCLR ) & ZENEQC_MASK_MSGCLR;
  if ( msg_clr ) {
    output.fatal(CALL_INFO, -2, "[ZEN] %s no support for msg clear yet; zap=%u, hart=%u, datum=%" PRIu64 "\n",
                 getName().c_str(), ev->getSrcZap(), ev->getHart(), ev->getDatum() );
  }
  //ActorMsgsRecd->addData( 1 );
  uint64_t dest_mbox = ( ev->getDatum() >> ZENEQC_SHIFT_DESTMBOX ) & ZENEQC_MASK_DESTMBOX;
  if ( regs.mbox_cntrs[dest_mbox] == UINT8_MAX )
    output.fatal(CALL_INFO, -2, "[ZEN] %s no support for saturated mbox counter yet; zap=%u, hart=%u, mbox=%" PRIu64 "\n",
                 getName().c_str(), ev->getSrcZap(), ev->getHart(), dest_mbox);
  else if ( regs.mbox_cntrs[dest_mbox] == (UINT8_MAX-1) )
    regs.status |= ( 1UL << dest_mbox ); //if we're about to saturate the mbox counter, we have to set the status bit
  regs.mbox_cntrs[dest_mbox]++;
  output.verbose(CALL_INFO, 5, 0, "[ZEN] %s ZENEqCtrl src[Zap:Hart]=[%u:%u], data=0x%" PRIx64 "\n",
                 getName().c_str(), ev->getSrcZap(), ev->getHart(), ev->getDatum());
  // TODO: DOES THIS NEED A RING RESPONSE?

  auto msg_zop = createMsgZop( regs.msg, ev );
  OutMsgQueue.push(msg_zop);
}

void ZEN::handleRingSpawn( SST::Forza::ringEvent *ev )
{
  // TODO: Verify the behavior of this function with Tina
  output.fatal(CALL_INFO, -1, "[ZEN] %s function not fully implemented yet\n", getName().c_str());

  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENEQSpawn message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_UPDATE )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto &regs = PerHartCSRs[ev->getSrcZap()][ev->getHart()];
  // TODO: Check on status before setting it?
  // TODO: Check on value of spawn_cur_word?
  regs.status |= (1UL << ZENSTAT_SHIFT_SPNBUSY);

  regs.spawn_thread[regs.spawn_cur_word] = ev->getDatum();
  regs.spawn_cur_word++;
  if (regs.spawn_cur_word == 2){
    auto out_spawn = new OutgoingSpawn( regs.spawn_thread, ev->getSrcZap(), ev->getHart() );
    OutSpawnQueue.push(out_spawn);
  }
  // TODO: DOES THIS NEED A RING RESPONSE?
}

void ZEN::sendRingResponse( SST::Forza::ringEvent* ev, uint64_t data ) {
  auto     resp      = new ringEvent( zopCompID::Z_ZEN, ev->getHart(), ev->getSrcComp(), ringMsgT::R_RETDATA, ev->getCSR(), data );
  uint64_t next_dest = zone_ring->getNextAddress();
  output.verbose(
    CALL_INFO,
    5,
    0,
    "[ZEN] sending ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "; data=0x%" PRIx64 "\n",
    resp->getCSR(),
    RevCPU::safe_static_cast<uint8_t>( resp->getOp() ),
    data
  );
  zone_ring->send( resp, next_dest );
}

uint16_t ZEN::getRetrySeqNum()
{
  for (uint16_t i = 0; i < SeqNumMgrList.size(); i++){
    if (!SeqNumMgrList[i].in_use){
      SeqNumMgrList[i].set();
      seqNumsAvail--;
      return i;
    }
  }
  return UINT16_MAX;
} 

void ZEN::execMsgPipe2() {
  if ( ( MsgPipeline[2] == nullptr ) && NackRdReqQueue.empty() )
    return;

  // This stage will put a message out onto either the zone_nic or precinct_nic depending 
  // on destination; should probably be handling those output nics through separate pipelines,
  // but maybe the next round of updates we can do that

  // Use pipe2_alternate to alternate between NackRdReqQueue and MgsPipeline[2]
  zopCompID dest_comp = zopCompID::Z_ZQM;
  zopEvent *zop = nullptr;
  if (pipe2_alternate) {
    if ( MsgPipeline[2] != nullptr ) {
      zop = MsgPipeline[2];
      MsgPipeline[2] = nullptr;
    } else if (!NackRdReqQueue.empty() ) {
      zop = NackRdReqQueue.front();
      NackRdReqQueue.pop();
      dest_comp = zopCompID::Z_MSGRZA;
    } else {
      output.fatal( CALL_INFO, -1, "Invalid code...\n" );
    }
  } else {
    // pipe2_alternate==false case
    if (!NackRdReqQueue.empty()) {
      zop = NackRdReqQueue.front();
      NackRdReqQueue.pop();
      dest_comp = zopCompID::Z_MSGRZA;
      pipe2_alternate = true;
    } else if ( MsgPipeline[2] != nullptr ) {
      zop = MsgPipeline[2];
      MsgPipeline[2] = nullptr;
    } else {
      output.fatal( CALL_INFO, -1, "Invalid code...\n" );
    }
  }

  if ( zop == nullptr )
    output.fatal( CALL_INFO, -1, "Invalid zop event\n" );
  auto dest_zone = zop->getDestPCID();
  auto dest_prec = zop->getDestPrec();
  output.verbose(CALL_INFO, 5, 0, "ZEN[%s]: Send msg from %s to %s; id=%" PRIu16 "\n", getName().c_str(),
               zop->getSrcString().c_str(), zop->getDestString().c_str(), zop->getID() );
  if ( (dest_prec == PrecinctId) && (dest_zone == ZoneId) ){
    zNic->send( zop, dest_comp);
  } else {
    precNic->send( zop, dest_comp, zop->getPCID( zop->getDestPCID() ), zop->getDestPrec() );
  }
}

void ZEN::handleMsgResp(zopEvent *ack)
{    
  ack->decodeEvent();

  if ( ack->getOpc() == zopOpc::Z_MSG_NACK ) {
    // Have to use the msg_id to generate a read address; wait on data return and then resend the msg zop
    // Does not rewrite memory
    //NacksRecd->addData( 1 );
    // Nominally, this would be a LDMA request with a single payload word of 8 (as a request of 8 words); however, the RZA doesn't
    // support that operation. Thus, no payload for now and we just do a single load
    auto zop = new SST::Forza::zopEvent();
    zop->setType(SST::Forza::zopMsgT::Z_MZOP);
    zop->setOpc(SST::Forza::zopOpc::Z_MZOP_LD);
    zop->setFullSrc( 0, zopCompID::Z_ZEN, zop->getPCID( ZoneId ), PrecinctId );
    setLocalRzaAsZopDest(zop);
    zop->setID( ack->getID() );
    zop->setAppID(ack->getAppID() );
    uint64_t wr_addr = getRetryBuffAddr( ack->getID() );
    zop->setAddr( wr_addr );
    //std::vector<uint64_t> payload;
    //payload.push_back( ACTOR_MSG_LENGTH );
    //zop->setPayload( payload );
    zop->encodeEvent();
    output.verbose(CALL_INFO, 9, 0, "[ZEN] %s: Send LoadDMA Req from ZoneId=%u with %s to %s to RZA1 msg_id=%" PRIu16 ", Addr=0x%" PRIx64 "\n",
                   getName().c_str(), ZoneId, zop->getSrcString().c_str(), zop->getDestString().c_str(),
                   zop->getID(), wr_addr );
    NackRdReqQueue.push( zop );
    SeqNumMgrList.at( ack->getID() ).outstanding_read = true;
    delete ack;
    return;
  }

  // Reduce the mailbox counter
  //AcksRecd->addData( 1 );
  auto &regs = PerHartCSRs[ack->getDestZCID()][ack->getDestHart()];
  auto &cntr = regs.mbox_cntrs[ack->getMbxID()];
  output.verbose(CALL_INFO, 9, 0, "ZEN[%s]; ACK received; MboxId=%u, Orig Counter=%u; packet %s to %s\n",
      getName().c_str(), ack->getMbxID(), cntr, ack->getSrcString().c_str(), ack->getDestString().c_str());
  
  // Sanity check
  if (cntr == 0){
    output.fatal(CALL_INFO, -1, "ZEN[%s]; Counter was zero; packet %s to %s \n", getName().c_str(), 
                 ack->getSrcString().c_str(), ack->getDestString().c_str());
  }
  cntr--;
  // we've reduced the counter (no longer saturated), so the busy bit for this mbox should be cleared (active sending is handled with the is_sending flag)
  uint64_t mask = ~( 1UL << ack->getMbxID() );
  regs.status &= mask;

  auto retry_num = ack->getID();
  if ( SeqNumMgrList[retry_num].in_use ) {
    if (SeqNumMgrList[retry_num].written) {
      // Return the retry number to the list
      SeqNumMgrList[retry_num].clear();
      seqNumsAvail++;
      output.verbose( CALL_INFO, 9, 0, "ZEN[%s]: Releasing SeqNum %" PRIu16 "\n", getName().c_str(), retry_num );
    } else {
      // Have to wait for rza ack before releasing this retry number
      SeqNumMgrList[retry_num].acked = true;
      output.verbose( CALL_INFO, 9, 0, "ZEN[%s]: SeqNum %" PRIu16 " ACKed, but awaiting RZA response\n", getName().c_str(), retry_num );
    }
  } else {
    output.flush();
    output.fatal(CALL_INFO, -3, "ZEN[%s]; SeqNumMgrList[%u].in_use was false; Packet %s to %s \n", getName().c_str(),
                 retry_num, ack->getSrcString().c_str(), ack->getDestString().c_str());
  }
  // Had a regular ack, need to delete it
  delete ack;
}

void ZEN::sendMsgToMemory( zopEvent* out_msg )
{
  auto *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SDMA);
  rzaMsg->setFullSrc( 0, zopCompID::Z_ZEN, rzaMsg->getPCID( ZoneId ), PrecinctId );
  setLocalRzaAsZopDest(rzaMsg);
  rzaMsg->setID( out_msg->getID() );
  rzaMsg->setAppID(out_msg->getAppID() );
  uint64_t wr_addr = getRetryBuffAddr( out_msg->getID() );
  rzaMsg->setAddr( wr_addr );
  rzaMsg->setPayload(out_msg->getPayload() );
  rzaMsg->encodeEvent();
  output.verbose(CALL_INFO, 5, 0, "[ZEN] %s: Send StoreDMA from ZoneId=%u with %s to %s to RZA1 msg_id=%" PRIu16 ", Addr=0x%" PRIx64 "\n",
                 getName().c_str(), ZoneId, rzaMsg->getSrcString().c_str(), rzaMsg->getDestString().c_str(),
                 out_msg->getID(), wr_addr );
  zNic->send(rzaMsg, zopCompID::Z_MSGRZA);
  MsgDataMap.insert( std::pair<uint32_t, std::vector<uint64_t>>( out_msg->getID(), out_msg->getPayload() ) );
}

void ZEN::execMsgPipe1()
{
  // If any acks have returned, clear the retry entry and update counter
  if ( !MsgAckQueue.empty() ) {
    auto ack = MsgAckQueue.front();
    MsgAckQueue.pop();
    handleMsgResp(ack);
  }

  // Pipeline stalled
  if ( MsgPipeline[2] != nullptr )
    return;
  
  // Pipeline empty
  if ( MsgPipeline[1] == nullptr )
    return;

  // Send message to RZA1
  sendMsgToMemory( MsgPipeline[1] );

  // Move down the pipe
  MsgPipeline[2] = MsgPipeline[1];
  MsgPipeline[1] = nullptr;
}

void ZEN::execMsgPipe0()
{
  // Pipeline stalled
  if ( MsgPipeline[1] != nullptr )
    return;
  
  // Pipeline empty
  if ( MsgPipeline[0] == nullptr )
    return;

  // See if we have any retry entries available
  uint16_t msg_id = getRetrySeqNum();
  if ( msg_id == UINT16_MAX )
    return;

  // Retry entry available; update that in the msg
  MsgPipeline[0]->setID( msg_id );
  SeqNumMgrList[msg_id].setMsgSrc( MsgPipeline[0]->getSrcZCID(), MsgPipeline[0]->getSrcHart() );
  output.verbose( CALL_INFO, 7, 0, "ZEN[%s]: Created zop msg packet with msg_id=%" PRIu16 "\n", getName().c_str(), msg_id );

  // Move down the pipe
  MsgPipeline[1] = MsgPipeline[0];
  MsgPipeline[0] = nullptr;
}

void ZEN::updateMsgPipe0()
{
  if ( MsgPipeline[0] != nullptr )
    return;

  if ( OutMsgQueue.empty() )
    return;
  
  MsgPipeline[0] = OutMsgQueue.front();
  OutMsgQueue.pop();

  // message into pipeline, we can reset the send buffers for this hart
  auto &regs = PerHartCSRs[MsgPipeline[0]->getSrcZCID()][MsgPipeline[0]->getSrcHart()];
  regs.allowNextMsg();
}

// I suspect there is a better way of doing this, but not real clear to me what it
// might be at this point
void ZEN::ExecMsgPipeline()
{
  // send msg to zone xbar
  execMsgPipe2();

  // if pipe[2]==nullptr, send retry to rza
  execMsgPipe1();

  // if pipe[1]==nullptr, get a retry seq num and mem addr
  execMsgPipe0();
  // update pipe[0]
  updateMsgPipe0();
}

void ZEN::ExecSpawns()
{
  if ( OutSpawnQueue.empty() )
    return;

  auto spawn = OutSpawnQueue.front();
  OutSpawnQueue.pop();

  auto zop = new SST::Forza::zopEvent();
  // Set packet header info
  zop->setType(SST::Forza::zopMsgT::Z_TMIG);
  zop->setOpc(SST::Forza::zopOpc::Z_TMIG_SPAWN);
  zop->setID(0);

  // Set source to be the sending hart
  zop->setSrcHart(spawn->src_hart);
  zop->setSrcZCID(spawn->src_zap);
  zop->setSrcPCID(ZoneId);
  zop->setSrcPrec(PrecinctId);

  zop->setAppID(spawn->aid); 

  // Is dest always the local ZQM? Seems that way
  setLocalZqmAsZopDest(zop);

  std::vector<uint64_t> payload;
  for (size_t i = 0; i < spawn->thread.size(); i++)
    payload.push_back(spawn->thread[i]);
  zop->setPayload(payload);
  zop->encodeEvent();
  output.verbose(CALL_INFO, 9, 0, "ZEN[%s]: Send spawned thread from %s to %s\n", getName().c_str(),
                 zop->getSrcString().c_str(), zop->getDestString().c_str());
  zNic->send(zop, zopCompID::Z_ZQM);
}


zopEvent* ZEN::createMsgZop( std::array<uint64_t, ACTOR_MSG_LENGTH> msg_data, ringEvent *ring_event ) {
  auto* zop = new SST::Forza::zopEvent();
  // Set packet header info
  zop->setType( SST::Forza::zopMsgT::Z_MSG );
  zop->setOpc( SST::Forza::zopOpc::Z_MSG_SENDP );

  // Set source to be the sending hart
  zop->setSrcHart( ring_event->getHart() );
  zop->setSrcZCID( ring_event->getSrcZap() );
  zop->setSrcPCID( ZoneId );
  zop->setSrcPrec( PrecinctId );

  auto ctrl_word = msg_data[0];
  auto aid       = ( ctrl_word >> ZENEQC_SHIFT_MSGAID ) & ZENEQC_MASK_MSGAID;
  zop->setAppID( aid );
  auto mbox_id = ( ctrl_word >> ZENEQC_SHIFT_DESTMBOX ) & ZENEQC_MASK_DESTMBOX;
  zop->setMboxID( mbox_id );

  // going to dest zone/precinct
  auto dest_logic_pe = ctrl_word & ZENEQC_MASK_DESTPE;
  auto dest_zone     = ( ctrl_word >> ZENEQC_SHIFT_DESTZONE ) & ZENEQC_MASK_DESTZONE;
  auto dest_prec     = ( ctrl_word >> ZENEQC_SHIFT_DESTPREC ) & ZENEQC_MASK_DESTPREC;

  // The destHart field is now up to 11b to allow for a logical pe to be sent
  zop->setDestHart( dest_logic_pe );
  zop->setDestZCID( zopCompID::Z_ZQM );
  zop->setDestPCID( dest_zone );
  zop->setDestPrec( dest_prec );
  zop->setID( UINT16_MAX );

  std::vector<uint64_t> payload;
  for( uint16_t i = 0; i < ACTOR_MSG_LENGTH; i++ ) {
    payload.push_back( msg_data[i] );
    //output.verbose( CALL_INFO, 5, 0, "Packet payload[%u] = 0x%" PRIx64 "\n", i, msg->msg[i] );
  }
  zop->setPayload( payload );
  zop->encodeEvent();
  return zop;
}

void ZEN::handleIncomingPrecZOP(SST::Event *event) {
  auto* ev = static_cast<zopEvent*>(event);
  ev->decodeEvent();

  output.verbose(CALL_INFO, 7, 0, "ZEN[%s] PrecinctNOC received zop from %s to %s\n",
                 getName().c_str(),
                 ev->getSrcString().c_str(),
                 ev->getDestString().c_str());

  if (!isDestLocal(ev))
    output.fatal(CALL_INFO, -1, "ZEN %s: received a packet from precinct NoC not for this zone.\n",
        getName().c_str());

  switch(ev->getType()){
    case SST::Forza::zopMsgT::Z_MSG:
      helper_handleMsgZop(ev);
      break;

    case SST::Forza::zopMsgT::Z_RESP:
      // RZA Response
      // Put onto zone NoC
      zNic->send( ev, zNic->getZCID( ev->getDestZCID(), false ) );
      break;

    //case SST::Forza::zopMsgT::Z_HZOPV: [[fallthrough]];
    //case SST::Forza::zopMsgT::Z_RZOP: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_MZOP: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPAC:
      // Memory zop type - forward on to zone NoC
      zNic->send( ev, zopCompID::Z_RZA );
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

void ZEN::handleIncomingZOP(SST::Event *event) {
  SST::Forza::zopEvent* ev = static_cast<SST::Forza::zopEvent*>(event);

  output.verbose(CALL_INFO, 7, 0, "[ZEN]: %s received msg_id=%u from %s to %s\n",
                 getName().c_str(),
                 ev->getID(),
                 ev->getSrcString().c_str(),
                 ev->getDestString().c_str());

  // Sanity check for incoming packets
  if (!isSrcLocal(ev)) {
    output.fatal(CALL_INFO, -1, "ZEN %s: received a packet from zone NoC with non-local source.\n",
        getName().c_str());
  }

  switch(ev->getType()){
    case SST::Forza::zopMsgT::Z_MSG:
      // Messaging type; should only be ack/nack
      helper_handleMsgZop(ev);
      break;

    case SST::Forza::zopMsgT::Z_RESP:
      RzaRespQueue.push(ev);
      break;

    //case SST::Forza::zopMsgT::Z_HZOPV: [[fallthrough]];
    //case SST::Forza::zopMsgT::Z_RZOP: [[fallthrough]];
  case SST::Forza::zopMsgT::Z_MZOP: [[fallthrough]];
    case SST::Forza::zopMsgT::Z_HZOPAC:
      // Memory zop type - should be strictly outgoing to precinct NoC
      if ( isDestLocal(ev) )
        output.fatal(CALL_INFO, -2, "ZEN %s: received a memory zop with local dest; %s to %s\n",
                     getName().c_str(), ev->getSrcString().c_str(), ev->getDestString().c_str() );
      // Put packet in outgoing queue
      precNic->send( ev, zopCompID::Z_RZA );
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

void ZEN::helper_handleMsgZop(SST::Forza::zopEvent *ev)
{
  // There are several of these that the ZEN needs to handle
  switch(ev->getOpc()){
    case SST::Forza::zopOpc::Z_MSG_NACK: [[fallthrough]];
    case SST::Forza::zopOpc::Z_MSG_ACK:
      // if for this locale, push it onto the queue to handle in the msg pipeline;
      // if for diff locale, put it onto the precinct network
      if ( isDestLocal( ev ) ){
        output.verbose( CALL_INFO, 9, 0, "ZEN %s: received a MSG_(N)ACK\n", getName().c_str() );
        MsgAckQueue.push(ev);
      } else {
        output.verbose( CALL_INFO, 9, 0, "ZEN %s: received a MSG_(N)ACK; PUT ON PRECINCT NOC\n", getName().c_str() );
        precNic->send( ev, SST::Forza::zopCompID::Z_ZEN, ev->getPCID( ev->getDestPCID() ), ev->getDestPrec() );
      }
    break;

    case SST::Forza::zopOpc::Z_MSG_SENDP:
      output.verbose( CALL_INFO, 9, 0, "ZEN %s: received a MSG_SENDP\n", getName().c_str() );
      if ( (ev->getSrcPrec() == PrecinctId ) && ( ev->getSrcPCID() == ZoneId ) )
        output.fatal(CALL_INFO, -1, "ZEN[%s]: received a messaging packet from this [Prec:Zone]=[%u:%u]; dest[%u:%u]; Packet %s to %s\n",
                     getName().c_str(), PrecinctId, ZoneId,
                     (unsigned)ev->getSrcPrec(), (unsigned)ev->getSrcPCID(),
                     ev->getSrcString().c_str(), ev->getDestString().c_str());
      zNic->send(ev, SST::Forza::zopCompID::Z_ZQM);
      break;

    default:
      output.fatal(CALL_INFO, -1, "ZEN[%s]: received an unexpected messaging packet opcode=%u; Packet: %s to %s\n\n",
                 getName().c_str(), (unsigned)ev->getOpc(), ev->getSrcString().c_str(), ev->getDestString().c_str());
      break;
  }
}

void ZEN::processIncomingRZAMsgs() {
  if ( RzaRespQueue.empty() )
    return;

  auto *resp = RzaRespQueue.front();
  RzaRespQueue.pop();

  // Ensure this is an rza1 response
  if ( resp->getSrcZCID() != RevCPU::safe_static_cast<uint8_t>( zopCompID::Z_MSGRZA ) ) {
    output.fatal( CALL_INFO, -1, "ZEN[%s]: Received RZA Response from not RZA1 %s to %s\n", getName().c_str(),
                   resp->getSrcString().c_str(), resp->getDestString().c_str() );
  }

  auto retry_num = resp->getID();
  switch( resp->getOpc() ) {
  case zopOpc::Z_RESP_SACK:
    if ( SeqNumMgrList[retry_num].in_use ) {
      if (SeqNumMgrList[retry_num].acked) {
        // Return the retry number to the list
        SeqNumMgrList[retry_num].clear();
        seqNumsAvail++;
        output.verbose( CALL_INFO, 9, 0, "ZEN[%s]: Releasing SeqNum %" PRIu16 "\n", getName().c_str(), retry_num );
      } else {
        // Have to wait for msg ack before releasing this retry number
        SeqNumMgrList[retry_num].written = true;
        output.verbose( CALL_INFO, 9, 0, "ZEN[%s]: SeqNum %" PRIu16 "; rza resp rec'd, awaiting ack\n", getName().c_str(), retry_num );
      }
    } else {
      output.fatal(CALL_INFO, -3, "ZEN[%s]; SeqNumMgrList[%u].in_use was false; Packet %s to %s \n", getName().c_str(),
                   retry_num, resp->getSrcString().c_str(), resp->getDestString().c_str());
    }
    break;

  case zopOpc::Z_RESP_LR: {
    auto* zop = new SST::Forza::zopEvent( zopMsgT::Z_MSG, zopOpc::Z_MSG_SENDP );

    // Set source to be the sending hart
    if ( !SeqNumMgrList[retry_num].in_use ) {
      output.fatal( CALL_INFO, -1, "ZEN[%s]: SeqNumMgrList[%" PRIu16 "] was false; packet %s to %s\n",
        getName().c_str(), retry_num, resp->getSrcString().c_str(), resp->getDestString().c_str() );
    }
    zop->setSrcHart( SeqNumMgrList[retry_num].src_hart );
    zop->setSrcZCID( SeqNumMgrList[retry_num].src_zap );
    zop->setSrcPCID( ZoneId );
    zop->setSrcPrec( PrecinctId );

    // Get payload
    auto iter = MsgDataMap.find( retry_num );
    if (iter == MsgDataMap.end()) {
      output.fatal( CALL_INFO, -1, "Did not find data packet\n");
    }
    auto payload = iter->second;

    if ( payload.empty() )
      output.fatal( CALL_INFO, -2, "Empty payload...\n" );
    auto ctrl_word = payload[0];
    auto aid       = ( ctrl_word >> ZENEQC_SHIFT_MSGAID ) & ZENEQC_MASK_MSGAID;
    zop->setAppID( aid );
    auto mbox_id = ( ctrl_word >> ZENEQC_SHIFT_DESTMBOX ) & ZENEQC_MASK_DESTMBOX;
    zop->setMboxID( mbox_id );

    // going to dest zone/precinct
    auto dest_logic_pe = ctrl_word & ZENEQC_MASK_DESTPE;
    auto dest_zone     = ( ctrl_word >> ZENEQC_SHIFT_DESTZONE ) & ZENEQC_MASK_DESTZONE;
    auto dest_prec     = ( ctrl_word >> ZENEQC_SHIFT_DESTPREC ) & ZENEQC_MASK_DESTPREC;

    // The destHart field is now up to 11b to allow for a logical pe to be sent
    zop->setDestHart( dest_logic_pe );
    zop->setDestZCID( zopCompID::Z_ZQM );
    zop->setDestPCID( dest_zone );
    zop->setDestPrec( dest_prec );
    zop->setID( retry_num );
    zop->setPayload( payload );
    zop->encodeEvent();

    output.verbose( CALL_INFO, 7, 0, "ZEN[%s]: Resending message zop from %s to %s with id=%" PRIu16 "\n",
      getName().c_str(), zop->getSrcString().c_str(), zop->getDestString().c_str(), retry_num );

    // Then we have to insert this into the MsgPipeline somewhere....but for now, just inject it into the network
    if ( (dest_prec == PrecinctId) && (dest_zone == ZoneId) ){
      zNic->send( zop, zopCompID::Z_ZQM) ;
    } else {
      if (!precNic) {
        output.fatal( CALL_INFO, -1, "ZEN[%s]: No precinct NIC\n", getName().c_str() );
      }
      precNic->send( zop, zopCompID::Z_ZQM, zop->getPCID( zop->getDestPCID() ), zop->getDestPrec() );
    }
  }
    break;

  default:
    output.flush();
    output.fatal(
      CALL_INFO,
      -1,
      "ZEN[%s]: Unexpected packet received %s to %s\n",
      getName().c_str(),
      resp->getSrcString().c_str(),
      resp->getDestString().c_str()
    );
  }

  delete resp;
}

bool ZEN::clock(Cycle_t cycle){
  // new arch related
  ExecMsgPipeline();
  ExecSpawns();
  processIncomingRZAMsgs();
  return false;
}

} // namespace SST::Forza
// EOF
