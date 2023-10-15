//
// _RevCoProc_h_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVCOPROC_H_
#define _SST_REVCPU_REVCOPROC_H_

// -- C++ Headers
#include <ctime>
#include <vector>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <random>
#include <queue>
#include <tuple>

// -- SST Headers
#include "SST.h"

// -- RevCPU Headers
#include "RevOpts.h"
#include "RevFeature.h"
#include "RevMem.h"
#include "RevInstTable.h"
#include "RevProcPasskey.h"
#include "RevProc.h"

namespace SST::RevCPU{
class RevProc;
// ----------------------------------------
// RevCoProc
// ----------------------------------------
class RevCoProc : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API(SST::RevCPU::RevCoProc, RevProc*);
  SST_ELI_DOCUMENT_PARAMS({ "verbose", "Set the verbosity of output for the attached co-processor", "0" });

  /// RevCoProc: Constructor
  RevCoProc( ComponentId_t id, Params& params, RevProc* parent);

  /// RevCoProc: default destructor
  virtual ~RevCoProc();                               ///< RevCoProc: Destructor
  virtual bool IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst) = 0;          /// RevCoProc: Instruction Interface to RevCore
  virtual bool Reset() = 0;                           ///< RevCoProc: Reset - called on startup
  virtual bool Teardown() = 0;                        ///< RevCoProc: Teardown - called when associated RevProc completes
  virtual bool ClockTick(SST::Cycle_t cycle) = 0;     ///< RevCoProc: Clock - can be called by SST or by overriding RevCPU
  virtual bool IsDone() = 0;                          ///< RevCoProc: Returns true when co-processor has completed execution - used for proper exiting of associated RevProc

protected:
  SST::Output*   output;                                ///< RevCoProc: sst output object
  RevProc* const parent;                                  ///< RevCoProc: Pointer to RevProc this CoProc is attached to

  ///< RevCoProc: Create the passkey object - this allows access to otherwise private members within RevProc 
  RevProcPasskey<RevCoProc> CreatePasskey(){return RevProcPasskey<RevCoProc>();}
}; // class RevCoProc

// ----------------------------------------
// RevSimpleCoProc
// ----------------------------------------
class RevSimpleCoProc : public RevCoProc{
public:
  SST_ELI_REGISTER_SUBCOMPONENT(RevSimpleCoProc, "revcpu",
                                "RevSimpleCoProc",
                                SST_ELI_ELEMENT_VERSION(1, 0, 0),
                                "RISC-V Rev Simple Co-Processor",
                                SST::RevCPU::RevCoProc
    );

  // Set up parameters accesible from the python configuration
  SST_ELI_DOCUMENT_PARAMS({ "verbose",        "Set the verbosity of output for the co-processor",     "0" },
                          { "clock",          "Sets the clock frequency of the co-processor",         "1Ghz" },
    );

  // Register any subcomponents used by this element
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS();

  // Register any ports used with this element
  SST_ELI_DOCUMENT_PORTS();

  // Add statistics
  SST_ELI_DOCUMENT_STATISTICS(
    {"InstRetired",        "Counts the total number of instructions retired by this coprocessor",    "count", 1}
    );

  // Enum for referencing statistics
  enum CoProcStats{
    InstRetired = 0,
  };

  /// RevSimpleCoProc: constructor
  RevSimpleCoProc(ComponentId_t id, Params& params, RevProc* parent);

  /// RevSimpleCoProc: destructor
  virtual ~RevSimpleCoProc();

  /// RevSimpleCoProc: clock tick function - currently not registeres with SST, called by RevCPU
  virtual bool ClockTick(SST::Cycle_t cycle);

  void registerStats();

  /// RevSimpleCoProc: Enqueue Inst into the InstQ and return
  virtual bool IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst);

  /// RevSimpleCoProc: Reset the co-processor by emmptying the InstQ
  virtual bool Reset();

  /// RevSimpleCoProv: Called when the attached RevProc completes simulation. Could be used to
  ///                   also signal to SST that the co-processor is done if ClockTick is registered
  ///                   to SSTCore vs. being driven by RevCPU
  virtual bool Teardown() { return Reset(); };

  /// RevSimpleCoProc: Returns true if instruction queue is empty
  virtual bool IsDone(){ return InstQ.empty();}

