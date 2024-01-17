//
// _zip_memctrl_h_
//
//

#ifndef _ZIP_MEMCTRL_H_
#define _ZIP_MEMCTRL_H_

// -- CXX Headers
#include <vector>
#include <map>

// -- SST Headers
#include "SST.h"

namespace SST::Forza{

  using namespace SST::Interfaces;

  // target class so that we can keep track of when read and write requests finish
  class ZIPMemTarget {
    public:
      ZIPMemTarget() : done(false) {}
      ZIPMemTarget(std::vector<uint8_t> target) : done(false), target(target) {}
      ~ZIPMemTarget() {}

      bool isDone() { return done; }
      void setDone() { done = true; }

      std::vector<uint8_t> getTarget() { return target; };
      void setTarget(std::vector<uint8_t> t) { target = t; };

    private:
      bool done;
      std::vector<uint8_t> target;
  };

  // ------------------------------------------------------------
  // ZIPMemOp Class
  // ------------------------------------------------------------
  class ZIPMemOp{
  public:
    enum MemOp{
      ZIP_MemOpREAD       = 0,
      ZIP_MemOpWRITE      = 1,
    };

    /// ZIPMemOp: constructor
    ZIPMemOp(MemOp Op, uint64_t Addr, uint32_t Size, ZIPMemTarget* Target)
      : Op(Op), Addr(Addr), Size(Size), Target(Target){}

    /// ZIPMemOp: destructor
    ~ZIPMemOp() = default;

    /// ZIPMemOp: retrieve the operation type
    uint64_t getOp() const { return Op; }

    /// ZIPMemOp: retrieve the address
    uint64_t getAddr() const { return Addr; }

    /// ZIPMemOp: retrieve the size
    uint32_t getSize() const { return Size; }

    /// ZIPMemOp: retrieve the buffer
    std::vector<uint8_t> getBuf() const { return Target->getTarget(); }

    /// ZIPMemOp: retrieve the target
    ZIPMemTarget* getTarget() const { return Target; }

    /// ZIPMemOp: set the operation type
    void setOp(MemOp O) { Op = O; }

    /// ZIPMemOp: set the address
    void setAddr(uint64_t A) { Addr = A; }

    /// ZIPMemOp: set the size
    void setSize(uint32_t S) {Size = S; }

  private:
    MemOp Op;                       ///< ZIPMemOp: target memory operation
    uint64_t Addr;                  ///< ZIPMemOp: address
    uint32_t Size;                  ///< ZIPMemOp: size of the request
    ZIPMemTarget* Target;           ///< ZIPMemOp: memory target
  };

  // ------------------------------------------------------------
  // ZIPMemCtrl Base Subcomponent Class
  // ------------------------------------------------------------
  class ZIPMemCtrl : public SST::SubComponent {
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Forza::ZIPMemCtrl)

    SST_ELI_DOCUMENT_PARAMS(
      { "verbose", "Set the verbosity of output for the memory controller", "0" }
    )

    /// ZIPMemCtrl: constructor
    ZIPMemCtrl(ComponentId_t id, const Params& params)
      : SubComponent(id) {
      const int Verbosity = params.find<int>("verbose", 0);
      output.init("ZIPMemCtrl[" + getName() + ":@p:@t]: ",
                  Verbosity, 0, SST::Output::STDOUT);
    }

    /// ZIPMemCtrl: destructor
    virtual ~ZIPMemCtrl() = default;

    /// ZIPMemCtrl: initialization function
    virtual void init(unsigned int phase) = 0;

    /// ZIPMemCtrl: setup function
    virtual void setup() = 0;

    /// ZIPMemCtrl: finish function
    virtual void finish() = 0;

    // ZIPMemCtrl: handle a read response
    virtual void handleReadResp(StandardMem::ReadResp* ev) = 0;

    /// ZIPMemCtrl: handle a write response
    virtual void handleWriteResp(StandardMem::WriteResp* ev) = 0;

    /// ZIPMemCtrl: handle a flush response
    virtual void handleFlushResp(StandardMem::FlushResp* ev) = 0;

    /// ZIPMemCtrl: handle a custom response
    virtual void handleCustomResp(StandardMem::CustomResp* ev) = 0;

    /// ZIPMemCtrl: handle an invalidate response
    virtual void handleInvResp(StandardMem::InvNotify* ev) = 0;

    /// ZIPMemCtrl: send a read request
    virtual bool sendWRITERequest(uint64_t Addr, uint32_t Size, ZIPMemTarget* Target) = 0;

    /// ZIPMemCtrl: send a read request
    virtual bool sendREADRequest(uint64_t Addr, uint32_t Size, ZIPMemTarget* Target) = 0;

  protected:
    SST::Output output;       ///< ZIPMemCtrl: sst output object

