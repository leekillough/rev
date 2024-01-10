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

static uint32_t selected_hart = 0xbeef;

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

    // Verify that we can do *something*
    if (do_read){
        if (mem_read_ptr == mem_write_ptr)
            return 0; // empty buffer
    } else {
        if (next_ptr >= mem_read_ptr)
            return 0; // no room to write
    }

    // Update proper ptr
    if (do_read)
        mem_read_ptr = next_ptr;
    else
        mem_write_ptr = next_ptr;

    return addr_ptr;
}

ZQM::ZQM(ComponentId_t id, Params& params)
        : Component(id)
{
    // Init the output handler
    const int Verbosity = params.find<int>("verbose", 7);
    output.init("ZQM[" + getName() + ":@p:@t]: ",
                Verbosity, 0, SST::Output::STDOUT);

    // read the remaining parameters
    clockFreq = params.find<std::string>("clockFreq", "1GHz");
    cycleCount = params.find<SST::Cycle_t>("clockTicks", "500");

    // register the clock handler
    registerClock(clockFreq, new Clock::Handler<ZQM>(this, &ZQM::clock));
    output.verbose(CALL_INFO, 9, 0, "ZQM[%s] Registering clock with frequency=%s\n",
                   getName().c_str(), clockFreq.c_str());

    m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "zone_nic" );
    m_zop_iface->setMsgHandler(new Event::Handler<ZQM>(this, &ZQM::handleIncomingZOP));
    m_zop_iface->setEndpointType(zopCompID::Z_ZQM);
    //m_linkControl = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 1 );
    // The parameter finds below are using the same names as RevCPU.h
    num_zaps = params.find<unsigned>("numCores", 1);
    num_harts = params.find<uint16_t>("numHarts", 4);
    precinct_id = params.find<unsigned>("precinctId", 0);
    zone_id = params.find<unsigned>("zoneId", 0);
    m_zop_iface->setNumHarts(num_harts);
    m_zop_iface->setPrecinctID(precinct_id);
    m_zop_iface->setZoneID(zone_id);

    output.verbose(CALL_INFO, 1, 0, "Precinct[%u].ZQM[%u]\n", precinct_id, zone_id);

    // Create and init matrix of HART status
    zap_hart_status.resize(num_zaps);
    for (auto &hart_vec: zap_hart_status)
      hart_vec.resize(num_harts, false);

    // TODO: What is int_id and do I need it?
    int_id = 0;
    msg_id = 0;
    sent = false;
    // register with SST
    registerAsPrimaryComponent();
}

ZQM::~ZQM()
{
}

void ZQM::init(unsigned int phase) {
    output.verbose(CALL_INFO, 1, 0, "Init id %lu\n", int_id);
    m_zop_iface->init(phase);
}

void ZQM::setup() {
    output.verbose(CALL_INFO, 1, 0, "Setup id %lu\n", int_id);
	m_zop_iface->setup();
}

void ZQM::complete(unsigned int phase) {
    return;
}

void ZQM::finish() {
    output.verbose(CALL_INFO, 1, 0, "Finish()\n");
}

/** Handles incoming ZOP events; set as handler in ZQM::setup()
 *    not sure where the events are coming in *from*; assumption is
 *    zone noc, but tbd
 *
 *  Valid messages into here should be the following:
 * - Messaging with ZQM Setup Opcode
 * - RZA Response
 * - Thread Migration
 *
 * * @param event
*/

void ZQM::handleIncomingZOP(SST::Event *event)
{
    output.output("Handle zop\n");	
    SST::Forza::zopEvent* ev = dynamic_cast<SST::Forza::zopEvent*>(event);
    ev->decodeEvent();
    output.verbose(CALL_INFO, 1, 0, "Msg type %u, opcode %u\n",
                   static_cast<uint8_t>(ev->getType()), 
		   static_cast<uint8_t>(ev->getOpc()));

    if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
        rza_responses.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG) {
        setup_reqs.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        incoming_threads_vec.push_back(ev);
    } else{
        //output.fatal(CALL_INFO, 1, "Received unexpected msg type = %u\n",
        //             (uint32_t) ev->getType());
        output.verbose(CALL_INFO, 1, 0, "Received unexpected msg type = %u, id=%u\n",
                       (uint32_t) ev->getType(), (uint32_t)ev->getID());
        //TODO: Is there a generic ZOP Dump/print function for debugging?  If so, use it
        return;
    }
}

