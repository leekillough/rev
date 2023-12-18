//
// _zqm_cc_
//

#include <sst/core/sst_config.h>
#include "zqm.h"
#include "ZOPNet.h" // TODO: Replace with version from forzarev

/**
 * NOTE: There are numerous places where the code uses extra variables, etc.
 * I don't care.  My goal is to make this easy to follow and ensure the spec matches since
 * it's being developed alongside the code.
 */

using namespace SST::Forza;

uint64_t ZqmAidStateTableRow::getMemAddr(bool do_read, bool update_ptr)
{
    uint64_t addr_ptr = (do_read) ? mem_read_ptr : mem_write_ptr;
    if (!update_ptr)
        return addr_ptr;

    // Below here, we do an update
    // First, determine the updated ptr
    uint64_t next_ptr = addr_ptr + ThreadLengthBytes;
    if (next_ptr == mem_buffer_high)
        next_ptr = mem_buffer_low;

    // Verify that we can do *something*
    if (do_read){
        if (mem_read_ptr == mem_write_ptr)
            return 0; // empty buffer
    } else {
        if (next_ptr == mem_read_ptr)
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
    const std::string cpuFreq = params.find<std::string>("clockFreq", "1GHz");
    int_id = params.find<uint64_t>("int_id", 0);

    // register the clock handler
    registerClock(cpuFreq, new Clock::Handler<ZQM>(this, &ZQM::clock));
    output.output("ZQM[%s] Registering clock with frequency=%s\n",
                  getName().c_str(), cpuFreq.c_str());

    m_zop_iface = loadUserSubComponent<SST::Forza::zopAPI>( "m_zop_iface" );
    //m_linkControl = loadUserSubComponent<SST::Interfaces::SimpleNetwork>( "rtrLink", ComponentInfo::SHARE_NONE, 1 );
    // The parameter finds below are using the same names as RevCPU.h
    num_zaps = params.find<unsigned>("numCores", 1);
    num_harts = params.find<uint16_t>("numHarts", 4);
    precinct_id = params.find<unsigned>("precinctId", 0);
    zone_id = params.find<unsigned>("zoneId", 0);

    // Create and init matrix of HART status
    zap_hart_status.resize(num_zaps);
    for (auto &hart_vec: zap_hart_status) {
        hart_vec.resize(m_num_harts);
        for (auto &j : hart_vec)
            j = false;
    }

    //assert( m_linkControl );
    msg_id = 0;
    sent = false;
    //mem_acks.push_back(0);
    //mem_acks.push_back(1);
    //mem_acks.push_back(2);
    //m_linkControl->setNotifyOnReceive( new SST::Interfaces::SimpleNetwork::Handler<ZQM>(this,&ZQM::handleNetworkEvent) );
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
    m_zop_iface->setMsgHandler(new Event::Handler<ZQM>(this, &ZQM::handleIncomingZOP));
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
 * @param event
 */
void ZQM::handleIncomingZOP(SST::Event *event) {
    SST::Forza::zopEvent* ev = dynamic_cast<SST::Forza::zopEvent*>(event);
    ev->decodeEvent();
    output.verbose(CALL_INFO, 1, 0, "Msg type %d, opcode %ld\n",
                   ev->getType(), ev->getOpcode());

    /** Valid messages into here should be the following:
     * - Messaging with ZQM Setup Opcode
     * - RZA Response
     * - Thread Migration
     */
    if (ev->getType() == SST::Forza::zopMsgT::Z_RESP) {
        rza_responses.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG) {
        setup_reqs.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        incoming_threads_vec.push_back(ev);
    } else{
        output.fatal(CALL_INFO, 1, "Received unexpected msg type = %u\n",
                     (uint32_t) ev->getType());
        //TODO: Is there a generic ZOP Dump/print function for debugging?  If so, use it
        return;
    }
}

// TODO: Just do the find?  When inserting, want a null table (which
// wouldn't return since it would throw the fatal error)
ZqmAidStateTableRow* ZQM::getAidStateTableRow(SST::Forza::zopEvent *zop)
{
    auto iter = aid_state_table.find(zop->getAppID());
    if (iter != aid_state_table.end())
        return iter.second;
    output.fatal(CALL_INFO, 1, "Received a zop with an unfound AID; AID=%u\n", aid);
    return nullptr;
}

ZqmAidStateTableRow* ZQM::getAidStateTableRow(uint32_t aid)
{
    auto iter = aid_state_table.find(aid));
    if (iter != aid_state_table.end())
        return iter.second;
    output.fatal(CALL_INFO, 1, "Received a zop with an unfound AID; AID=%u\n", aid);
    return nullptr;
}

void ZQM::sendThreadToRza(SST::Forza::zopEvent *thread)
{
    // Let's start by getting the state buffer entry for this AID
    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread);

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
    if (iter == outstanding_rza_reqs.end())
        outstanding_rza_reqs.insert(store_thread_zop->getID(), std::pair<uint64_t, uint64_t>(0,0)); // TODO: Fix pair
    else
        output.fatal(CALL_INFO, 1, "Duplicate msg_id going out to RZA\n");

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
        output.verbose(CALL_INFO, 1, 0, "No threads to read from RZA\n",);
        return;
    }

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

    // TODO: How do I decide on how many words to get back???  Where does that live?
    std::vector<uint64_t> payload;
    payload.push_back(addr_ptr);
    load_thread_zop->setPayload(payload);

    // Send Zop
    output.verbose(CALL_INFO, 1, 0, "Sending LDMA Zop to RZA; msg_id=%u\n", (uint32_t)load_thread_zop->getID());
    m_zop_iface->send(load_thread_zop, zopCompID::Z_RZA);
    auto iter = outstanding_rza_reqs.find(load_thread_zop->getID());
    if (iter == outstanding_rza_reqs.end())
        outstanding_rza_reqs.insert(load_thread_zop->getID(), std::pair<uint64_t, uint64_t>(0,0)); // TODO: Fix pair
    else
        output.fatal(CALL_INFO, 1, "Duplicate msg_id going out to RZA\n");
}

