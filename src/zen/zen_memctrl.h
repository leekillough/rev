//
// _rza_memctrl_h_
//
//

#ifndef _ZEN_MEMCTRL_H_
#define _ZEN_MEMCTRL_H_

// -- CXX Headers
#include <vector>
#include <map>

// -- SST Headers
#include "zen_sst.h"

namespace SST::Forza{

  using namespace SST::Interfaces;

  // ------------------------------------------------------------
  // ZENMemOp Class
  // ------------------------------------------------------------
  class ZENMemOp{
  public:
    enum MemOp{
      ZEN_MemOpREAD       = 0,
      ZEN_MemOpWRITE      = 1,
    };

    /// ZENMemOp: constructor
    ZENMemOp(MemOp Op, uint64_t Addr, uint32_t Size)
      : Op(Op), Addr(Addr), Size(Size){}

    /// ZENMemOp: overloaded constructor
    ZENMemOp(MemOp Op, uint64_t Addr, uint32_t Size,
             std::vector<uint8_t> buffer)
      : Op(Op), Addr(Addr), Size(Size), membuf(buffer){}

    /// ZENMemOp: destructor
    ~ZENMemOp() = default;

    /// ZENMemOp: retrieve the operation type
    uint64_t getOp() const { return Op; }

    /// ZENMemOp: retrieve the address
    uint64_t getAddr() const { return Addr; }

    /// ZENMemOp: retrieve the size
    uint32_t getSize() const { return Size; }

    /// ZENMemOp: retrieve the buffer
    std::vector<uint8_t> getBuf() const { return membuf; }

    /// ZENMemOp: set the operation type
    void setOp(MemOp O) { Op = O; }

    /// ZENMemOp: set the address
    void setAddr(uint64_t A) { Addr = A; }

    /// ZENMemOp: set the size
    void setSize(uint32_t S) {Size = S; }

  private:
    MemOp Op;                       ///< ZENMemOp: target memory operation
    uint64_t Addr;                  ///< ZENMemOp: address
    uint32_t Size;                  ///< ZENMemOp: size of the request
    std::vector<uint8_t> membuf;    ///< ZENMemOp: memory buffer
  };

  // ------------------------------------------------------------
  // ZENMemCtrl Base Subcomponent Class
  // ------------------------------------------------------------
  class ZENMemCtrl : public SST::SubComponent {
  public:
    SST_ELI_REGISTER_SUBCOMPONENT_API(SST::Forza::ZENMemCtrl)

    SST_ELI_DOCUMENT_PARAMS(
      { "verbose", "Set the verbosity of output for the memory controller", "0" }
    )

    /// ZENMemCtrl: constructor
    ZENMemCtrl(ComponentId_t id, const Params& params)
      : SubComponent(id) {
      const int Verbosity = params.find<int>("verbose", 0);
      output.init("ZENMemCtrl[" + getName() + ":@p:@t]: ",
                  Verbosity, 0, SST::Output::STDOUT);
    }

    /// ZENMemCtrl: destructor
    virtual ~ZENMemCtrl() = default;

    /// ZENMemCtrl: initialization function
    virtual void init(unsigned int phase) = 0;

    /// ZENMemCtrl: setup function
    virtual void setup() = 0;

    /// ZENMemCtrl: finish function
    virtual void finish() = 0;

    // ZENMemCtrl: handle a read response
    virtual void handleReadResp(StandardMem::ReadResp* ev) = 0;

    /// ZENMemCtrl: handle a write response
    virtual void handleWriteResp(StandardMem::WriteResp* ev) = 0;

    /// ZENMemCtrl: handle a flush response
    virtual void handleFlushResp(StandardMem::FlushResp* ev) = 0;

    /// ZENMemCtrl: handle a custom response
    virtual void handleCustomResp(StandardMem::CustomResp* ev) = 0;

    /// ZENMemCtrl: handle an invalidate response
    virtual void handleInvResp(StandardMem::InvNotify* ev) = 0;

    /// ZENMemCtrl: send a read request
    virtual bool sendWRITERequest(uint64_t Addr, uint32_t Size,
                                  std::vector<uint8_t> buf) = 0;

    /// ZENMemCtrl: send a read request
    virtual bool sendREADRequest(uint64_t Addr, uint32_t Size) = 0;

  protected:
    SST::Output output;       ///< ZENMemCtrl: sst output object

  private:

  };  // class ZENMemCtrl

