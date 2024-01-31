//
// _zip_cc_
//

#include <sst/core/sst_config.h>
#include "zip.h"

using namespace SST::Forza;

ZIP::ZIP(ComponentId_t id, Params& params)
  : Component(id) {

  // set parameters
  p_precID     = params.find<unsigned int>("precID",     0);
  p_clockFreq  = params.find<UnitAlgebra> ("clockFreq", "1GHz");
  p_maxWait    = params.find<UnitAlgebra> ("maxWait",   "1ms");
  p_verbose    = params.find<unsigned int>("verbose",    0);
  p_tests      = params.find<unsigned int>("tests",      0);

  p_numPrec    = params.find<unsigned int>("numPrec",    16);
  p_maxBuff    = params.find<unsigned int>("maxBuff",    16);
  p_maxZOP     = params.find<unsigned int>("maxZOP",     3);
  p_numZone    = params.find<unsigned int>("numZone",    8);
  p_maxZENBuff = params.find<unsigned int>("maxZENBuff", 8);
  p_maxRVBuff  = params.find<unsigned int>("maxRVBuff",  0);
  p_MTU        = params.find<unsigned int>("MTU",        0);
  p_RVThresh   = params.find<unsigned int>("RVThresh",   10000000);

  // Init the output handler
  output.init("ZIP[" + getName() + ":@p:@t]: ", p_verbose, p_tests, SST::Output::STDOUT);

  // register the clock handler
  zipTime = registerClock(p_clockFreq, new Clock::Handler<ZIP>(this, &ZIP::clock));
  output.verbose(CALL_INFO, 9, 0, "Registering clock with frequency=%s\n", p_clockFreq.toStringBestSI().c_str());

  // calculate how many cycles to wait before clearing outgoing queue based on maximum wait time and clock freqency
  waitCycles = (p_maxWait.isValueZero()) ? 1 : (p_maxWait*p_clockFreq).getRoundedValue();

  // register statistics
  s_numPackets    = registerStatistic<uint64_t>("num_packets");
  s_numStalls     = registerStatistic<uint64_t>("num_stalls");
  s_numSentPackets = registerStatistic<uint64_t>("num_sent_packets");
  s_numRecvPackets = registerStatistic<uint64_t>("num_recv_packets");
  s_numWaitCycles = registerStatistic<uint64_t>("num_wait_cycles");

  t_c1 = false;
  t_c2 = false;
  t_c3 = false;
  t_c4 = false;
  t_c5 = false;

  // load the backing memory controller
  Ctrl = loadUserSubComponent<ZIPMemCtrl>("memory");
  if( !Ctrl){
    output.fatal(CALL_INFO, -1, "Error: failed to initialize the memory subcomponent\n");
  }

  // zero out vectors tracking buffer sizes, one entry per precinct
  bufOutSize.assign(p_numPrec, 0);
  bufInSize.assign(p_numPrec, 0);

  bufOutLock.assign(p_numPrec, false);

  RVBufInSize = 0;

  RVBufOutCTS.assign(p_numPrec, false);

  // initialize vectors tracking credits by starting with maximum buffer size 
  outCredit.assign(p_numPrec, p_maxBuff);
  inCredit.assign(p_numZone, p_maxZENBuff);

  // configure links
  link_NOC = loadUserSubComponent<SST::Forza::zopAPI>("zopLink");
  link_NOC->setNumHarts(1);
  link_NOC->setPrecinctID(p_precID);
  // link_NOC->setZoneID((uint8_t)zopPrecID::Z_ZIP);
  link_NOC->setZoneID(9);
  link_NOC->setEndpointType(zopCompID::Z_PREC_ZIP);
  sst_assert(link_NOC, CALL_INFO, -1, "Error in %s: link configuration for 'port_NOC' failed\n", getName().c_str());

  link_HFI = loadUserSubComponent<SST::Forza::nicAPI>( "hfiLink");
  sst_assert(link_HFI, CALL_INFO, -1, "Error in %s: link configuration for 'port_HFI' failed\n", getName().c_str());
}

