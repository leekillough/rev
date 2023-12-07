//
// _zqm_cc_
//

#include <sst/core/sst_config.h>
#include "zqm.h"
#include "ZOPNet.h" // TODO: Replace with version from forzarev

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
    m_num_harts = params.find<uint64_t>("num_harts", 4);
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
        rza_reqs.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_MSG && ev->getOpcode() == SST::Forza::zopOpc::Z_MSG_ZQMSET) {
        setup_reqs.push_back(ev);
    } else if (ev->getType() == SST::Forza::zopMsgT::Z_TMIG){
        incoming_threqds_vec.push_back(ev);
    } else{
        output.verbose(CALL_INFO, 1, 0, "Invalid msg type %d\n", ev->getType());
        // TODO: Is there a generic ZOP Dump/print function for debugging?  If so, use it
        return;
    }
}

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

void ZQM::processRzaMsgs()
{
    // This is supposed to look at the RZA msgs and start progressing status of whatever
    // got an ACK
    for (int i = 0; i < mem_acks.size(); ++i) {
        output.verbose(CALL_INFO, 1, 0, "progress status msg_id %lu\n", mem_acks[i]->getID());
        if (outstanding_mem_req.count(mem_acks[i]->getID())) {
            uint64_t hart_id = outstanding_mem_req[mem_acks[i]->getID()].first;
            uint64_t queue_loc = outstanding_mem_req[mem_acks[i]->getID()].second;
            zqm_queue[hart_id][queue_loc]->status = 2;
            sendACKToZAP(hart_id);
            delete mem_acks[i];
            mem_acks[i] = NULL;
        }
    }
    mem_acks.erase(std::remove_if(
            mem_acks.begin(), mem_acks.end(),
            [](auto x) {
                return !x;
            }), mem_acks.end());
}

void ZQM::sendNACKToZAP(uint64_t dest) {
    std::vector<uint32_t> payload;
    // TODO: Update with ZAP id
    SST::Forza::zopEvent *nackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
    nackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
    nackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_EXCP);
    nackMsg->setSrcHart(m_zop_iface->getAddress());
    nackMsg->setSrcZCID(0);
    nackMsg->setSrcPCID(0);
    nackMsg->setSrcPrec(0);
    nackMsg->setDestHart(dest);
    nackMsg->setDestZCID(0);
    nackMsg->setDestPCID(0);
    nackMsg->setDestPrec(0);
    nackMsg->encodeEvent();
    m_zop_iface->send(nackMsg, (zopCompID)dest);
}

void ZQM::sendACKToZAP(uint64_t dest) {
    std::vector<uint32_t> payload;
    // TODO: Update with ZAP id
    SST::Forza::zopEvent *ackMsg = new SST::Forza::zopEvent(m_zop_iface->getAddress(), (zopCompID)dest);
    ackMsg->setType(SST::Forza::zopMsgT::Z_MSG);
    ackMsg->setOpc(SST::Forza::zopOpc::Z_MSG_ACK);
    ackMsg->setSrcHart(m_zop_iface->getAddress());
    ackMsg->setSrcZCID(0);
    ackMsg->setSrcPCID(0);
    ackMsg->setSrcPrec(0);
    ackMsg->setDestHart(dest);
    ackMsg->setDestZCID(0);
    ackMsg->setDestPCID(0);
    ackMsg->setDestPrec(0);
    ackMsg->encodeEvent();
    m_zop_iface->send(ackMsg, (zopCompID)dest);
}

void ZQM::processSetupMsgs() {
    for (auto &event : setup_reqs) {
        output.verbose(CALL_INFO, 1, 0, "setup pkt size for zqm\n");

        // TODO: Do I need a setup message that "releases" an AID?  I suspect
        //   the answer is yes.  Once done, then this needs to handle both the
        //   insert and delete state table requests (or we just assume an always
        //   incrementing AID for now and deal with it later)

        // TODO: Specify payload format in ZOP spec
        /**
         * payload 0,1 are header
         * payload 2-5 are data
         */
        uint32_t app_id = event->getAppID();
        uint64_t min_zap_hart = event->getPayload()[2];
        uint64_t max_zap_hart = event->getPayload()[3];
        uint64_t mem_buffer_low = event->getPayload()[4];
        uint64_t mem_buffer_high = event->getPayload()[5];

        auto it = aid_state_table.find(app_id);
        if (it != aid_state_table.end()) {
            // TODO: Do standard failure type...
            assert(false);
        }
        aid_state_table.insert(std::pair<uint32_t, ZqmAidStateTableRow>(app_id,
                                                                        {min_zap_hart,
                                                                         max_zap_hart,
                                                                         mem_buffer_low,
                                                                         mem_buffer_high}));
        output.verbose(CALL_INFO, 1, 0, "setup aid state table %u\n", app_id);
        delete event;
    }
}

bool ZQM::clock(Cycle_t cycle){
    handleIncomingRZAMsg();
    processSetupMsgs();
    return false;
}

// EOF