ZqmAidStateTableRow* ZQM::getAidStateTableRow(uint32_t aid)
{
    auto iter = aid_state_table.find(aid);
    if (iter != aid_state_table.end())
        return iter->second;
    output.fatal(CALL_INFO, 1, "Received a zop with an unfound AID; AID=%u\n", aid);
    return nullptr;
}

void ZQM::sendThreadToRza(SST::Forza::zopEvent *thread)
{
    // Let's start by getting the state buffer entry for this AID
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());

    // Going to need to get an address to write
    uint64_t addr_ptr = aid_state->getMemAddr(false, true);
    if (addr_ptr == 0){
        //TODO: What's the workaround here?  Basically have to be able to
        // backpressure somewhere
        output.fatal(CALL_INFO, 1, "Can't write thread to RZA\n");
    }

    // Create a new Zop (Store DMA type)
    SST::Forza::zopEvent *store_thread_zop = new SST::Forza::zopEvent(zopMsgT::Z_MZOP, zopOpc::Z_MZOP_SDMA);

    // Fill in Zop src/dest info
    store_thread_zop->setSrcZCID(zopCompID::Z_ZQM);
    store_thread_zop->setSrcPrec(precinct_id);
    store_thread_zop->setSrcPCID(zone_id);
    store_thread_zop->setDestZCID(zopCompID::Z_RZA);
    store_thread_zop->setDestPrec(precinct_id);
    store_thread_zop->setDestPCID(zone_id);
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
    output.verbose(CALL_INFO, 1, 0, "Sending SDMA Zop to RZA; msg_id=%u\n", (uint32_t)store_thread_zop->getID());
    m_zop_iface->send(store_thread_zop, zopCompID::Z_RZA);
    auto iter = outstanding_rza_reqs.find(store_thread_zop->getID());
    if (iter == outstanding_rza_reqs.end()) {
        outstanding_rza_reqs.insert(std::pair<uint8_t, std::pair<uint64_t,uint64_t>>(store_thread_zop->getID(),
                std::pair<uint64_t, uint64_t>(0, 0))); // TODO: Fix second pair
    } else {
        output.fatal(CALL_INFO, 1, "Duplicate msg_id going out to RZA\n");
    }

    aid_state->run_queue_depth++;

    // Delete thread
    delete thread;
}

