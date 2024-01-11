//
// _zip_cc_
//

#include <sst/core/sst_config.h>
#include "zip.h"

using namespace SST::Forza;

ZIP::ZIP(ComponentId_t id, Params& params)
  : Component(id) {

  // set parameters
  p_precID    = params.find<unsigned int>("precID",     0);
  p_clockFreq = params.find<UnitAlgebra> ("clockFreq", "1GHz");
  p_maxWait   = params.find<UnitAlgebra> ("maxWait",   "1ms");
  p_verbose   = params.find<unsigned int>("verbose",    0);

  // Init the output handler
  output.init("ZIP[" + getName() + ":@p:@t]: ", p_verbose, 0, SST::Output::STDOUT);

  // register the clock handler
  registerClock(p_clockFreq, new Clock::Handler<ZIP>(this, &ZIP::clock));
  output.verbose(CALL_INFO, 9, 0, "Registering clock with frequency=%s\n", p_clockFreq.toStringBestSI().c_str());

  // calculate how many cycles to wait before clearing outgoing queue based on maximum wait time and clock freqency
  waitCycles = (p_maxWait*p_clockFreq).getRoundedValue();

  // load the backing memory controller
  Ctrl = loadUserSubComponent<ZIPMemCtrl>("memory");
  if( !Ctrl){
    output.fatal(CALL_INFO, -1, "Error: failed to initialize the memory subcomponent\n");
  }

  // zero out vectors tracking buffer sizes, one entry per precinct
  bufOutSize.assign(MAX_PREC, 0);
  bufInSize.assign(MAX_PREC, 0);

  // initialize vectors tracking credits by starting with maximum buffer size 
  outCredit.assign(MAX_PREC, MAX_BUFF);
  inCredit.assign(MAX_ZEN, MAX_ZEN_BUFF);

  // configure links
  link_NOC = loadUserSubComponent<SST::Forza::zopAPI>("zopLink");
  link_NOC->setNumHarts(3);
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
}

void ZIP::emergencyShutdown(SST::Output& out) {
}

void ZIP::printStatus() {
}

// try to send out data from the outgoing buffer for DestPrec if we have enough credits and return true if succesful
bool ZIP::aggregatePackets(uint16_t DestPrec, ZIPMemTarget* Target) {
  // check if read operation is done and if the destination precinct has sufficient credits
  if ( (Target->isDone()) && (outCredit[DestPrec] >= Target->getTarget().size()) ) {
    // decrease credits by the number of bytes we're about to send
    outCredit[DestPrec] -= Target->getTarget().size();

    // put buffer of concatenated zopEvent packets into a new ZIPAggEvent
    ZIPAggEvent* event = new ZIPAggEvent(vec8to64(Target->getTarget()));

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

    return true;
  // if waiting or read operation or if destination precinct's ZIP doesn't have enough credits to accept the payload, return false
  } else {
    return false;
  }
}