ZIP::~ZIP() {}

void ZIP::init(unsigned phase) {
  Ctrl->init(phase);
  // todo: initialize vectors to proper sizes by recognizing other components rather than using hard-coded values
  link_HFI->init(phase);
  link_NOC->init(phase);
}

void ZIP::setup() {
  Ctrl->setup();
  link_NOC->setMsgHandler( new Event::Handler<ZIP>(this, &ZIP::handleNOCEvent) );
  link_NOC->setup();
  link_HFI->setMsgHandler( new Event::Handler<ZIP>(this, &ZIP::handleHFIEvent) );
  link_HFI->setup();
  output.verbose(CALL_INFO, 9, 0, "num hfi dest: %d\n", link_HFI->getNumDestinations());
}

void ZIP::complete(unsigned phase) {
  Ctrl->complete(phase);
}

void ZIP::finish() {
  Ctrl->finish();
  if (t_c1) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_C1] pass\n");
  if (t_c2) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_C2] pass\n");
  if (t_c3) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_C3] pass\n");
  if (t_c4) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_C4] pass\n");
  if (t_c5) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_C5] pass\n");
  if (dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numPackets)->getSum() > 0) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_P1] %f%%\n", 100.0*dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numStalls)->getSum()/dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numPackets)->getSum());
  if (dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numSentPackets)->getSum() > 0) output.verbose(CALL_INFO, 1, 1, "[TEST ZIP_P{2,3,4}] %f cycles/packet\n", 1.0*dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numWaitCycles)->getSum()/dynamic_cast<AccumulatorStatistic<uint64_t>*>(s_numSentPackets)->getSum());
}

// void ZIP::emergencyShutdown(SST::Output& out) {
// }
// 
// void ZIP::printStatus() {
// }

// try to send out data from the outgoing buffer for DestPrec if we have enough credits and return true if succesful
bool ZIP::aggregatePackets(uint16_t DestPrec, ZIPMemTarget* Target, bool* hasStalled) {
  // check if read operation is done
  if (Target->isDone()) {
    // check if the destination precinct has sufficient credits
    if (outCredit[DestPrec] >= Target->getTarget64().size()) {
      t_c3 = true;

      // decrease credits by the number of bytes we're about to send
      outCredit[DestPrec] -= Target->getTarget64().size();

      // put buffer of concatenated zopEvent packets into a new ZIPAggEvent
      ZIPAggEvent* event = new ZIPAggEvent(Target->getTarget64());

      event->dest = DestPrec;
      event->src = p_precID;

      // send request to HFI
      output.verbose(CALL_INFO, 9, 0, "aggregatePackets: SrcPrec %d, DestPrec %d\n", p_precID, DestPrec);
      link_HFI->send(event, DestPrec);
      // reset buffer size
      bufOutSize[DestPrec] = 0;

      // replenish ZEN credits
      // iterate over zopEvents sent to HFI
      for (zopEvent sentZop : event->getZOPs()) {
        // construct new Z_MSG_CREDIT zopEvent with credits equal to the total number of flits to go to the source ZEN of sentZop
        zopEvent* creditZop = new zopEvent();
        creditZop->setType(zopMsgT::Z_MSG);
        creditZop->setOpc(zopOpc::Z_MSG_CREDIT);
        creditZop->setSrcZCID(zopCompID::Z_PREC_ZIP);
        creditZop->setSrcPCID((uint8_t)zopPrecID::Z_ZIP);
        creditZop->setSrcPrec(link_NOC->getPrecinctID());
        creditZop->setCredit(sentZop.getLength()+Z_NUM_HEADER_FLITS);

        // send credits back to source ZEN
        creditZop->encodeEvent();
        link_NOC->send(creditZop, zopCompID::Z_ZEN, (zopPrecID)sentZop.getSrcPCID(), link_NOC->getPrecinctID());
      }

      delete Target;

      t_c4 = true;
      s_numSentPackets->addData(1);
      return true;
    // stall due to not enough credits
    } else {
      t_c1 = true;
      if (!*hasStalled) {
        s_numStalls->addData(1);
	*hasStalled = true;
      }

      return false;
    }
  // still waiting for read to complete
  } else {
    return false;
  }
}