private:
  struct RevCoProcInst {
    RevCoProcInst() = default;
    RevCoProcInst(uint32_t inst, RevFeature* F, RevRegFile* R, RevMem* M) :
      Inst(inst), Feature(F), RegFile(R), Mem(M) {}
    RevCoProcInst(const RevCoProcInst& rhs) = default;

    uint32_t      Inst = 0;
    RevFeature*   Feature = nullptr;
    RevRegFile*   RegFile = nullptr;
    RevMem*       Mem = nullptr;
  };

  /// RevSimpleCoProc: Total number of instructions retired
  Statistic<uint64_t>* num_instRetired;

  /// Queue of instructions sent from attached RevProc
  std::queue<RevCoProcInst> InstQ;

  SST::Cycle_t cycleCount;

}; //class RevSimpleCoProc

// ----------------------------------------
// RZALSCoProc
// ----------------------------------------
class RZALSCoProc: public RevCoProc{
public:
  // Subcomponent info
  SST_ELI_REGISTER_SUBCOMPONENT(RZALSCoProc, "revcpu",
                                "RZALSCoProc",
                                SST_ELI_ELEMENT_VERSION(1,0,0),
                                "FORZA RZA Load/Store CoProc",
                                SST::RevCPU::RevCoProc)
  // Register the paramaters
  SST_ELI_DOCUMENT_PARAMS( {"verbose",    "Sets the verbosity",       "0" },
                           {"clock",      "Sets the clock frequency", "1Ghz"},
                         )

  // Register subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  // Register any ports
  SST_ELI_DOCUMENT_PORTS()

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS()


  /// RZALSCoProc: default constructor
  RZALSCoProc(ComponentId_t id, Params& params, RevProc* parent);

  /// RZALSCoProc: default destructor
  virtual ~RZALSCoProc();

  /// RZALSCoProc: clock tick function
  virtual bool ClockTick(SST::Cycle_t cycle);

  /// RZALSCoProc: Enqueue a new instruction
  virtual bool IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst);

  /// RZALSCoProc: reset the coproc
  virtual bool Reset();

  /// RZALSCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() { return Reset(); }

  /// RZALSCoProc: determines whether the coproc is complete
  virtual bool IsDone();

private:
};  // RZALSCoProc

// ----------------------------------------
// RZAAMOCoProc
// ----------------------------------------
class RZAAMOCoProc: public RevCoProc{
public:
  // Subcomponent info
  SST_ELI_REGISTER_SUBCOMPONENT(RZAAMOCoProc, "revcpu",
                                "RZAAMOCoProc",
                                SST_ELI_ELEMENT_VERSION(1,0,0),
                                "FORZA RZA Load/Store CoProc",
                                SST::RevCPU::RevCoProc)
  // Register the paramaters
  SST_ELI_DOCUMENT_PARAMS( {"verbose",    "Sets the verbosity",       "0" },
                           {"clock",      "Sets the clock frequency", "1Ghz"},
                         )

  // Register subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  // Register any ports
  SST_ELI_DOCUMENT_PORTS()

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS()


  /// RZAAMOCoProc: default constructor
  RZAAMOCoProc(ComponentId_t id, Params& params, RevProc* parent);

  /// RZAAMOCoProc: default destructor
  virtual ~RZAAMOCoProc();

  /// RZAAMOCoProc: clock tick function
  virtual bool ClockTick(SST::Cycle_t cycle);

  /// RZAAMOCoProc: Enqueue a new instruction
  virtual bool IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst);

  /// RZAAMOCoProc: reset the coproc
  virtual bool Reset();

  /// RZAAMOCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() { return Reset(); }

  /// RZAAMOCoProc: determines whether the coproc is complete
  virtual bool IsDone();

private:
};  // RZAAMOCoProc

} //namespace SST::RevCPU

#endif
