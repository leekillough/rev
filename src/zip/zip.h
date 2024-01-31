//
// _zip_h_
//

#ifndef _ZIP_H_
#define _ZIP_H_

#include "SST.h"
#include "zip_events.h"
#include "zip_memctrl.h"
#include "zip_nic.h"
#include <string>

#include "ZOPNET.h"

namespace SST::Forza{
  class ZIP : public SST::Component{
  public:
    // register the component
    SST_ELI_REGISTER_COMPONENT(
      ZIP,                                    // component class
      "ForzaZIP",                             // component libary
      "ZIP",                                  // component name
      SST_ELI_ELEMENT_VERSION(1,0,0),         // Version of the component
      "ZIP: Forza ZIP component",             // description
      COMPONENT_CATEGORY_PROCESSOR            // category
    )

    // describe the parameters
    SST_ELI_DOCUMENT_PARAMS(
      { "precID",     "Precinct ID.",                                  "0" },
      { "clockFreq",  "ZIP core clock frequency.",                     "1GHz" },
      { "maxWait",    "Maximum time for a ZOP to wait in ZIP buffer.", "1ms" },
      { "verbose",    "Sets the output verbosity.",                    "0" },
      { "tests",      "Output flag, set to 1 for testing.",            "0" },
      { "numPrec",    "Number of precincts",                           "16" },
      { "maxBuff",    "Maximum buffer size in flits",                  "128" },
      { "maxZOP",     "Maximum ZOP size in flits",                     "24" },
      { "numZone",    "Number of zones",                               "8" },
      { "maxZENBuff", "Maximum ZEN buffer size in flits",              "64" },
      { "maxRVBuff",  "Maximum rendezvous buffer size in flits",       "0" },
      { "MTU",        "MTU in flits",                                  "0" },
      { "RVThresh",   "Rendezvous threshold in flits",                 "10000000" }
    )

    // describe the ports
    SST_ELI_DOCUMENT_PORTS(
      { "port_NOC", "Link to NOC. Used to inject ZOPs destined for local zones within the precinct.", { "SST::Forza::zopEvent" } },
      { "port_HFI", "Link to HFI. Used to inject ZOPs destined for the off-precinct fabric.",         { "SST::Forza::ZIPEvent" } }
    )

    // describe the statistics
    SST_ELI_DOCUMENT_STATISTICS(
      { "num_packets",      "Count of packets to be sent to another ZIP.",                      "count of packets", 1},
      { "num_stalls",       "Count of packets stalled before being sent to another ZIP.",       "count of packets", 1},
      { "num_sent_packets", "Count of packets successfully sent to another ZIP.",               "count of packets", 1},
      { "num_recv_packets", "Count of packets successfully received from another ZIP.",         "count of packets", 1},
      { "num_wait_cycles" , "Count of cycles packets waited before being sent to another ZIP.", "count of cycles",  1}
    )

    // describe the subcomponent slots
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      { "memory",  "Backend ZIP memory infrastruture", "SST::Forza::ZIPMemCtrl" },
      { "rtrLink", "ZIP NIC",                          "SST::Interface::SimpleNetwork" }
    )

    // public class members

    /// ZIP: constructor
    ZIP(SST::ComponentId_t id, SST::Params& params);

    /// ZIP: destructor
    ~ZIP();

    // required SST component functions
    void init(unsigned phase);
    void setup();
    void complete(unsigned phase);
    void finish();
    // void emergencyShutdown(SST::Output& out);
    // void printStatus();

  private:
    /// ZIP: event handlers
    /// These functions handle events coming into the ZIP. These events can either ask our ZIP to replenish credits or give us ZOPs.
    /// The NOC will give us ZOPs that we'll store in the outgoing buffer, and the HFI will give us bundled ZOPs that we'll store in
    /// the incoming buffer. After putting the ZOPs in memory, these functions will queue requests to send the data from the buffers
    /// to where they need to go.
    void handleNOCEvent(SST::Event* ev); // triggered when intra-precinct zopEvent comes from the NOC
    void handleHFIEvent(SST::Event* ev); // triggered when inter-precinct ZIPEvent comes from HFI

    /// ZIP: clock handler
    /// The clock function runs with the frequency of the clockFreq parameter. Each cycle, the outgoing and incoming queues are
    /// processed, and buffers are sent off if we have the necessary credits.
    bool clock(SST::Cycle_t cycle);
    TimeConverter* zipTime;

    /// ZIP: interfacing with memory
    /// These functions work with the memory controller to send write and read requests. The write operation requires a Buf to store.
    /// The read operation returns a pointer to a target that we can check to see if the operation is finished and pull the retrieved
    /// data. The thinking is that memory operations should happen linearly, so writes will finish before the read, so reads will act
    /// as a barrier.
    void          waitMemWriteComplete(uint64_t Addr, uint32_t Size, std::vector<uint64_t> Buf);
    ZIPMemTarget*  waitMemReadComplete(uint64_t Addr, uint32_t Size);