// try to send out data from the incoming buffer for SrcPrec if we have enough credits and return true if succesful
bool ZIP::disaggregatePackets(uint16_t SrcPrec, ZIPMemTarget* Target) {
  // check if read operation is done
  if (Target->isDone()) {
    // read incoming buffer for the source precinct
    ZIPAggEvent event(vec8to64(Target->getTarget()));

    // track the zopEvents we will need to send to ZENs if they have enough credits
    std::vector<zopEvent*> zenZOPs;
    // make copy of inCredits to track how many credits it'll take to send zopEvents to ZENs
    std::vector<uint64_t> inCreditCopy = inCredit;

    // iterate over zopEvents received from HFI
    for (zopEvent receivedZop : event.getZOPs()) {
      // if the ZEN has enough credits to accept the zopEvent, add it to zenZOPs
      if (inCreditCopy[receivedZop.getDestPCID()] >= receivedZop.getLength()+Z_NUM_HEADER_FLITS) {
        // update copy of credits to make sure each ZEN can receive all of the disaggregated ZOPs
        inCreditCopy[receivedZop.getDestPCID()] -= receivedZop.getLength()+Z_NUM_HEADER_FLITS;
        zenZOPs.push_back(dynamic_cast<zopEvent*>(receivedZop.clone()));
      // if any ZEN doesn't have enough credits, disaggregatePackets returns false
      } else{
        output.verbose(CALL_INFO, 9, 0, "disaggregatePackets: need %d credits for %d and only have %llu\n", receivedZop.getLength()+Z_NUM_HEADER_FLITS, receivedZop.getDestPCID(), inCreditCopy[receivedZop.getDestPCID()]);
        return false;
      }
    }

    // all ZENs have enough credits, so go through and send all of them to the NOC
    for (zopEvent* disaggZop : zenZOPs) {
      link_NOC->send(disaggZop, zopCompID::Z_ZEN, (zopPrecID)disaggZop->getDestPCID(), link_NOC->getPrecinctID());
      output.verbose(CALL_INFO, 9, 0, "disaggregatePackets: Type %hhu, SrcHart %d, SrcZCID %d, SrcPCID %d, SrcPrec %d, DestHart %d, DestZCID %d, DestPCID %d, DestPrec %d\n", disaggZop->getType(), disaggZop->getSrcHart(), disaggZop->getSrcZCID(), disaggZop->getSrcPCID(), disaggZop->getSrcPrec(), disaggZop->getDestHart(), disaggZop->getDestZCID(), disaggZop->getDestPCID(), disaggZop->getDestPrec());
      // update actual credits
      inCredit[disaggZop->getDestPCID()] -= disaggZop->getLength()+Z_NUM_HEADER_FLITS;
    }

    // after disaggregation, return credits to source precinct
    ZIPCreditEvent* cEvent = new ZIPCreditEvent(bufInSize[SrcPrec]);
    cEvent->dest = SrcPrec;
    cEvent->src = p_precID;
    link_HFI->send(cEvent, SrcPrec);

    // reset buffer size
    bufInSize[SrcPrec] = 0;

    delete Target;

    return true;
  } else {
    return false;
  }
}

