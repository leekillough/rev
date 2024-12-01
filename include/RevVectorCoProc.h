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
#include "RevVRegFile.h"
#include <cinttypes>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

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
    { "elen", "Maximum number of bits of a vector element that any operation can produce or consume", "64" },
    { "vlen", "Max number of bits in a single vector register", "128" },
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
  ~RevVectorCoProc() final                             = default;

  /// RevVectorCoProc: disallow copying and assignment
  RevVectorCoProc( const RevVectorCoProc& )            = delete;
  RevVectorCoProc& operator=( const RevVectorCoProc& ) = delete;

  /// RevVectorCoProc: clock tick function - currently not registeres with SST, called by RevCPU
  bool ClockTick( SST::Cycle_t cycle ) final;

  void registerStats();

  /// RevVectorCoProc: Decode valid vector coprocessor instructions
  bool Decode( const uint32_t inst );

  /// RevVectorCoProc: Enqueue Inst into the InstQ and return
  bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) final;

  /// RevVectorCoProc: Reset the co-processor by emmptying the InstQ
  bool Reset() final;

  /// RevSimpleCoProv: Called when the attached RevCore completes simulation. Could be used to
  ///                   also signal to SST that the co-processor is done if ClockTick is registered
  ///                   to SSTCore vs. being driven by RevCPU
  bool Teardown() final { return Reset(); };

  /// RevVectorCoProc: Returns true if instruction queue is empty
  bool IsDone() final { return InstQ.empty(); }

private:
  uint16_t   vlen    = 0;
  uint16_t   elen    = 0;
  RevVecInst VecInst = {};

  struct RevCoProcInst {
    RevCoProcInst( RevVecInst D, const RevFeature* F, RevRegFile* R, RevMem* M )
      : VecInst( D ), Feature( F ), RegFile( R ), Mem( M ) {}

    RevVecInst              VecInst;
    const RevFeature* const Feature;
    RevRegFile* const       RegFile;
    RevMem* const           Mem;
  };

  std::vector<RevVecInstEntry>                InstTable{};    ///< RevVectorCoProc: vector instruction table
  std::vector<std::unique_ptr<RevExt>>        Extensions{};   ///< RevVectorCoProc: vector of enabled extensions
  std::unordered_multimap<uint64_t, unsigned> EncToEntry{};   ///< RevVectorCoProc: instruction encoding to table entry mapping
  std::unordered_map<std::string, unsigned>   NameToEntry{};  ///< RevVectorCoProc: instruction mnemonic to table entry mapping
  std::unordered_map<unsigned, std::pair<unsigned, unsigned>>
    EntryToExt{};  ///< RevVectorCoProc: instruction entry to extension mapping

  Statistic<uint64_t>*      num_instRetired{};  ///< RevVectorCoProc: Total number of instructions retired
  std::queue<RevCoProcInst> InstQ{};            ///< RevVectorCoProc: Queue of instructions sent from attached RevCore
  SST::Cycle_t              cycleCount{};       ///< RevVectorCoProc: Cycles executed

  bool LoadInstructionTable();   ///< RevVectorCoProc: Loads the vector instruction table
  bool SeedInstructionTable();   ///< RevVectorCoProc: stage 1 loading initial tables
  bool EnableExt( RVVec* Ext );  ///< RevVectorCoProc: merge extension into main instruction table
  bool InitTableMapping();       ///< RevVectorCoProc: initializes the internal mapping tables

  std::string ExtractMnemonic( const RevVecInstEntry& Entry
  );  ///< RevVectorCoProc: extracts the instruction mnemonic from the table entry
  uint32_t    CompressEncoding( const RevVecInstEntry& Entry );  ///< RevCore: compressed the encoding structure to a single value
  auto        matchInst(  ///< RevVectorCoProc: finds an entry which matches an encoding whose predicate is true
    const std::unordered_multimap<uint64_t, unsigned>& map,
    uint64_t                                           encoding,
    const std::vector<RevVecInstEntry>&                InstTable,
    uint32_t                                           Inst
  ) const;

  std::unique_ptr<RevVRegFile> vregfile{};

public:
  /// RevVectorCoProc: Vector Execution Mock-up
  void Exec( RevCoProcInst rec );

};  //class RevVectorCoProc

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_REVVECTORCOPROC_H_