void ZQM::getThreadFromRza(uint32_t app_id)
{
    // Let's start by getting the state buffer entry for this AID
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(app_id);

    // Going to need to get an address to write
    uint64_t addr_ptr = aid_state->getMemAddr(true, true);
    if (addr_ptr == 0){
        output.verbose(CALL_INFO, 1, 0, "No threads to read from RZA\n");
        return;
    }

    if (aid_state->run_queue_depth == 0)
        output.fatal(CALL_INFO, 1, "app_id.run_queue_depth = 0, pointers found valid read. rptr=0x%lx, wptr=0x%lx\n",
                     aid_state->mem_read_ptr, aid_state->mem_write_ptr);

    // Create a new Zop (Load DMA type)
    SST::Forza::zopEvent *load_thread_zop = new SST::Forza::zopEvent(zopMsgT::Z_MZOP, zopOpc::Z_MZOP_LDMA);

    // Fill in Zop src/dest info
    load_thread_zop->setSrcZCID(zopCompID::Z_ZQM);
    load_thread_zop->setSrcPrec(precinct_id);
    load_thread_zop->setSrcPCID(zone_id);
    load_thread_zop->setDestZCID(zopCompID::Z_RZA);
    load_thread_zop->setDestPrec(precinct_id);
    load_thread_zop->setDestPCID(zone_id);
    load_thread_zop->setAppID(app_id);
    load_thread_zop->setID(msg_id++);

    // Zop Payload
    uint64_t load_acs = 0;
    std::vector<uint64_t> payload;// (load_acs, addr_ptr, aid_state->ThreadLengthDblWords);
    payload.push_back(load_acs);
    payload.push_back(addr_ptr);
    payload.push_back(aid_state->ThreadLengthDblWords);
    load_thread_zop->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "Sending LDMA Zop to RZA; msg_id=%u\n", (uint32_t)load_thread_zop->getID());
    m_zop_iface->send(load_thread_zop, zopCompID::Z_RZA);
    auto iter = outstanding_rza_reqs.find(load_thread_zop->getID());
    if (iter == outstanding_rza_reqs.end())
        outstanding_rza_reqs.insert(std::pair<uint8_t,std::pair<uint64_t,uint64_t>>(load_thread_zop->getID(),
                std::pair<uint64_t, uint64_t>(0,0))); // TODO: Fix pair
    else
        output.fatal(CALL_INFO, 1, "Duplicate msg_id going out to RZA\n");

    aid_state->run_queue_depth--;
    aid_state->outstanding_fills++;
}

void ZQM::processRzaMsgs() {
    /* Assumes that the Zop.Type field has already been checked via handleIncomingZop */
    for (auto &resp: rza_responses) {
        // Let's make sure the response was expected first....
        auto iter = outstanding_rza_reqs.find(resp->getID());
        if (iter == outstanding_rza_reqs.end()){
            output.fatal(CALL_INFO, 1, "Received RZA load response with an invalid ID; id=%u\n",
                        (uint32_t) resp->getID());
        }

        // Should have 4 valid types
        switch (resp->getOpc()) {
            case zopOpc::Z_RESP_LR: { // valid data (should be a load dma response)
                output.verbose(CALL_INFO, 1, 0, "Found msgId=%u (load response) in outstanding_rza_reqs map\n",
                               (uint32_t) resp->getID());
                processRzaThreadDataReturn(resp);
                break;
            }
            case zopOpc::Z_RESP_LEXCP: // load exception
                output.verbose(CALL_INFO, 1, 0, "[ZQM ERROR] Received a RZA load exception\n");
                // TODO: Make fatal?
                break;
            case zopOpc::Z_RESP_SACK: { // store ack (should be a store dma ack)
                output.verbose(CALL_INFO, 1, 0, "Found msgId=%u (store ack) in outstanding_rza_reqs map\n",
                                   (uint32_t) resp->getID());
                break;
            }
            case zopOpc::Z_RESP_SEXCP: // store exception
                output.verbose(CALL_INFO, 1, 0, "[ZQM ERROR]Received a RZA load exception\n");
                // TODO: Make fatal?
                break;
            default:
                output.fatal(CALL_INFO, 1, "Received an invalid RZA response type; type=%u\n",
                             (uint32_t) resp->getOpc());
        }
        outstanding_rza_reqs.erase(iter);
        delete resp;
    }
    rza_responses.clear();
}

void ZQM::processRzaThreadDataReturn(SST::Forza::zopEvent *ev)
{
    // Have to convert load data return to a thread zop
    SST::Forza::zopEvent *thread = new SST::Forza::zopEvent(zopMsgT::Z_TMIG, zopOpc::Z_TMIG_FIXED);
    std::vector rd_payload = ev->getPayload();
    thread->setPacket(rd_payload);
    // TODO: Necessary?
    thread->decodeEvent();

    // Get a destination HART & ship the thread
    if (!selectRandomDestHart(thread))
        output.fatal(CALL_INFO, 1, "Returned thread didn't have a HART to go into...\n");
    sendThreadToZap(thread);
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
    aid_state->outstanding_fills--;
    delete ev;
}