    /// ZIP: sending memory
    /// These functions try to send data from memory to the NOC or HFI and return true if successful. They'll return false if we
    /// don't have enough credits, meaning that the NOC or HFI doesn't have enough space in its receiving buffer to accept the data.
    /// They require a pointer to a target to check if the read operation has completed.
    bool    aggregatePackets(uint16_t DestPrec, ZIPMemTarget* Target, bool* hasStalled); // try to send a ZIPAggEvent to HFI containing aggregated
								                         // zopEvents stored in outgoing buffer for precinct DestPrec
    bool disaggregatePackets(uint16_t SrcPrec, ZIPMemTarget* Target, bool RV);           // try to send zopEvents to the local NOC from the incoming
								                         // buffer for precinct SrcPrec

    bool aggregateRVPackets(uint16_t DestPrec, ZIPMemTarget* Target);

    void addToOutQ(uint16_t);

    // private data members
    SST::Output output;         ///< ZIP: SST output handler
    ZIPMemCtrl* Ctrl;           ///< ZIP: memory controller

    // links
    SST::Forza::zopAPI* link_NOC;
    SST::Forza::nicAPI* link_HFI;

    // buffer sizes indexed by precinct
    // The macro FIRST_OUT_ADDR(p) gives the address of the first byte in memory reserved for the buffer of ZOPs to be sent out to
    // the ZIP of precinct p. Similarly, FIRST_IN_ADDR(p) gives the address of the first byte in memory reserved for the buffer of
    // ZOPs received from the ZIP of precinct p. These vectors keep track of how many bytes have been placed in the respective buffers.
    std::vector<uint64_t> bufOutSize; // bytes contained in the outgoing buffer for given destination precinct
    std::vector<uint64_t> bufInSize;  // bytes contained in the incoming buffer for given source precinct

    std::vector<bool> bufOutLock;

    uint64_t RVBufInSize;
    uint64_t RVBufInRecv;

    std::vector<bool> RVBufOutCTS;

    // credits indexed by precinct/ZEN
    // These credit vectors keep track of how many bytes are available in the receiving buffers of external ZIPs and the local ZENs.
    // Credits are replenished once we receive credit events, indicating that the receiving buffers have been emptied.
    std::vector<uint64_t> outCredit;  // bytes available in the receiving buffer for the ZIP of the given destination precinct
    std::vector<uint64_t> inCredit;   // bytes available in the receiving buffer for the the given local ZEN

    // queues for precincts that have packets going in and out
    // These queues keep track of which buffers are ready to be sent off to external ZIPs or the local NOC. Precincts are added to
    // the outgoing queue once the buffer size hits a threshold size or a maximum number of clock cycles have elapsed, whichever comes
    // sooner. Precincts are added to the incoming queue as soon as a packet of aggregated ZOPs is received from an external ZIP.
    // Targets are also stored with the precinct ID to track when read operations have completed as well as (for the outgoing queue)
    // the clock cycle when the packet was added to the queue and whether it was stalled due to lack of credits.
    std::queue<std::tuple<uint16_t, ZIPMemTarget*, SimTime_t, bool*>> outQ; // contains destination precinct IDs that must be sent their aggregated outgoing buffers
    std::queue<std::tuple<uint16_t, ZIPMemTarget*>>                   inQ;  // contains source precinct IDs whose incoming buffers must be disaggregated and sent to the NOC

    std::queue<std::tuple<uint16_t, ZIPMemTarget*>> outRVQ;
    std::queue<std::tuple<uint16_t, ZIPMemTarget*>> inRVQ;
    std::queue<std::tuple<uint16_t, uint64_t>> recvRVQ;
 
    // maximum number of cycles to wait for outgoing buffers to be cleared
    unsigned int waitCycles;

    // parameters
    unsigned int p_precID;    // local precinct ID
    UnitAlgebra  p_clockFreq;
    UnitAlgebra  p_maxWait;
    unsigned int p_verbose;
    unsigned int p_tests;

    unsigned int p_numPrec;
    unsigned int p_maxBuff;
    unsigned int p_maxZOP;
    unsigned int p_numZone;
    unsigned int p_maxZENBuff;
    unsigned int p_maxRVBuff;
    unsigned int p_MTU;
    unsigned int p_RVThresh;

    // statistics
    Statistic<uint64_t>* s_numPackets;
    Statistic<uint64_t>* s_numStalls;
    Statistic<uint64_t>* s_numSentPackets;
    Statistic<uint64_t>* s_numRecvPackets;
    Statistic<uint64_t>* s_numWaitCycles;

    // correctness tests
    bool t_c1;
    bool t_c2;
    bool t_c3;
    bool t_c4;
    bool t_c5;
  }; // class SST::ZIP
} // namespace SST::Forza

#endif // _ZIP_H_

// EOF