bool ZIP::aggregateRVPackets(uint16_t DestPrec, ZIPMemTarget* Target) {
  if ( (Target->isDone()) && (RVBufOutCTS[DestPrec]) ) {
    std::vector<uint64_t> payload = Target->getTarget64();

    output.verbose(CALL_INFO, 9, 0, "aggregateRVPackets: SrcPrec %d, DestPrec %d\n", p_precID, DestPrec);

    uint64_t num = 0;
    uint64_t sentFlits = 0;
    while (sentFlits < payload.size()) {
      uint64_t toSend = std::min(p_MTU, (unsigned)payload.size());
      ZIPAggRVEvent* event = new ZIPAggRVEvent(num, std::vector<uint64_t>(payload.begin()+sentFlits, payload.begin()+sentFlits+toSend));

      event->dest = DestPrec;
      event->src = p_precID;

      link_HFI->send(event, DestPrec);

      num++;
      sentFlits += toSend;
    }

    bufOutSize[DestPrec] = 0;

    for (zopEvent sentZop : ZIPAggEvent(payload).getZOPs()) {
        // construct new Z_MSG_CREDIT zopEvent with credits equal to the total number of flits to go to the source ZEN of sentZop
        zopEvent* creditZop = new zopEvent();
        creditZop->setType(zopMsgT::Z_MSG);
        creditZop->setOpc(zopOpc::Z_MSG_CREDIT);
        creditZop->setSrcZCID(zopCompID::Z_PREC_ZIP);
        creditZop->setSrcPCID((uint8_t)zopPrecID::Z_ZIP);
        creditZop->setSrcPrec(link_NOC->getPrecinctID());
        creditZop->setCredit(sentZop.getLength()+Z_NUM_HEADER_FLITS);

        // send credits back to source ZEN
        creditZop->encodeEvent();
        link_NOC->send(creditZop, zopCompID::Z_ZEN, (zopPrecID)sentZop.getSrcPCID(), link_NOC->getPrecinctID());
    }

    delete Target;

    RVBufOutCTS[DestPrec] = false;

    return true;
  } else {
    return false;
  }
}

