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
    { "ELEN", "Maximum number of bits of a vector element that any operation can produce or consume", "64" },
    { "VLEN", "Max number of bits in a single vector register", "128" },
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
  virtual ~RevVectorCoProc();

  /// RevVectorCoProc: disallow copying and assignment
  RevVectorCoProc( const RevVectorCoProc& )            = delete;
  RevVectorCoProc& operator=( const RevVectorCoProc& ) = delete;

  /// RevVectorCoProc: clock tick function - currently not registeres with SST, called by RevCPU
  virtual bool ClockTick( SST::Cycle_t cycle );

  void registerStats();

  /// RevVectorCoProc: Decode valid vector coprocessor instructions
  bool Decode( const uint32_t inst );

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
  std::unordered_map<unsigned, std::pair<unsigned, unsigned>> EntryToExt{
  };  ///< RevVectorCoProc: instruction entry to extension mapping

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

  RevVRegFile* vregfile = nullptr;

  // Quick Decode helpers for special CSR access
  union csr_inst_t {
    uint32_t v = 0x0;

    struct {
      uint32_t opcode : 7;   // [6:0]   SYSTEM
      uint32_t rd     : 5;   // [11:7]  dest
      uint32_t funct3 : 3;   // [14:12] 001:CSRRW
                             //         010:CSRRS
                             //         011:CSRRC
                             //         101:CSRRWI
                             //         110:CSRRSI
                             //         111:CSRRCI
      uint32_t rs1    : 5;   // [19:15] source, uimm[4:0]
      uint32_t csr    : 12;  // [31:20] src/dest csr
    } f;

    csr_inst_t( uint32_t _v ) : v( _v ) {};
  };

  const uint8_t csr_inst_opcode = 0b1110011;

  enum csr_inst_funct3 : uint8_t {
    csrrw  = 0b001,
    csrrs  = 0b010,
    csrrc  = 0b011,
    csrrwi = 0b101,
    csrrsi = 0b110,
    csrrci = 0b111,
  };

  std::map<uint16_t, uint64_t> csrmap{
    // URW
    {0x008,             0}, // vstart
    {0x009,             0}, // vxsat
    {0x00a,             0}, // vxrm
    {0x00f,             0}, // vxcsr
    // URO
    {0xc20,             0}, // vl
    {0xc21,    1ULL << 63}, // vtype
    {0xc22, 128ULL / 8ULL}, // vlenb
  };

public:
  /// RevVectorCoProc: Vector Execution Mock-up
  void Exec( RevCoProcInst rec );

};  //class RevVectorCoProc

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_REVVECTORCOPROC_H_