void ZQM::processRzaMsgs() {
    /**
     * Assume that the Zop.Type field has already been checked via handleIncomingZop()
     */
    for (auto &resp: rza_responses) {
        // Should have 4 valid types
        switch (resp->getOpcode()) {
            case zopOpc::Z_RESP_LR: { // valid data (should be a load dma response)
                sendThreadToZap(resp);
                auto it = outstanding_rza_reqs.find(resp->getID());
                if (it != outstanding_rza_reqs.end()) {
                    output.verbose(CALL_INFO, 1, 0, "Found msgId=%u (load response) in outstanding_rza_reqs map\n",
                                   (uint32_t) resp->getID());
                    outstanding_rza_reqs.erase(it);
                } else {
                    output.fatal(CALL_INFO, 1, "Received RZA load response with an invalid ID; id=%u\n",
                                 (uint32_t) resp->getID());
                }
                break;
            }
            case zopOpc::Z_RESP_LEXCP: // load exception
                output.verbose(CALL_INFO, 1, 0, "[ZQM ERROR] Received a RZA load exception\n");
                break;
            case zopOpc::Z_RESP_SACK: { // store ack (should be a store dma ack)
                auto it = outstanding_rza_reqs.find(resp->getID());
                if (it != outstanding_rza_reqs.end()) {
                    output.verbose(CALL_INFO, 1, 0, "Found msgId=%u (store ack) in outstanding_rza_reqs map\n",
                                   (uint32_t) resp->getID());
                    outstanding_rza_reqs.erase(it);
                } else {
                    output.fatal(CALL_INFO, 1, "Received RZA ack with an invalid ID; id=%u\n",
                                 (uint32_t) resp->getID());
                }
                break;
            }
            case zopOpc::Z_RESP_SEXCP: // store exception
                output.verbose(CALL_INFO, 1, 0, "[ZQM ERROR]Received a RZA load exception\n");
                break;
            default:
                output.fatal(CALL_INFO, 1, "Received an invalid RZA response type; type=%u\n",
                             (uint32_t) resp->getOpcode());
        }
        delete resp;
    }
}

void ZQM::sendThreadToZap(SST::Forza::zopEvent *thread)
{
    uint8_t dest_zap = thread->getDestZCID();
    uint16_t dest_hart = thread->getDestHart();
    if (zap_hart_status.at(dest_zap).at(dest_hart)){
        output.fatal(CALL_INFO, 1, "TMIG Dest already occupied; ZAP=%u, HART=%u\n",
                     (uint32_t) zap, (uint32_t) hart);
    } else {
        zap_hart_status.at(dest_zap).at(dest_hart) = true;
    }

    m_zop_iface->send(thread, getZCID(dest_zap, false));
}