  // ------------------------------------------------------------
  // ZENBasicMemCtrl Inherited Subcomponent Class
  // ------------------------------------------------------------
  class ZENBasicMemCtrl : public ZENMemCtrl{
  public:
    SST_ELI_REGISTER_SUBCOMPONENT(
      ZENBasicMemCtrl,
      "ForzaZEN",
      "ZENBasicMemCtrl",
      SST_ELI_ELEMENT_VERSION(1, 0, 0),
      "Forza ZEN basic memory controller",
      SST::Forza::ZENMemCtrl
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

    SST_ELI_DOCUMENT_STATISTICS()

    // Public class members

    /// ZENBasicMemCtrl: constructor
    ZENBasicMemCtrl(ComponentId_t id, const Params& params);

    /// ZENBasicMemCtrl: destructor
    virtual ~ZENBasicMemCtrl();

    /// ZENBasicMemCtrl: initialization function
    virtual void init(unsigned int phase) override;

    /// ZENBasicMemCtrl: setup function
    virtual void setup() override;

    /// ZENBasicMemCtrl: finish function
    virtual void finish() override;

    /// ZENBasicMemCtrl: clock function
    bool clock(Cycle_t cycle);

    /// ZENBasicMemCtrl: memory event processing handler
    void processMemEvent(StandardMem::Request* ev);

    /// ZENBasicMemCtrl: handle a read response
    virtual void handleReadResp(StandardMem::ReadResp* ev) override;

    /// ZENBasicMemCtrl: handle a write response
    virtual void handleWriteResp(StandardMem::WriteResp* ev) override;

    /// ZENBasicMemCtrl: handle a flush response
    virtual void handleFlushResp(StandardMem::FlushResp* ev) override;

    /// ZENBasicMemCtrl: handle a custom response
    virtual void handleCustomResp(StandardMem::CustomResp* ev) override;

    /// ZENBasicMemCtrl: handle an invalidate response
    virtual void handleInvResp(StandardMem::InvNotify* ev) override;

    /// ZENMemCtrl: send a read request
    virtual bool sendWRITERequest(uint64_t Addr, uint32_t Size,
                                  std::vector<uint8_t> buf) override;

    /// ZENMemCtrl: send a read request
    virtual bool sendREADRequest(uint64_t Addr, uint32_t Size) override;

  protected:
    class ZENStdMemHandlers : public Interfaces::StandardMem::RequestHandler {
    public:
      friend class ZENBasicMemCtrl;

      /// ZENStdMemHandlers: constructor
      ZENStdMemHandlers(SST::Output* output, ZENBasicMemCtrl *Ctrl)
        : Interfaces::StandardMem::RequestHandler(output), Ctrl(Ctrl){
      }

      /// ZENStdMemHandlers: destructor
      virtual ~ZENStdMemHandlers() = default;

      /// ZENStdMemHandlers: handle read response
      virtual void handle(StandardMem::ReadResp* ev){
        Ctrl->handleReadResp(ev);
      }

      /// ZENStdMemhandlers: handle write response
      virtual void handle(StandardMem::WriteResp* ev){
        Ctrl->handleWriteResp(ev);
      }

      /// ZENStdMemHandlers: handle flush response
      virtual void handle(StandardMem::FlushResp* ev){
        Ctrl->handleFlushResp(ev);
      }

      /// ZENStdMemHandlers: handle custom response
      virtual void handle(StandardMem::CustomResp* ev){
        Ctrl->handleCustomResp(ev);
      }

      /// ZENStdMemHandlers: handle invalidate response
      virtual void handle(StandardMem::InvNotify* ev){
        Ctrl->handleInvResp(ev);
      }

    private:
      ZENBasicMemCtrl *Ctrl;          ///< ZENStdMemHandlers
    }; // class ZENStdMemHandlers

  private:

    // Private methods

    /// ZENBasicMemCtrl: process the next memory request
    bool processNextRqst( unsigned &t_max_ops );

    /// ZENBasicMemCtrl: determine if there available injection slots
    bool isOpenSlots();

    /// ZENBasicMemCtrl: is the target memory op available to send?
    bool isMemOpAvail(ZENMemOp *op);

    /// ZENBasicMemCtrl: build a StandardMem request for the current operation
    bool buildStandardMemRqst(ZENMemOp *op, bool &success);

    // Private data
    StandardMem* memIface;                    ///< StandardMem memory interfaces
    ZENStdMemHandlers *stdMemHandlers;        ///< StandardMem interface handlers
    unsigned LineSize;                        ///< Cache line size
    unsigned NumRead;                         ///< maximum number of outstanding reads
    unsigned NumWrite;                        ///< maximum number of outstanding writes
    unsigned OpsPerCycle;                     ///< number of ops per cycle to dispatch

    unsigned OutstandingReads;                ///< number of outstanding reads
    unsigned OutstandingWrites;               ///< number of outstanding writes

    std::vector<StandardMem::Request::id_t> requests; ///< outstanding StandardMem requests
    std::vector<ZENMemOp *> rqstQ;                    ///< queued memory requests
    std::map<StandardMem::Request::id_t, ZENMemOp *> outstanding;    ///< map of outstanding requests

  }; // class ZENBasicMemCtrl
} // namespace SST::ZEN

#endif

// EOF