  private:

  };  // class ZIPMemCtrl

  // ------------------------------------------------------------
  // ZIPBasicMemCtrl Inherited Subcomponent Class
  // ------------------------------------------------------------
  class ZIPBasicMemCtrl : public ZIPMemCtrl{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
      ZIPBasicMemCtrl,
      "ForzaZIP",
      "ZIPBasicMemCtrl",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Forza ZIP basic memory controller",
      SST::Forza::ZIPMemCtrl
    )

    SST_ELI_DOCUMENT_PARAMS(
      { "verbose",        "Set the verbosity of output for the memory controller",    "0" },
      { "clock",          "Sets the clock frequency of the memory conroller",         "1Ghz" },
      { "num_read",       "Sets the number of outstanding reads",                     "16" },
      { "num_write",      "Sets the number of outstanding write",                     "16" },
      { "ops_per_cycle",  "Sets the number of ops per cycle",                         "32" }
    )

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
      { "memIface", "Set the interface to memory", "SST::Interfaces::StandardMem" }
    )

    SST_ELI_DOCUMENT_PORTS()

    SST_ELI_DOCUMENT_STATISTICS(
      {"TotalNumReads",     "Total number of read operations",        "count", 1},
      {"TotalNumWrites",    "Total number of write operations",       "count", 1},
      {"OutstandingReads",  "Number of oustanding write operations",  "count", 1},
      {"OutstandingWrites", "Number of outstanding write operations", "count", 1}
    )

    // Public class members

    /// ZIPBasicMemCtrl: constructor
    ZIPBasicMemCtrl(ComponentId_t id, const Params& params);

    /// ZIPBasicMemCtrl: destructor
    virtual ~ZIPBasicMemCtrl();

    /// ZIPBasicMemCtrl: initialization function
    virtual void init(unsigned int phase) override;

    /// ZIPBasicMemCtrl: setup function
    virtual void setup() override;

    /// ZIPBasicMemCtrl: finish function
    virtual void finish() override;

    /// ZIPBasicMemCtrl: clock function
    bool clock(Cycle_t cycle);

    /// ZIPBasicMemCtrl: memory event processing handler
    void processMemEvent(StandardMem::Request* ev);

    /// ZIPBasicMemCtrl: handle a read response
    virtual void handleReadResp(StandardMem::ReadResp* ev) override;

    /// ZIPBasicMemCtrl: handle a write response
    virtual void handleWriteResp(StandardMem::WriteResp* ev) override;

    /// ZIPBasicMemCtrl: handle a flush response
    virtual void handleFlushResp(StandardMem::FlushResp* ev) override;

    /// ZIPBasicMemCtrl: handle a custom response
    virtual void handleCustomResp(StandardMem::CustomResp* ev) override;

    /// ZIPBasicMemCtrl: handle an invalidate response
    virtual void handleInvResp(StandardMem::InvNotify* ev) override;

    /// ZIPMemCtrl: send a read request
    virtual bool sendWRITERequest(uint64_t Addr, uint32_t Size, ZIPMemTarget* Target) override;

    /// ZIPMemCtrl: send a read request
    virtual bool sendREADRequest(uint64_t Addr, uint32_t Size, ZIPMemTarget* Target) override;

  protected:
    class ZIPStdMemHandlers : public Interfaces::StandardMem::RequestHandler {
    public:
      friend class ZIPBasicMemCtrl;

      /// ZIPStdMemHandlers: constructor
      ZIPStdMemHandlers(SST::Output* output, ZIPBasicMemCtrl *Ctrl)
        : Interfaces::StandardMem::RequestHandler(output), Ctrl(Ctrl){
      }

      /// ZIPStdMemHandlers: destructor
      virtual ~ZIPStdMemHandlers() = default;

      /// ZIPStdMemHandlers: handle read response
      virtual void handle(StandardMem::ReadResp* ev){
        Ctrl->handleReadResp(ev);
      }

      /// ZIPStdMemhandlers: handle write response
      virtual void handle(StandardMem::WriteResp* ev){
        Ctrl->handleWriteResp(ev);
      }

      /// ZIPStdMemHandlers: handle flush response
      virtual void handle(StandardMem::FlushResp* ev){
        Ctrl->handleFlushResp(ev);
      }

      /// ZIPStdMemHandlers: handle custom response
      virtual void handle(StandardMem::CustomResp* ev){
        Ctrl->handleCustomResp(ev);
      }

      /// ZIPStdMemHandlers: handle invalidate response
      virtual void handle(StandardMem::InvNotify* ev){
        Ctrl->handleInvResp(ev);
      }

    private:
      ZIPBasicMemCtrl *Ctrl;          ///< ZIPStdMemHandlers
    }; // class ZIPStdMemHandlers

  private:

    // Private methods

    /// ZIPBasicMemCtrl: process the next memory request
    bool processNextRqst( unsigned &t_max_ops );

    /// ZIPBasicMemCtrl: determine if there available injection slots
    bool isOpenSlots();

    /// ZIPBasicMemCtrl: is the target memory op available to send?
    bool isMemOpAvail(ZIPMemOp *op);

    /// ZIPBasicMemCtrl: build a StandardMem request for the current operation
    bool buildStandardMemRqst(ZIPMemOp *op, bool &success);

    // Private data
    StandardMem* memIface;                    ///< StandardMem memory interfaces
    ZIPStdMemHandlers *stdMemHandlers;        ///< StandardMem interface handlers
    unsigned LineSize;                        ///< Cache line size
    unsigned NumRead;                         ///< maximum number of outstanding reads
    unsigned NumWrite;                        ///< maximum number of outstanding writes
    unsigned OpsPerCycle;                     ///< number of ops per cycle to dispatch

    unsigned OutstandingReads;                ///< number of outstanding reads
    unsigned OutstandingWrites;               ///< number of outstanding writes

    std::vector<StandardMem::Request::id_t> requests; ///< outstanding StandardMem requests
    std::vector<ZIPMemOp *> rqstQ;                    ///< queued memory requests
    std::map<StandardMem::Request::id_t, ZIPMemOp *> outstanding;    ///< map of outstanding requests

    // Statistics
    // Statistic<uint64_t>* TotalReads;          ///< total number of reads
    // Statistic<uint64_t>* TotalWrites;         ///< total number of writes
    // Statistic<uint64_t>* OutReads;            ///< number of outstanding reads
    // Statistic<uint64_t>* OutWrites;           ///< number of outstanding writes

  }; // class ZIPBasicMemCtrl
} // namespace SST::ZIP

#endif

// EOF