// try to send out data from the incoming buffer for SrcPrec if we have enough credits and return true if succesful
bool ZIP::disaggregatePackets(uint16_t SrcPrec, ZIPMemTarget* Target, bool RV) {
  // check if read operation is done
  if (Target->isDone()) {
    // read incoming buffer for the source precinct
    ZIPAggEvent event(Target->getTarget64());

    // track the zopEvents we will need to send to ZENs if they have enough credits
    std::vector<zopEvent*> zenZOPs;
    // make copy of inCredits to track how many credits it'll take to send zopEvents to ZENs
    std::vector<uint64_t> inCreditCopy = inCredit;

    // iterate over zopEvents received from HFI
    for (zopEvent receivedZop : event.getZOPs()) {
      // if the ZEN has enough credits to accept the zopEvent, add it to zenZOPs
      if (inCreditCopy[receivedZop.getDestPCID()] >= (long unsigned int)(receivedZop.getLength()+Z_NUM_HEADER_FLITS)) {
        // update copy of credits to make sure each ZEN can receive all of the disaggregated ZOPs
        inCreditCopy[receivedZop.getDestPCID()] -= receivedZop.getLength()+Z_NUM_HEADER_FLITS;
        zenZOPs.push_back(dynamic_cast<zopEvent*>(receivedZop.clone()));
      // if any ZEN doesn't have enough credits, disaggregatePackets returns false
      } else{
        if (RV) {
          output.verbose(CALL_INFO, 10, 0, "disaggregateRVPackets: need %d credits for %d and only have %llu\n", receivedZop.getLength()+Z_NUM_HEADER_FLITS, receivedZop.getDestPCID(), inCreditCopy[receivedZop.getDestPCID()]);
	} else {
          output.verbose(CALL_INFO, 10, 0, "disaggregatePackets: need %d credits for %d and only have %llu\n", receivedZop.getLength()+Z_NUM_HEADER_FLITS, receivedZop.getDestPCID(), inCreditCopy[receivedZop.getDestPCID()]);
          t_c2 = true;
	}
        return false;
      }
    }

    // all ZENs have enough credits, so go through and send all of them to the NOC
    for (zopEvent* disaggZop : zenZOPs) {
      link_NOC->send(disaggZop, zopCompID::Z_ZEN, (zopPrecID)disaggZop->getDestPCID(), link_NOC->getPrecinctID());
      if (RV) {
        output.verbose(CALL_INFO, 9, 0, "disaggregateRVPackets: Type %hhu, SrcHart %d, SrcZCID %d, SrcPCID %d, SrcPrec %d, DestHart %d, DestZCID %d, DestPCID %d, DestPrec %d\n", (uint8_t)(disaggZop->getType()), disaggZop->getSrcHart(), disaggZop->getSrcZCID(), disaggZop->getSrcPCID(), disaggZop->getSrcPrec(), disaggZop->getDestHart(), disaggZop->getDestZCID(), disaggZop->getDestPCID(), (int)(disaggZop->getDestPrec()));
      } else {
        output.verbose(CALL_INFO, 9, 0, "disaggregatePackets: Type %hhu, SrcHart %d, SrcZCID %d, SrcPCID %d, SrcPrec %d, DestHart %d, DestZCID %d, DestPCID %d, DestPrec %d\n", (uint8_t)(disaggZop->getType()), disaggZop->getSrcHart(), disaggZop->getSrcZCID(), disaggZop->getSrcPCID(), disaggZop->getSrcPrec(), disaggZop->getDestHart(), disaggZop->getDestZCID(), disaggZop->getDestPCID(), (int)(disaggZop->getDestPrec()));
      }

      // update actual credits
      inCredit[disaggZop->getDestPCID()] -= disaggZop->getLength()+Z_NUM_HEADER_FLITS;
    }

    if (RV) {
      RVBufInRecv = 0;
      RVBufInSize = 0;
    } else {
      // after disaggregation, return credits to source precinct
      ZIPCreditEvent* cEvent = new ZIPCreditEvent(bufInSize[SrcPrec]/8);
      cEvent->dest = SrcPrec;
      cEvent->src = p_precID;
      link_HFI->send(cEvent, SrcPrec);

      // reset buffer size
      bufInSize[SrcPrec] = 0;

      t_c5 = true;
    }

    delete Target;

    return true;
  } else {
    return false;
  }
}

// write Buf to memory
void ZIP::waitMemWriteComplete(uint64_t Addr, uint32_t Size, std::vector<uint64_t> Buf) {
  ZIPMemTarget* Target = new ZIPMemTarget();
  Target->setTarget64(Buf);
  Ctrl->sendWRITERequest(Addr, Size, Target);
}

// read data from memory, return target to retreive data when operation has completed
ZIPMemTarget* ZIP::waitMemReadComplete(uint64_t Addr, uint32_t Size) {
  ZIPMemTarget* Target = new ZIPMemTarget();
  Ctrl->sendREADRequest(Addr, Size, Target);
  return Target;
}

