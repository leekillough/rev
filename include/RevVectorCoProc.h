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

  /// Quick Decode helpers for special CSR access
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
  void FauxExec( RevCoProcInst rec );

};  //class RevVectorCoProc

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_REVVECTORCOPROC_H_