void ZQM::sendThreadToZap(SST::Forza::zopEvent *thread)
{
    uint8_t dest_zap = thread->getDestZCID();
    uint16_t dest_hart = thread->getDestHart();
    int32_t ha = 0;
    if (zap_hart_status.at(dest_zap).at(dest_hart)){
        output.fatal(CALL_INFO, 1, "TMIG Dest already occupied; ZAP=%u, HART=%u\n",
                     (uint32_t) dest_zap, (uint32_t) dest_hart);
    } else {
        zap_hart_status.at(dest_zap).at(dest_hart) = true;
        ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread->getAppID());
        aid_state->harts_available--;
        ha = aid_state->harts_available;
    }
    output.verbose(CALL_INFO, 1, 0, "Sending thread to ZAP=%u, HART=%u; ZAP harts avail=%d\n",
                   dest_zap, dest_hart, ha);
    selected_hart = (uint32_t)dest_hart;
    //m_zop_iface->send(thread, static_cast<SST::Forza::zopCompID>(dest_zap)); // TODO: UNCOMMENT IN FULL ZONE SIM
}

void ZQM::processMessagingMsgs()
{
    for (auto &event : setup_reqs) {
        output.verbose(CALL_INFO, 1, 0, "setup pkt for zqm\n");
        switch(event->getOpc()){
            case SST::Forza::zopOpc::Z_MSG_ZQMSET:
                processMessagingZqmSet(event); break;
            case SST::Forza::zopOpc::Z_MSG_ZQMHARTDONE:
                processMessagingHartDone(event); break;
                // TODO: Add ZQM Free AID (or equivalent)
                // TODO: Add ZQM Set HART (needed for initial program thread)
            default:
                //output.fatal(CALL_INFO, 1, "Received an expected zqm msg opcode = %u\n",
                //             (uint32_t)event->getOpc());
                output.verbose(CALL_INFO, 1, 0, "Received messaging packet; opcode = %x, id=%u\n",
                               (uint32_t) event->getOpc(), (uint32_t)event->getID());
        }
        delete event;
	output.output("Done with MSG type\n");
    }
    setup_reqs.clear();
}

void ZQM::processMessagingZqmSet(SST::Forza::zopEvent *event)
{
    uint32_t app_id = event->getAppID();
    std::vector<uint64_t> payload = event->getPayload(); // payload doesn't include the header
    uint64_t min_zap_hart = payload[0];
    uint64_t max_zap_hart = payload[1];
    uint64_t mem_buffer_low = payload[2];
    uint64_t mem_buffer_high = payload[3];
    bool seq_hart_assign = (payload[4] != 0) ? true : false;

    output.verbose(CALL_INFO, 1, 0, "Payload=0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx\n", min_zap_hart, max_zap_hart,
                   mem_buffer_low, mem_buffer_high, payload[4]);

    auto it = aid_state_table.find(app_id);
    if (it != aid_state_table.end()) {
        output.fatal(CALL_INFO, 1, "Received a second setup packet for aid=%u\n", app_id);
    }

    ZqmAidStateTableRow *aid_state_row = new ZqmAidStateTableRow(min_zap_hart,
                                                                 max_zap_hart,
                                                                 mem_buffer_low,
                                                                 mem_buffer_high,
                                                                 seq_hart_assign,
                                                                 num_zaps);
    if (!aid_state_row->validateMemBuffSize())
        output.fatal(CALL_INFO, 1, "Invalid memory buffer size for aid=%u, buff_low=%lu, buff_high=%lu\n",
                     app_id, mem_buffer_low, mem_buffer_high);
    aid_state_table.insert(std::pair<uint32_t, ZqmAidStateTableRow*>(app_id, aid_state_row));
    output.verbose(CALL_INFO, 1, 0, "Setup AID state table %u\n", app_id);
}