// event handler, called when an event is received on the NOC link
void ZIP::handleNOCEvent(SST::Event* ev) {
  zopEvent* event = dynamic_cast<zopEvent*>(ev);

  // if zopEvent was received from NOC
  if (event) {
    event->decodeEvent();
    output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: Type %hhu, SrcHart %d, SrcZCID %d, SrcPCID %d, SrcPrec %d, DestHart %d, DestZCID %d, DestPCID %d, DestPrec %d\n", (uint8_t)(event->getType()), event->getSrcHart(), event->getSrcZCID(), event->getSrcPCID(), event->getSrcPrec(), event->getDestHart(), event->getDestZCID(), event->getDestPCID(), event->getDestPrec());

    // if the zopEvent is a Z_MSG_CREDIT, update credits for the source ZEN
    if (event->getOpc() == zopOpc::Z_MSG_CREDIT) {
      inCredit[event->getSrcPCID()] += event->getCredit();
      output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: received %d credits for %d, now at %llu\n", event->getCredit(), event->getSrcPCID(), inCredit[event->getSrcPCID()]);
    // otherwise add the zopEvent to the outgoing buffer for the destination precinct
    } else {
      uint16_t DestPrec = event->getDestPrec();

      // store zopEvent in the buffer
      waitMemWriteComplete((2*DestPrec+1)*p_maxBuff*8+bufOutSize[DestPrec], event->getPacket().size()*8, event->getPacket());
      bufOutSize[DestPrec] += event->getPacket().size()*8;

      // if remaining buffer space can no longer fit a zopEvent of maximum size, add the destination precinct to the outgoing queue
      if (p_maxBuff*8-bufOutSize[DestPrec] < p_maxZOP*8) {
        output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: DestPrec %d buffer out of space, aggregating packet\n", DestPrec);
        addToOutQ(DestPrec);
      }
    }
  }
}

// event handler, called when an event is received on the HFI link
void ZIP::handleHFIEvent(SST::Event* ev) {
  ZIPEvent* event = dynamic_cast<ZIPEvent*>(ev);
  uint16_t SrcPrec = event->src;
  output.verbose(CALL_INFO, 9, 0, "handleHFIEvent: from precinct %d\n", SrcPrec);

  // check if event is a credit event, in which case replenish credits
  ZIPCreditEvent* cEvent = dynamic_cast<ZIPCreditEvent*>(event);
  if (cEvent) {
    outCredit[SrcPrec] += cEvent->getCredits();
    output.verbose(CALL_INFO, 9, 0, "handleHFIEvent: received %d credits for %d, now at %llu\n", cEvent->getCredits(), SrcPrec, outCredit[SrcPrec]);
    delete ev;
    return;
  }
  
  ZIPRVEvent* RVEvent = dynamic_cast<ZIPRVEvent*>(event);
  if (RVEvent) {
    if (RVEvent->isCTS()) {
      RVBufOutCTS[RVEvent->src] = true;
    } else {
      recvRVQ.push(std::tuple<uint16_t, uint64_t>(RVEvent->src, RVEvent->getSize()));
    }
    delete ev;
    return;
  }

  // check if event is ZIPEvent containing many zopEvents
  ZIPAggEvent* aggEvent = dynamic_cast<ZIPAggEvent*>(event);
  if (aggEvent) {
    // store concatenated zopEvents in buffer
    waitMemWriteComplete(2*SrcPrec*p_maxBuff*8+bufInSize[SrcPrec], aggEvent->getPayload().size()*8, aggEvent->getPayload());
    bufInSize[SrcPrec] += aggEvent->getPayload().size()*8;

    // add the source precinct to the incoming queue for disaggregation
    inQ.push(std::tuple<uint16_t, ZIPMemTarget*>(SrcPrec, waitMemReadComplete(2*SrcPrec*p_maxBuff*8, bufInSize[SrcPrec])));
    s_numRecvPackets->addData(1);

    delete ev;
    return;
  }

  ZIPAggRVEvent* aggRVEvent = dynamic_cast<ZIPAggRVEvent*>(event);
  if (aggRVEvent) {
    waitMemWriteComplete((uint64_t)(p_numPrec*p_maxBuff*8)+aggRVEvent->getNum()*((uint64_t)(p_MTU*8)), aggRVEvent->getPayload().size()*8, aggRVEvent->getPayload());
    RVBufInRecv += aggRVEvent->getPayload().size()*8;

    if (RVBufInRecv >= RVBufInSize) {
      inRVQ.push(std::tuple<uint16_t, ZIPMemTarget*>(SrcPrec, waitMemReadComplete(p_numPrec*p_maxBuff*8, RVBufInSize)));
    }

    delete ev;
    return;
  }
}

