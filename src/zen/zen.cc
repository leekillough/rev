//
// _zen_cc_
//

#include <sst/core/sst_config.h>
#include "zen.h"

namespace SST::Forza{

ZEN::ZEN(ComponentId_t id, Params& params)
  : Component(id)/*,
    zip_credits(_ZEN_DEFAULT_ZIP_CREDITS_)*/{

  // Init the output handler
  const int Verbosity = params.find<int>("verbose", 7);
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
  PerHartCSRs.resize( numZaps );
  for ( auto &i : PerHartCSRs ){
    i.resize( numHarts );
  }

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
    uint64_t next_addr = zone_ring->getNextAddress();
    zone_ring->send( ev, next_addr );
    output.verbose(CALL_INFO, 5, 0, "[ZEN] %s forwarding ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n", getName().c_str(), ev->getCSR(), (uint8_t)ev->getOp());
    return;
  }

  // Do some sanity checks
  // PerHartCSRs[ev->getSrcZap()][ev->getHart()]
#if 0
  if (ev->getSrcZap() >= m_num_zaps ) {
    output.fatal(CALL_INFO, -1, "ev with srcZap=%" PRIu8 "\n", ev->getSrcZap());
  }
  if (ev->getHart() >= m_num_harts ) {
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
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENOMC message\n", getName().c_str());
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
  output.verbose(CALL_INFO, 7, 0, "[ZEN] %s handle ZENSTAT message\n", getName().c_str());
  if ( ev->getOp() != SST::Forza::ringMsgT::R_READ )
    output.fatal(CALL_INFO, -1, "[ZEN] %s unexpected optype message; OpType=%" PRIu8 "\n", getName().c_str(), static_cast<uint8_t>(ev->getOp()));

  auto &status = PerHartCSRs[ev->getSrcZap()][ev->getHart()].status;
  status |= ( PerHartCSRs[ev->getSrcZap()][ev->getHart()].is_sending ) ? 0x0ffUL : 0;
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

  auto out_msg = new OutgoingMessage( regs.msg, ev->getSrcZap(), ev->getHart() );
  OutMsgQueue.push(out_msg);
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

void ZEN::sendRingResponse( SST::Forza::ringEvent *ev, uint64_t data )
{
    auto resp = new ringEvent(zopCompID::Z_ZEN, ev->getHart(), ev->getSrcComp(), ringMsgT::R_RETDATA, ev->getCSR(), data );
    uint64_t next_dest = zone_ring->getNextAddress();
    output.verbose(
      CALL_INFO,
      5,
      0,
      "[ZEN] sending ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "; data=0x%" PRIx64 "\n",
      resp->getCSR(),
      (uint8_t) resp->getOp(),
      data
    );
    zone_ring->send( resp, next_dest );
}

uint32_t ZEN::getRetrySeqNum()
{
  for (uint32_t i = 0; i < SeqNumMgrList.size(); i++){
    if (!SeqNumMgrList[i]){
      SeqNumMgrList[i] = true;
      return i;
    }
  }
  return UINT32_MAX;
} 

void ZEN::sendMsgZop( OutgoingMessage* msg, bool is_msg )
{
  auto *zop = new SST::Forza::zopEvent();
  // Set packet header info
  zop->setType(SST::Forza::zopMsgT::Z_MSG);
  zop->setOpc(SST::Forza::zopOpc::Z_MSG_SENDP);

  // Set source to be the sending hart
  zop->setSrcHart(msg->src_hart);
  zop->setSrcZCID(msg->src_zap);
  zop->setSrcPCID(ZoneId);
  zop->setSrcPrec(PrecinctId);

  auto ctrl_word = msg->msg[0];
  auto aid = ( ctrl_word >> ZENEQC_SHIFT_MSGAID ) & ZENEQC_MASK_MSGAID;
  zop->setAppID(aid);
  auto mbox_id = ( ctrl_word >> ZENEQC_SHIFT_DESTMBOX ) & ZENEQC_MASK_DESTMBOX;
  zop->setMboxID(mbox_id);
  //zop->setID(msg->msg_id); // redundant

  if ( is_msg ){
    // going to dest zone/precinct
    auto dest_logic_pe = ctrl_word & ZENEQC_MASK_DESTPE;
    auto dest_zone = ( ctrl_word >> ZENEQC_SHIFT_DESTZONE ) & ZENEQC_MASK_DESTZONE;
    auto dest_prec = ( ctrl_word >> ZENEQC_SHIFT_DESTPREC ) & ZENEQC_MASK_DESTPREC;

    // Need to divide the 11 bit dest logical PE to a 9 bit hart and 2 bit "zcid"
    // zopnet.cc should ensure all packets of this type are delivered to the zqm
    // so we can use this bit of hackery - provided we can reassemble properly
    zop->setDestHart( dest_logic_pe );
    zop->setDestZCID( zopCompID::Z_ZQM );
    zop->setDestPCID(dest_zone);
    zop->setDestPrec(dest_prec);
    zop->setID( msg->msg_id );

  } else {
    output.fatal( CALL_INFO, -1, "ZEN [%s] Not yet supported\n", getName().c_str() );
    //setLocalRzaAsZopDest(zop);
  }
  
  std::vector<uint64_t> payload;
  for (uint16_t i = 0; i < msg->msg.size(); i++){
    payload.push_back(msg->msg[i]);
    output.verbose( CALL_INFO, 5, 0, "Packet payload[%u] = 0x%" PRIx64 "\n", i, msg->msg[i] );
  }
  zop->setPayload(payload);
  zop->encodeEvent();
  output.flush();
  output.verbose(CALL_INFO, 5, 0, "ZEN[%s]: Send msg from %s to %s\n", getName().c_str(),
                 zop->getSrcString().c_str(), zop->getDestString().c_str());
  if ( (zop->getDestPrec() == PrecinctId ) && ( zop->getDestPCID() == ZoneId ) )
    zNic->send(zop, zopCompID::Z_ZQM);
  else {
    if (!precNic) {
      output.flush();
      output.fatal( CALL_INFO, -1, "ZEN[%s]: Packet needs unavailable precinct NoC\n");
    }
    precNic->send(zop, zopCompID::Z_ZQM, zop->getPCID( zop->getDestPCID() ), zop->getDestPrec());
  }
}

void ZEN::execMsgPipe2()
{
  if ( MsgPipeline[2] == nullptr )
    return;

  // This stage will put a message out onto either the zone_nic or precinct_nic depending 
  // on destination; zone_nic would send it to the local zqm - anything else goes to precinct nic
  auto ctrl_word = MsgPipeline[2]->msg[0];
  auto dest_zone = ( ctrl_word >> ZENEQC_SHIFT_DESTZONE ) & ZENEQC_MASK_DESTZONE;
  auto dest_prec = ( ctrl_word >> ZENEQC_SHIFT_DESTPREC ) & ZENEQC_MASK_DESTPREC;
  if ( (dest_prec == PrecinctId) && (dest_zone == ZoneId) ){
    // local zop -- get an id
    uint16_t zop_msg_id = zoneMsgID->getMsgId();
    if (zop_msg_id != Z_MAX_MSG_IDS) {
      sendMsgZop( MsgPipeline[2], true );
    } else {
      output.verbose(CALL_INFO, 7, 0, "[WARNING] ZEN[%s]; out of zone_msg_ids\n", getName().c_str());
      return;
    }
  } else {
    // put onto m_prec_iface;
    sendMsgZop( MsgPipeline[2], true );
    //output.fatal( CALL_INFO, -1, "ZEN[%s]; no send of msg to precinct nic implemented\n", getName().c_str() );
  }

  // Delete outgoing message and Update pipeline
  delete MsgPipeline[2];
  MsgPipeline[2] = nullptr;
}

void ZEN::handleMsgAck(zopEvent *ack)
{    
  ack->decodeEvent();
  // Reduce the mailbox counter
  auto &regs = PerHartCSRs[ack->getDestZCID()][ack->getDestHart()];
  auto &cntr = regs.mbox_cntrs[ack->getMbxID()];
  output.verbose(CALL_INFO, 9, 0, "ZEN[%s]; Counter=%u; packet %s to %s; credit=%u \n", getName().c_str(), cntr,
                 ack->getSrcString().c_str(), ack->getDestString().c_str(), ack->getMbxID());
  
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
  // Return the retry number to the list
  if ( SeqNumMgrList[retry_num] ) {
    SeqNumMgrList[retry_num] = false;
  } else {
    output.fatal(CALL_INFO, -3, "ZEN[%s]; SeqNumMgrList[%u] was false; Packet %s to %s \n", getName().c_str(), 
                 retry_num, ack->getSrcString().c_str(), ack->getDestString().c_str());
  }
  
  // Remove the entry from the RetryMgrMap
  auto num_deletes = RetryMgrMap.erase(retry_num);
  if (num_deletes != 1) {
    output.fatal(CALL_INFO, -2, "ZEN[%s]; Deleted %lu entries in the RetryMgrMap[%u]; Packet %s to %s \n", getName().c_str(), 
                 num_deletes, retry_num, ack->getSrcString().c_str(), ack->getDestString().c_str());
  }

  //output.verbose(CALL_INFO, 9, 0, "ZEN[%s]; Counter=%u; packet %s to %s; credit=%u \n", getName().c_str(), cntr,
  //               ack->getSrcString().c_str(), ack->getDestString().c_str(), ack->getCredit());

}

void ZEN::sendMsgToMemory( OutgoingMessage* out_msg )
{
  auto *rzaMsg = new SST::Forza::zopEvent();
  rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
  rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SDMA);
  rzaMsg->setFullSrc( 0, zopCompID::Z_ZEN, rzaMsg->getPCID( ZoneId ), PrecinctId );
  setLocalRzaAsZopDest(rzaMsg);
  rzaMsg->setID(out_msg->msg_id);
  rzaMsg->setAppID(0); // TODO: This comes from out_msg->msg.at(0)
  uint64_t wr_addr = getRetryBuffAddr( out_msg->msg_id );
  rzaMsg->setAddr( wr_addr );
  std::vector<uint64_t> mem_payload(out_msg->msg.begin(), out_msg->msg.end() );
  rzaMsg->setPayload(mem_payload );
  rzaMsg->encodeEvent();
  output.verbose(CALL_INFO, 9, 0, "[ZEN] %s: Send StoreDMA from ZoneId=%u with %s to %s to RZA1 msg_id=%" PRIu16 ", Addr=0x%" PRIx64 "\n",
                 getName().c_str(), ZoneId, rzaMsg->getSrcString().c_str(), rzaMsg->getDestString().c_str(),
                 out_msg->msg_id, wr_addr );
  zNic->send(rzaMsg, zopCompID::Z_RZA1);
}

void ZEN::execMsgPipe1()
{
  // If any acks have returned, clear the retry entry and update counter
  if ( !MsgAckQueue.empty() ) {
    auto ack = MsgAckQueue.front();
    MsgAckQueue.pop();
    handleMsgAck(ack);
    delete ack;
  }

  // Pipeline stalled
  if ( MsgPipeline[2] != nullptr )
    return;
  
  // Pipeline empty
  if ( MsgPipeline[1] == nullptr )
    return;

  // TODO: Do the work - send message to proper RZA
  sendMsgToMemory( MsgPipeline[1] );
  //output.verbose(CALL_INFO, 1, 0, "[WARNING] ZEN[%s]; no send of msg to RZA implemented\n", getName().c_str());

  // Insert message into retry mgr map
  RetryMgrMap[MsgPipeline[1]->msg_id] = MsgPipeline[1];

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
  uint32_t msg_id = getRetrySeqNum();
  if ( msg_id == UINT32_MAX )
    return;

  // Retry entry available; update that in the msg
  MsgPipeline[0]->msg_id = msg_id;
  output.verbose( CALL_INFO, 5, 0, "ZEN insert msg_id=%" PRIu32 "\n", msg_id );
  output.flush();
  // TODO: Fill in mem addr

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
  auto &regs = PerHartCSRs[MsgPipeline[0]->src_zap][MsgPipeline[0]->src_hart];
  regs.msg_cur_word = 1;
  regs.is_sending = false;
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

/* 
    TODO: Numerous cases that are not handled yet; will need to add them
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
  if (!isSrcLocal(ev)) {
    output.flush();
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
    case SST::Forza::zopOpc::Z_MSG_ACK:
      // if for this locale, clear the retry msg entry for this ack; push it onto the queue to handle
      // in the msg pipeline; if for diff locale, put it onto the precinct network
      if ( isDestLocal( ev ) ){
        output.verbose( CALL_INFO, 9, 0, "ZEN %s: received a MSG_ACK\n", getName().c_str() );
        MsgAckQueue.push(ev);
      } else {
        output.verbose( CALL_INFO, 9, 0, "ZEN %s: received a MSG_ACK; PUT ON PRECINCT NOC\n", getName().c_str() );
        precNic->send( ev, SST::Forza::zopCompID::Z_ZEN, ev->getPCID( ev->getDestPCID() ), ev->getDestPrec() );
      }
    break;

    case SST::Forza::zopOpc::Z_MSG_NACK:
      // Received a NACK, need to retry the message
      output.fatal(CALL_INFO, -1, "ZEN[%s]: received a intrazone msg nack packet - not yet implemented\n",
                 getName().c_str());
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

// Keep the two functions below until they (or equivalents) are in the ZQM
#if 0
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
  output.verbose(CALL_INFO, 9, 0, "ZEN %s sending ACK with msg_id=%" PRIu16 " to %s\n",
                 getName().c_str(), ev->getID(), ack->getDestString().c_str());
}
#endif

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

bool ZEN::clock(Cycle_t cycle){
  // new arch related
  ExecMsgPipeline();
  ExecSpawns();


  //handleIncomingRZAMsg();
  //prepSendRZAStore();
  return false;
}

} // namespace SST::Forza
// EOF
