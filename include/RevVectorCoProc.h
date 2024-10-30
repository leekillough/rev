//
// _RevVectorCoProc_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVVECTORCOPROC_H_
#define _SST_REVCPU_REVVECTORCOPROC_H_

// -- C++ Headers

// -- RevCPU Headers
#include "RevCoProc.h"

namespace SST::RevCPU {

// ----------------------------------------
// RevVectorCoProc
// ----------------------------------------
class RevVectorCoProc : public RevCoProc {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RevVectorCoProc,
    "revcpu",
    "RevVectorCoProc",
    SST_ELI_ELEMENT_VERSION( 1, 0, 0 ),
    "RISC-V Rev Simple Co-Processor",
    SST::RevCPU::RevCoProc
  );

  // Set up parameters accesible from the python configuration
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Set the verbosity of output for the co-processor", "0" },
    { "clock", "Sets the clock frequency of the co-processor", "1Ghz" },
    { "ELEN", "Maximum number of bits of a vector element that any operation can produce or consume" },
    { "VLEN", "Max number of bits in a single vector register" },
  );

  // Register any subcomponents used by this element
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS();

  // Register any ports used with this element
  SST_ELI_DOCUMENT_PORTS();

  // Add statistics
  SST_ELI_DOCUMENT_STATISTICS( { "InstRetired", "Counts the total number of instructions retired by this coprocessor", "count", 1 }
  );

  // Enum for referencing statistics
  enum CoProcStats {
    InstRetired = 0,
  };

  /// RevVectorCoProc: constructor
  RevVectorCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RevVectorCoProc: destructor
  virtual ~RevVectorCoProc()                           = default;

  /// RevVectorCoProc: disallow copying and assignment
  RevVectorCoProc( const RevVectorCoProc& )            = delete;
  RevVectorCoProc& operator=( const RevVectorCoProc& ) = delete;

  /// RevVectorCoProc: clock tick function - currently not registeres with SST, called by RevCPU
  virtual bool ClockTick( SST::Cycle_t cycle );

  void registerStats();

  /// RevVectorCoProc: Enqueue Inst into the InstQ and return
  virtual bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst );

  /// RevVectorCoProc: Reset the co-processor by emmptying the InstQ
  virtual bool Reset();

  /// RevSimpleCoProv: Called when the attached RevCore completes simulation. Could be used to
  ///                   also signal to SST that the co-processor is done if ClockTick is registered
  ///                   to SSTCore vs. being driven by RevCPU
  virtual bool Teardown() { return Reset(); };

  /// RevVectorCoProc: Returns true if instruction queue is empty
  virtual bool IsDone() { return InstQ.empty(); }

private:
  struct RevCoProcInst {
    RevCoProcInst( uint32_t inst, const RevFeature* F, RevRegFile* R, RevMem* M )
      : Inst( inst ), Feature( F ), RegFile( R ), Mem( M ) {}

    uint32_t const          Inst;
    const RevFeature* const Feature;
    RevRegFile* const       RegFile;
    RevMem* const           Mem;
  };

  /// RevVectorCoProc: Total number of instructions retired
  Statistic<uint64_t>* num_instRetired{};

  /// Queue of instructions sent from attached RevCore
  std::queue<RevCoProcInst> InstQ{};

  SST::Cycle_t cycleCount{};

  // VLEN=128, ELEN=64
  uint64_t vreg[32][2] = { { 0 } };

public:
  /// RevVectorCoProc: Vector Execution Mock-up
  void FauxExec( RevCoProcInst rec );

};  //class RevVectorCoProc

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_REVVECTORCOPROC_H_