void ZIP::addToOutQ(uint16_t DestPrec) {
  if (bufOutSize[DestPrec] < p_RVThresh*8) {
    outQ.push(std::tuple<uint16_t, ZIPMemTarget*, Cycle_t, bool*>(DestPrec, waitMemReadComplete((2*DestPrec+1)*p_maxBuff*8, bufOutSize[DestPrec]), getNextClockCycle(zipTime), new bool(false)));
    s_numPackets->addData(1);
  } else {
    ZIPRVEvent* RVEvent = new ZIPRVEvent(bufOutSize[DestPrec]);
    RVEvent->dest = DestPrec;
    RVEvent->src = p_precID;
    link_HFI->send(RVEvent, DestPrec);

    outRVQ.push(std::tuple<uint16_t, ZIPMemTarget*>(DestPrec, waitMemReadComplete((2*DestPrec+1)*p_maxBuff*8, bufOutSize[DestPrec])));
  }
}

// clock handler, called on each clock cycle
bool ZIP::clock(Cycle_t cycle){
  // send out rendezvous CTS
  if ( (RVBufInSize == 0) && (!recvRVQ.empty()) ) {
    ZIPRVEvent* RVEvent = new ZIPRVEvent(true);
    RVEvent->dest = std::get<0>(recvRVQ.front());
    RVEvent->src = p_precID;
    link_HFI->send(RVEvent, std::get<0>(recvRVQ.front()));

    RVBufInSize = std::get<1>(recvRVQ.front());

    recvRVQ.pop();
  }

  // every waitCycles cycles, send out all nonempty outgoing buffers to external ZIPs
  if ( cycle % waitCycles == 0 ) {
    for (unsigned DestPrec=0; DestPrec<p_numPrec; DestPrec++) {
      if (bufOutSize[DestPrec] && (!bufOutLock[DestPrec])) {
        bufOutLock[DestPrec] = true;
        addToOutQ(DestPrec);
      }
    }
  }

  unsigned loop_size;

  // loop over outgoing queue
  loop_size = outQ.size();
  for (unsigned i=0; i<loop_size; i++) {
    std::tuple<uint16_t, ZIPMemTarget*, Cycle_t, bool*> memReq = outQ.front();
    // try to send aggregated zopEvents to the destination precinct
    if (aggregatePackets(std::get<0>(memReq), std::get<1>(memReq), std::get<3>(memReq))) {
      bufOutLock[std::get<0>(memReq)] = false;
      outQ.pop();
      s_numWaitCycles->addData(getNextClockCycle(zipTime)-std::get<2>(memReq));
    // if unsuccessful (for lack of credits), put back at the end of the queue
    } else {
      outQ.pop();
      outQ.push(memReq);
    }
  }

  // loop over outgoing rendezvous queue
  loop_size = outRVQ.size();
  for (unsigned i=0; i<loop_size; i++) {
    std::tuple<uint16_t, ZIPMemTarget*> memReq = outRVQ.front();
    if (aggregateRVPackets(std::get<0>(memReq), std::get<1>(memReq))) {
      bufOutLock[std::get<0>(memReq)] = false;
      outRVQ.pop();
    } else {
      outRVQ.pop();
      outRVQ.push(memReq);
    }
  }

  // loop over incoming queue
  loop_size = inQ.size();
  for (unsigned i=0; i<loop_size; i++) {
    std::tuple<uint16_t, ZIPMemTarget*> memReq = inQ.front();
    // try to send disaggregated zopEvents to the local ZENs
    if (disaggregatePackets(std::get<0>(memReq), std::get<1>(memReq), false)) {
      inQ.pop();
    // if unsuccessful (for lack of credits), put back at the end of the queue
    } else {
      inQ.pop();
      inQ.push(memReq);
    }
  }

  // loop over incoming rendezvous queue
  loop_size = inRVQ.size();
  for (unsigned i=0; i<loop_size; i++) {
    std::tuple<uint16_t, ZIPMemTarget*> memReq = inRVQ.front();
    if (disaggregatePackets(std::get<0>(memReq), std::get<1>(memReq), true)) {
      inRVQ.pop();
    } else {
      inRVQ.pop();
      inRVQ.push(memReq);
    }
  }

  return false;
}

// EOF