// write Buf to memory
void ZIP::waitMemWriteComplete(uint64_t Addr, uint32_t Size, std::vector<uint8_t> Buf) {
  ZIPMemTarget* Target = new ZIPMemTarget(Buf);
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
    output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: Type %hhu, SrcHart %d, SrcZCID %d, SrcPCID %d, SrcPrec %d, DestHart %d, DestZCID %d, DestPCID %d, DestPrec %d\n", event->getType(), event->getSrcHart(), event->getSrcZCID(), event->getSrcPCID(), event->getSrcPrec(), event->getDestHart(), event->getDestZCID(), event->getDestPCID(), event->getDestPrec());

    // if the zopEvent is a Z_MSG_CREDIT, update credits for the source ZEN
    if (event->getOpc() == zopOpc::Z_MSG_CREDIT) {
      inCredit[event->getSrcPCID()] += event->getCredit();
      output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: received %d credits for %d, now at %llu\n", event->getCredit(), event->getSrcPCID(), inCredit[event->getSrcPCID()]);
    // otherwise add the zopEvent to the outgoing buffer for the destination precinct
    } else {
      uint16_t DestPrec = event->getDestPrec();

      std::vector<uint8_t> packet8 = vec64to8(event->getPacket());

      // store zopEvent in the buffer
      waitMemWriteComplete(FIRST_OUT_ADDR(DestPrec)+bufOutSize[DestPrec], packet8.size(), packet8);
      bufOutSize[DestPrec] += packet8.size();

      // if remaining buffer space can no longer fit a zopEvent of maximum size, add the destination precinct to the outgoing queue
      if (MAX_BUFF-bufOutSize[DestPrec] < MAX_ZOP) {
        output.verbose(CALL_INFO, 9, 0, "handleNOCEvent: DestPrec %d buffer out of space, aggregating packet\n", DestPrec);
        outQ.push(std::pair<uint16_t, ZIPMemTarget*>(DestPrec, waitMemReadComplete(FIRST_OUT_ADDR(DestPrec), bufOutSize[DestPrec])));
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
  
  // check if event is ZIPEvent containing many zopEvents
  ZIPAggEvent* aggEvent = dynamic_cast<ZIPAggEvent*>(event);
  if (aggEvent) {
    std::vector<uint8_t> packet8 = vec64to8(aggEvent->getPayload());

    // store concatenated zopEvents in buffer
    waitMemWriteComplete(FIRST_IN_ADDR(SrcPrec)+bufInSize[SrcPrec], packet8.size(), packet8);
    bufInSize[SrcPrec] += packet8.size();

    // add the source precinct to the incoming queue for disaggregation
    inQ.push(std::pair<uint16_t, ZIPMemTarget*>(SrcPrec, waitMemReadComplete(FIRST_IN_ADDR(SrcPrec), bufInSize[SrcPrec])));

    delete ev;
  }
}

// clock handler, called on each clock cycle
bool ZIP::clock(Cycle_t cycle){
  // every waitCycles cycles, send out all nonempty outgoing buffers to external ZIPs
  if ( cycle % waitCycles == 0 ) {
    for (int DestPrec=0; DestPrec<MAX_PREC; DestPrec++) {
      if (bufOutSize[DestPrec]) {
        outQ.push(std::pair<uint16_t, ZIPMemTarget*>(DestPrec, waitMemReadComplete(FIRST_OUT_ADDR(DestPrec), bufOutSize[DestPrec])));
      }
    }
  }

  // loop over outgoing queue
  for (unsigned i=0; i<outQ.size(); i++) {
    std::pair<uint16_t, ZIPMemTarget*> memReq = outQ.front();
    // try to send aggregated zopEvents to the destination precinct
    if (aggregatePackets(memReq.first, memReq.second)) {
      outQ.pop();
    // if unsuccessful (for lack of credits), put back at the end of the queue
    } else {
      outQ.pop();
      outQ.push(memReq);
    }
  }

  // loop over incoming queue
  for (unsigned i=0; i<inQ.size(); i++) {
    std::pair<uint16_t, ZIPMemTarget*> memReq = inQ.front();
    // try to send disaggregated zopEvents to the local ZENs
    if (disaggregatePackets(memReq.first, memReq.second)) {
      inQ.pop();
    // if unsuccessful (for lack of credits), put back at the end of the queue
    } else {
      inQ.pop();
      inQ.push(memReq);
    }
  }

  return false;
}

// change 64-bit vector from ZOP to 8-bit vector for storage in memory
std::vector<uint8_t> ZIP::vec64to8(std::vector<uint64_t> oldvec) {
  std::vector<uint8_t> newvec;

  for (auto elem : oldvec) {
    newvec.push_back((uint8_t)(elem >> 56));
    newvec.push_back((uint8_t)(elem >> 48));
    newvec.push_back((uint8_t)(elem >> 40));
    newvec.push_back((uint8_t)(elem >> 32));
    newvec.push_back((uint8_t)(elem >> 24));
    newvec.push_back((uint8_t)(elem >> 16));
    newvec.push_back((uint8_t)(elem >>  8));
    newvec.push_back((uint8_t)(elem >>  0));
  }

  return newvec;
}

// change 8-bit vector from memory to 64-bit vector for zopEvent
std::vector<uint64_t> ZIP::vec8to64(std::vector<uint8_t> oldvec) {
  std::vector<uint64_t> newvec;

  for (unsigned i=0; i<oldvec.size(); i+=8) {
    newvec.push_back(((uint64_t) oldvec[i+0] << 56) |
                     ((uint64_t) oldvec[i+1] << 48) |
                     ((uint64_t) oldvec[i+2] << 40) |
                     ((uint64_t) oldvec[i+3] << 32) |
                     ((uint64_t) oldvec[i+4] << 24) |
                     ((uint64_t) oldvec[i+5] << 16) |
                     ((uint64_t) oldvec[i+6] <<  8) |
                     ((uint64_t) oldvec[i+7] <<  0));
  }

  return newvec;
}

// EOF