void ZQM::processMessagingHartDone(SST::Forza::zopEvent *event)
{
    // No payload required; source information is sufficient
    uint8_t src_zap = event->getSrcZCID(); // this will be the zap
    uint16_t src_hart = event->getSrcHart();

    output.verbose(CALL_INFO, 1, 0, "process SetupMsgHartDone, zap=%u, hart=%u\n",
                   src_zap, src_hart);

    if (zap_hart_status.at(src_zap).at(src_hart)) {
        zap_hart_status.at(src_zap).at(src_hart) = false;
        ZqmAidStateTableRow *aid_state = getAidStateTableRow(event->getAppID());
        aid_state->harts_available++;
    } else {
        output.fatal(CALL_INFO, 1, "Received a HART done notification for an unused HART; ZAP=%u, HART=%u\n",
                     (uint32_t) src_zap, (uint32_t) src_hart);
    }
}

// Going to use early returns in this function - its ugly.
void ZQM::processIncomingThreadsMsgs()
{
    // Sanity check
    if (outstanding_rza_reqs.size() == UINT8_MAX) {
        output.verbose(CALL_INFO, 1, 0, "Too many outstanding RZA requests\n");
        return;
    }
    for (auto &thread : incoming_threads_vec){
        if (thread->getOpc() == zopOpc::Z_TMIG_FIXED){
            // Always assumed to have the hart available, but the sendThreadToZap checks
            sendThreadToZap(thread);
        } else if (thread->getOpc() == zopOpc::Z_TMIG_SELECT){
            output.verbose(CALL_INFO, 1, 0, "Handling TMIG_SELECT ZOP\n");
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
                    output.verbose(CALL_INFO, 1, 0, "Failed to select a HART (all in use)\n");
                    sendThreadToRza(thread);
                }
            }
        } else {
            output.fatal(CALL_INFO, 1, "Invalid TMIG Opcode=%u\n", (uint32_t)thread->getOpc());
        }
    }
    incoming_threads_vec.clear();
}

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
void ZQM::selectSequentialDestHart(SST::Forza::zopEvent *thread, ZqmAidStateTableRow *aid_state)
{
    for (unsigned i = 0; i < num_zaps; i++){
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
    output.fatal(CALL_INFO, 1, "Couldn't find an empty HART for AID=%u\n", thread->getAppID());
}

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

void ZQM::doSimpleMsg()
{
#if 0
    // This was a successful test: Do a messaging packet with a non-ZQM opcode
    SST::Forza::zopEvent *dummy_zop1 = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_CREDIT);

    // Fill in Zop src/dest info
    dummy_zop1->setSrcZCID(zopCompID::Z_ZQM);
    dummy_zop1->setSrcPrec(precinct_id);
    dummy_zop1->setSrcPCID(zone_id);
    dummy_zop1->setDestZCID(zopCompID::Z_ZQM);
    dummy_zop1->setDestPrec(precinct_id);
    dummy_zop1->setDestPCID(zone_id);
    dummy_zop1->setAppID(0xc);
    dummy_zop1->setID(msg_id++);

    // Zop Payload
    std::vector<uint64_t> payload;// (load_acs, addr_ptr, aid_state->ThreadLengthDblWords);
    payload.push_back(0x11);
    payload.push_back(0x2002);
    payload.push_back(16);
    dummy_zop1->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "Sending dummy MSG type; msg_id=%u\n", (uint32_t)dummy_zop1->getID());
    m_zop_iface->send(dummy_zop1, zopCompID::Z_ZQM);
#endif

#if 1
    // Do a ZQM setup messaging packet
    SST::Forza::zopEvent *dummy_zop2 = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_ZQMSET);

    // Fill in Zop src/dest info
    dummy_zop2->setSrcZCID(zopCompID::Z_ZQM);
    dummy_zop2->setSrcPrec(precinct_id);
    dummy_zop2->setSrcPCID(zone_id);
    dummy_zop2->setDestZCID(zopCompID::Z_ZQM);
    dummy_zop2->setDestPrec(precinct_id);
    dummy_zop2->setDestPCID(zone_id);
    dummy_zop2->setAppID(0xa);
    dummy_zop2->setID(msg_id++);

    // Zop Payload
    std::vector<uint64_t> payload;
    payload.push_back(0x10); // min HART ID
    payload.push_back(0x11); // max HART id
    payload.push_back(0); // Mem buffer low
    payload.push_back((16*34*8)-1); // Mem buffer high
    payload.push_back(1); // sequential_hart_loading
    dummy_zop2->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "Sending ZQM SETUP MSG; msg_id=%u\n", (uint32_t)dummy_zop2->getID());
    m_zop_iface->send(dummy_zop2, zopCompID::Z_ZQM);