void ZQM::processMessagingMsgs()
{
    for (auto &event : setup_reqs) {
        output.verbose(CALL_INFO, 1, 0, "setup pkt for zqm\n");
        switch(event->getOpcode()){
            case SST::Forza::zopOpc::Z_MSG_ZQMSET:
                processSetupMsgSet(event); break;
            case SST::Forza::zopOpc::Z_MSG_ZQMHARTDONE:
                processSetupMsgHartDone(event); break;
                // TODO: Add ZQM Free AID (or equivalent)
                // TODO: Add ZQM Set HART (needed for initial program thread)
            default:
                output.fatal(CALL_INFO, 1, "Received an expected zqm msg opcode = %u\n",
                             (uint32_t)event->getOpcode());
        }
        delete event;
    }
}

void ZQM::processMessagingZqmSet(SST::Forza::zopEvent *event)
{
    uint32_t app_id = event->getAppID();
    std::vector<uint64_t> payload = event->getPayload(); // payload doesn't include the header
    uint64_t min_zap_hart = payload[0];
    uint64_t max_zap_hart = payload[1];
    uint64_t mem_buffer_low = payload[2];
    uint64_t mem_buffer_high = payload[3];

    auto it = aid_state_table.find(app_id);
    if (it != aid_state_table.end()) {
        output.fatal(CALL_INFO, 1, "Received a second setup packet for aid=%u\n", app_id);
    }
    aid_state_table.insert(std::pair<uint32_t, ZqmAidStateTableRow>(app_id,
                                                                    {min_zap_hart,
                                                                     max_zap_hart,
                                                                     mem_buffer_low,
                                                                     mem_buffer_high}));
    output.verbose(CALL_INFO, 1, 0, "setup aid state table %u\n", app_id);
}

void ZQM::processMessagingHartDone(SST::Forza::zopEvent *event)
{
    // No payload required; source information is sufficient
    output.verbose(CALL_INFO, 1, 0, "process SetupMsgHartDone\n");
    uint8_t src_zap = event->getSrcZCID(); // this will be the zap
    uint16_t src_hart = event->getSrcHart();

    if (zap_hart_status.at(src_zap).at(src_hart)) {
        zap_hart_status.at(src_zap).at(src_hart) = false;
    } else {
        output.fatal(CALL_INFO, 1, "Received a HART done notification for an unused HART; ZAP=%u, HART=%u\n",
                     (uint32_t) src_zap, (uint32_t) src_hart);
    }
}

void ZQM::processIncomingThreadsMsgs()
{
    for (auto &thread : incoming_threads_vec){
        if (thread->getOpcode() == zopOpc::Z_TMIG_FIXED){
            // Always assumed to have the hart available, but the sendThreadToZap checks
            sendThreadToZap(thread);
        } else if (thread->getOpcode() == zopOpc::Z_TMIG_SELECT){
            if (selectDestHart(thread)){
                sendThreadToZap(thread);
            } else {
                sendThreadToRza(thread);
            }
        }
    }
    incoming_threads_vec.clear();
}

bool ZQM::selectDestHart(SST::Forza::zopEvent *thread)
{
    // These should generally be migrating thread code...try to balance use of ZAPs...
    // Want to keep the logic sane so we can actually do it in verilog...however, we'll do the
    // optimal choice for now (for a given AID)

    ZqmAidStateTableRow *aid_state = getAidStateTableRow(thread);
    std::vector<uint32_t> num_free_harts(zap_hart_status.size(), 0);

    // Number of free harts per zap for this AID
    for (int i = 0; i < zap_hart_status.size(); i++){
	for (int j = aid_state->min_zap_hart; j <= aid_state->max_zap_hart; j++)
	    num_free_harts[i] += (zap_hart_status[i][j]) ? 0 : 1;
    }

    // Check if all zaps are fully occupied/find lowest occupancy
    // There's probably a more c++-ish way of doing this
    uint16_t max_free_harts = 0;
    int max_zap = -1;
    for (int i = 0; i < num_occupied_harts.size(); i++){
	if (max_free_harts < num_free_harts[i]){
	    max_free_harts = num_free_harts[i];
	    max_zap = i;
	}
    }

    // If nothing free, return false
    if (max_zap == -1)
	return false;

    // Set destination HART to first available hart in zap we just found
    for (int i = zap_hart_status[max_zap][aid_state->min_zap_hart];
	 i <= zap_hart_status[max_zap][aid_state->max_zap_hart];
	 i++){
	if (!zap_hart_status[max_zap][i]){
	    thread->setDestACID(max_zap);
	    thread->setDestHart(i);
	    thread->setOpc(zopOpc::Z_TMIG_FIXED);
	}
    }
    return true;
}

bool ZQM::clock(Cycle_t cycle)
{
    processIncomingThreadsMsgs();
    processRzaMsgs();
    processMessagingMsgs();
    return false;
}

// EOF
