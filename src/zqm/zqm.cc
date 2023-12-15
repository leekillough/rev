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
        // TODO: Is there a generic ZOP Dump/print function for debugging?  If so, use it
        return;
    }
}

// TODO: UPDATE THIS!
void ZQM::sendMsgToRZA(uint64_t addr, uint8_t msg_id) {
    output.verbose(CALL_INFO, 1, 0, "Msg tgt %d, msg id %" PRIu8 "\n", addr, msg_id);
    std::vector<uint64_t> payload;
    // TODO: Update with RZA id
    SST::Forza::zopEvent *rzaMsg = new SST::Forza::zopEvent();
    rzaMsg->setType(SST::Forza::zopMsgT::Z_MZOP);
    rzaMsg->setID(msg_id);
    rzaMsg->setOpc(SST::Forza::zopOpc::Z_MZOP_SD);
    rzaMsg->setSrcHart(m_zop_iface->getAddress());
    rzaMsg->setSrcZCID(0);
    rzaMsg->setSrcPCID(0);
    rzaMsg->setSrcPrec(0);
    // TODO: Update with RZA id
    rzaMsg->setDestHart(3);
    payload.push_back(addr);
    // payload.push_back(value);
    rzaMsg->setPayload(payload);
    rzaMsg->encodeEvent();
    m_zop_iface->send(rzaMsg, zopCompID::Z_RZA);
    output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
    rzaMsg->decodeEvent();
    output.verbose(CALL_INFO, 1, 0, "msg id  %" PRIu8 ", header %lu\n", rzaMsg->getID(), rzaMsg->getPacket()[0]);
    // TODO: Update with RZA id
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

void ZQM::sendThreadToZap(SST::Forza::zopEvent *thread, uint8_t dest_zap, uint16_t dest_hart)
{
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
            // Always assumed to have the hart available
            sendThreadToZap(thread, thread->getDestZCID(), thread->getDestHart());
        } else if (thread->getOpcode() == zopOpc::Z_TMIG_SELECT){
            if (selectDestHart(thread)){
                sendThreadToZap(thread);
            } else {
                //TODO: Send Thread to Memory
            }
        }
    }
    incoming_threads_vec.clear();
}

bool ZQM::selectDestHart(SST::Forza::zopEvent *thread)
{
    // These should generally be migrating thread code...try to balance use of ZAPs...
    // Want to keep the logic sane so we can actually do it in verilog

    // TODO: Do I want any other values/structures to help track which ZAP should be
    // filled next...probably not.
}

bool ZQM::clock(Cycle_t cycle)
{
    processIncomingThreadsMsgs();
    processRzaMsgs();
    processMessagingMsgs();
    return false;
}

// EOF