#endif
}

void ZQM::sendDummyThread()
{
    SST::Forza::zopEvent *thread = new SST::Forza::zopEvent(zopMsgT::Z_TMIG, zopOpc::Z_TMIG_SELECT);

    // Fill in Zop src/dest info
    thread->setSrcZCID(zopCompID::Z_ZAP1);
    thread->setSrcPrec(precinct_id);
    thread->setSrcPCID((uint8_t)zopPrecID::Z_ZONE6);
    thread->setDestZCID(zopCompID::Z_ZAP0);
    thread->setDestPrec(precinct_id);
    thread->setDestPCID(zone_id);
    thread->setAppID(0xa);
    thread->setID(msg_id++);

    std::vector<uint64_t> payload;
    payload.push_back(0x0dead);
    for (auto i = 0; i < 31; i++)
        payload.push_back(i);
    payload.push_back(0x0cafe);
    thread->setPayload(payload);
    output.verbose(CALL_INFO, 1, 0, "Sending MIGR THREAD; msg_id=%u\n", (uint32_t)thread->getID());
    m_zop_iface->send(thread, zopCompID::Z_ZQM);
}

void ZQM::sendHartDone()
{
    // Do a ZQM setup messaging packet
    SST::Forza::zopEvent *dummy_zop2 = new SST::Forza::zopEvent(zopMsgT::Z_MSG, zopOpc::Z_MSG_ZQMHARTDONE);

    // Fill in Zop src/dest info
    dummy_zop2->setSrcZCID(zopCompID::Z_ZAP0);
    dummy_zop2->setSrcPrec(precinct_id);
    dummy_zop2->setSrcPCID(zone_id);
    dummy_zop2->setSrcHart((uint16_t)selected_hart);
    dummy_zop2->setDestZCID(zopCompID::Z_ZQM);
    dummy_zop2->setDestPrec(precinct_id);
    dummy_zop2->setDestPCID(zone_id);
    dummy_zop2->setAppID(0xa);
    dummy_zop2->setID(msg_id++);

    // Set a payload?
    std::vector<uint64_t> payload;
    payload.push_back(0xdeadbeef);
    dummy_zop2->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "Sending ZQM HART DONE; msg_id=%u\n", (uint32_t)dummy_zop2->getID());
    m_zop_iface->send(dummy_zop2, zopCompID::Z_ZQM);
}

bool ZQM::clock(Cycle_t cycle)
{
   processIncomingThreadsMsgs();
   processRzaMsgs();
   processMessagingMsgs();
   fillEmptyHart();
#if 0
    if ( (cycle % 100) == 0 ){
        output.verbose(CALL_INFO, 1, 0, "Clock cycles: %" PRIu64 ", Sim Cycles: %" PRIu64 ", Sim ns: %" PRIu64 "\n",
                cycle, getCurrentSimCycle(), getCurrentSimTimeNano());
    }
#endif

    // Remove to allow for pushing into devel
#if 0
    if (cycle == 20){
        doSimpleMsg();
    }
    if ((cycle > 0) && (cycle % 1000 == 0) && (cycle < 10000)){
    //if (cycle == 1000 || cycle == 1100 || cycle == 4500){
        sendDummyThread();
    }

    //if (cycle == 2000){
    //    sendHartDone(); // really only want to do this if sending one a single dummy thread
    //}
#endif
    return false;
}

// EOF
