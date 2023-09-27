//
// _rza_memctrl_h_
//
//

#ifndef _ZEN_MEMCTRL_H_
#define _ZEN_MEMCTRL_H_

// -- CXX Headers

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
    };
  private:
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
      { "mzop_per_cycle", "Sets the number of MZOPs to dispatch per cycle",           "16"},
      { "hzop_per_cycle", "Sets the number of HZOPs to dispatch per cycle",           "16"}
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

    // Private data
    StandardMem* memIface;                    ///< StandardMem memory interfaces
    ZENStdMemHandlers *stdMemHandlers;        ///< StandardMem interface handlers
    unsigned LineSize;                        ///< Cache line size

  }; // class ZENBasicMemCtrl
} // namespace SST::ZEN

#endif

// EOF
