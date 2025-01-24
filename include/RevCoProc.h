//
// _RevCoProc_h_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVCOPROC_H_
#define _SST_REVCPU_REVCOPROC_H_

// -- C++ Headers
#include <functional>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

// -- SST Headers
#include "SST.h"

// -- RevCPU Headers
#include "RevCore.h"
#include "RevCorePasskey.h"
#include "RevFeature.h"
#include "RevInstTable.h"
#include "RevMem.h"
#include "RevOpts.h"

#include "ZOPNET.h"

namespace SST::RevCPU {

using namespace SST::Forza;

inline constexpr size_t RA_NUM_REG = 4096;
inline constexpr size_t UNDEF_REG  = RA_NUM_REG + 1;

enum class Hazard : uint8_t { CLEAR = 0, SET, DIRTY };

// ----------------------------------------
// RegAlloc
// ----------------------------------------
struct RegAlloc {
  /// RegAlloc: retrieve a single operand register
  bool getRegs( uint32_t& rs1 ) {
    for( uint32_t cur = 0; cur < RA_NUM_REG; ++cur ) {
      if( hazard[cur] == Hazard::CLEAR ) {
        rs1         = cur;
        hazard[cur] = Hazard::SET;
        return true;
      }
    }
    return false;
  }

  /// RegAlloc: retrieve a two-or-more operand register set
  template<typename... Ts>
  bool getRegs( uint32_t& rs1, uint32_t& rs2, Ts&... rest ) {
    uint32_t t_rs1;
    if( getRegs( t_rs1 ) && getRegs( rs2, rest... ) ) {
      rs1 = t_rs1;
      return true;
    }
    return false;
  }

  /// RegAlloc: clear the hazard on the target register
  void clearReg( uint32_t reg ) {
    if( reg < RA_NUM_REG ) {
      hazard[reg] = Hazard::CLEAR;
      regs[reg]   = 0;
    }
  }

  /// RegAlloc: set the target value
  void SetX( uint32_t Idx, uint64_t Val ) {
    if( Idx < RA_NUM_REG ) {
      regs[Idx] = Val;
    }
  }

  /// RegAlloc: get the target value
  uint64_t GetX( uint32_t Idx ) const { return Idx < RA_NUM_REG ? regs[Idx] : 0; }

  /// RegAlloc: get the address for the target reigster
  const uint64_t* getRegAddr( uint32_t Idx ) const { return Idx < RA_NUM_REG ? &regs[Idx] : nullptr; }

  uint64_t* getRegAddr( uint32_t Idx ) { return Idx < RA_NUM_REG ? &regs[Idx] : nullptr; }

  /// RegAlloc: get the hazard state
  Hazard getState( uint32_t Idx ) const { return Idx < RA_NUM_REG ? hazard[Idx] : Hazard::DIRTY; }

  /// RegAlloc: set the target register as dirty
  void setDirty( uint32_t Idx ) {
    if( Idx < RA_NUM_REG ) {
      hazard[Idx] = Hazard::DIRTY;
    }
  }

private:
  uint64_t regs[RA_NUM_REG]{};  ///< RegAlloc: register array
  Hazard   hazard[RA_NUM_REG]{};
};

// ----------------------------------------
// RevCoProc
// ----------------------------------------
class RevCoProc : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::RevCPU::RevCoProc, RevCore* );
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Set the verbosity of output for the attached co-processor", "0" } );

  // --------------------
  // Virtual methods
  // --------------------

  /// RevCoProc: Constructor
  RevCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RevCoProc: default destructor
  virtual ~RevCoProc();

  /// RevCoProc: disallow copying and assignment
  RevCoProc( const RevCoProc& )            = delete;
  RevCoProc& operator=( const RevCoProc& ) = delete;

  /// RevCoProc: send raw data to the coprocessor
  virtual bool sendRawData( std::vector<uint8_t> Data ) { return true; }

  /// RevCoProc: retrieve raw data from the coprocessor
  virtual std::vector<uint8_t> getRawData() {
    output->fatal( CALL_INFO, -1, "Error : no override method defined for getRawData()\n" );

    // inserting code to quiesce warnings
    return {};
  }

  // --------------------
  // Pure virtual methods
  // --------------------

  /// RevCoProc: Instruction interface to RevCore
  virtual bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) = 0;

  /// ReCoProc: Reset - called on startup
  virtual bool Reset()                                                                   = 0;

  /// RevCoProc: Teardown - called when associated RevCore completes
  virtual bool Teardown()                                                                = 0;

  /// RevCoProc: Clock - can be called by SST or by overriding RevCPU
  virtual bool ClockTick( SST::Cycle_t cycle )                                           = 0;

  /// RevCoProc: Returns true when co-processor has completed execution
  ///            - used for proper exiting of associated RevCore
  virtual bool IsDone()                                                                  = 0;

  // --------------------
  // FORZA virtual methods
  // --------------------
  /// RevCoProc: injects a zop packet into the coproc pipeline
  virtual bool InjectZOP( Forza::zopEvent* zev, bool& flag ) { return true; }

  /// RevCoProc: Set the memory handler
  virtual void setMem( RevMem* M ) {}

  /// RevCoProc: Set the ZOP NIC handler
  virtual void setZNic( Forza::zopAPI* Z ) {}

  // --------------------
  // FORZA methods
  // --------------------
  /// RevCoProc: Sends a successful response ZOP
  bool sendSuccessResp( Forza::zopAPI* zNic, Forza::zopEvent* zev, uint16_t Hart );

  /// RevCoProc: Sends a successful LOAD data response ZOP
  bool sendSuccessResp( Forza::zopAPI* zNic, Forza::zopEvent* zev, uint16_t Hart, uint64_t Data );

  /// RevCoProc: Virtual mark load complete method
  virtual void MarkLoadComplete( const MemReq& req ) {}

protected:
  SST::Output*   output{};  ///< RevCoProc: sst output object
  RevCore* const parent;    ///< RevCoProc: Pointer to RevCore this CoProc is attached to

  ///< RevCoProc: Create the passkey object - this allows access to otherwise private members within RevCore
  RevCorePasskey<RevCoProc> CreatePasskey() { return RevCorePasskey<RevCoProc>(); }
};  // class RevCoProc

// ----------------------------------------
// RevSimpleCoProc
// ----------------------------------------
class RevSimpleCoProc final : public RevCoProc {
public:
  SST_ELI_REGISTER_SUBCOMPONENT(
    RevSimpleCoProc,
    "revcpu",
    "RevSimpleCoProc",
    SST_ELI_ELEMENT_VERSION( 1, 0, 0 ),
    "RISC-V Rev Simple Co-Processor",
    SST::RevCPU::RevCoProc
  );

  // Set up parameters accesible from the python configuration
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Set the verbosity of output for the co-processor", "0" },
    { "clock", "Sets the clock frequency of the co-processor", "1Ghz" },
  );

  // Register any subcomponents used by this element
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS();

  // Register any ports used with this element
  SST_ELI_DOCUMENT_PORTS();

  // Add statistics
  SST_ELI_DOCUMENT_STATISTICS( { "InstRetired", "Counts the total number of instructions retired by this coprocessor", "count", 1 }
  );

  // Enum for referencing statistics
  enum class CoProcStats {
    InstRetired = 0,
  };

  /// RevSimpleCoProc: constructor
  RevSimpleCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RevSimpleCoProc: destructor
  ~RevSimpleCoProc() final                             = default;

  /// RevSimpleCoProc: disallow copying and assignment
  RevSimpleCoProc( const RevSimpleCoProc& )            = delete;
  RevSimpleCoProc& operator=( const RevSimpleCoProc& ) = delete;

  /// RevSimpleCoProc: clock tick function - currently not registered with SST, called by RevCPU
  bool ClockTick( SST::Cycle_t cycle ) final;

  void registerStats();

  /// RevSimpleCoProc: Enqueue Inst into the InstQ and return
  bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) final;

  /// RevSimpleCoProc: Reset the co-processor by emmptying the InstQ
  bool Reset() final;

  /// RevSimpleCoProc: Called when the attached RevCore completes simulation. Could be used to
  ///                   also signal to SST that the co-processor is done if ClockTick is registered
  ///                   to SSTCore vs. being driven by RevCPU
  bool Teardown() final { return Reset(); };

  /// RevSimpleCoProc: Returns true if instruction queue is empty
  bool IsDone() final { return InstQ.empty(); }

private:
  struct RevCoProcInst {
    RevCoProcInst( uint32_t inst, const RevFeature* F, RevRegFile* R, RevMem* M )
      : Inst( inst ), Feature( F ), RegFile( R ), Mem( M ) {}

    uint32_t const          Inst;
    const RevFeature* const Feature;
    RevRegFile* const       RegFile;
    RevMem* const           Mem;
  };

  /// RevSimpleCoProc: Total number of instructions retired
  Statistic<uint64_t>* num_instRetired{};

  /// Queue of instructions sent from attached RevCore
  std::queue<RevCoProcInst> InstQ{};

  SST::Cycle_t cycleCount{};

};  //class RevSimpleCoProc

// ----------------------------------------
// RZALSCoProc
// ----------------------------------------
class RZALSCoProc final : public RevCoProc {
public:
  // Subcomponent info
  SST_ELI_REGISTER_SUBCOMPONENT(
    RZALSCoProc, "revcpu", "RZALSCoProc", SST_ELI_ELEMENT_VERSION( 1, 0, 0 ), "FORZA RZA Load/Store CoProc", SST::RevCPU::RevCoProc
  )
  // Register the paramaters
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Sets the verbosity", "0" }, { "clock", "Sets the clock frequency", "1Ghz" }, )

  // Register subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  // Register any ports
  SST_ELI_DOCUMENT_PORTS()

#define MZOP_STAT( stat ) { "MZOP_" #stat, "MZOP " #stat " Requests", "count", 1 }

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS(
    MZOP_STAT( LB ),
    MZOP_STAT( LH ),
    MZOP_STAT( LW ),
    MZOP_STAT( LD ),
    MZOP_STAT( LSB ),
    MZOP_STAT( LSH ),
    MZOP_STAT( LSW ),
    MZOP_STAT( LDMA ),
    MZOP_STAT( SB ),
    MZOP_STAT( SH ),
    MZOP_STAT( SW ),
    MZOP_STAT( SD ),
    MZOP_STAT( SSB ),
    MZOP_STAT( SSH ),
    MZOP_STAT( SSW ),
    MZOP_STAT( SDMA ),
  )

  enum class mzopStats : uint32_t {
    MZOP_LB,
    MZOP_LH,
    MZOP_LW,
    MZOP_LD,
    MZOP_LSB,
    MZOP_LSH,
    MZOP_LSW,
    MZOP_LDMA,
    MZOP_SB,
    MZOP_SH,
    MZOP_SW,
    MZOP_SD,
    MZOP_SSB,
    MZOP_SSH,
    MZOP_SSW,
    MZOP_SDMA,
    MZOP_END
  };

  /// RZALSCoProc: default constructor
  RZALSCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RZALSCoProc: default destructor
  virtual ~RZALSCoProc();

  /// RZALSCoProc: clock tick function
  bool ClockTick( SST::Cycle_t cycle ) override;

  /// RZALSCoProc: Enqueue a new instruction
  bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) override;

  /// RZALSCoProc: reset the coproc
  bool Reset() override;

  /// RZALSCoProc: teardown function when the attached Proc is complete
  bool Teardown() override { return Reset(); }

  /// RZALSCoProc: determines whether the coproc is complete
  bool IsDone() override;

  /// RZALSCoProc: injects a packet into the HZOP AMO pipeline
  bool InjectZOP( Forza::zopEvent* zev, bool& flag ) override;

  /// RZALSCoProc: marks a load as being complete
  void MarkLoadComplete( const MemReq& req ) override;

  /// RZALSCoProc: Set the memory handler
  void setMem( RevMem* M ) override { Mem = M; }

  /// RZALSCoProc: Set the ZOP NIC handler
  void setZNic( Forza::zopAPI* Z ) override { zNic = Z; }

private:
  RevMem*        Mem;    ///< RZALSCoProc: RevMem object
  Forza::zopAPI* zNic;   ///< RZALSCoProc: ZOPNic object
  RegAlloc       Alloc;  ///< RZALSCoProc: Register allocator object

  /// RZALSCoProc: Handle the incoming MZOP request
  bool handleMZOP( Forza::zopEvent* zev, bool& flag );

  /// RZALSCoProc: Register all the statistics
  void registerStats();

  /// RZALSCoProc: record the target statistic
  void recordStat( mzopStats Stat, uint64_t Data );

  std::vector<std::pair<Forza::zopEvent*, uint32_t>> LoadQ;  ///< RZALSCoProc: Outstanding load queue

  std::function<void( const MemReq& )> MarkLoadCompleteFunc;  ///< RZALSCoProc: Hazard function

  std::vector<Statistic<uint64_t>*> stats;  ///< RZALSCoProc: Statistics handlers

  /// RZALSCoProc: checks the load queues for completed operations and clears hazards
  void CheckLSQueue();

  enum class mzopKind { load, store, sdma };

  static const std::unordered_map<zopOpc, std::tuple<mzopKind, uint32_t, mzopStats, RevFlag>> mzopTable;
};  // RZALSCoProc

// ----------------------------------------
// RZAAMOCoProc
// ----------------------------------------
class RZAAMOCoProc final : public RevCoProc {
public:
  // Subcomponent info
  SST_ELI_REGISTER_SUBCOMPONENT(
    RZAAMOCoProc,
    "revcpu",
    "RZAAMOCoProc",
    SST_ELI_ELEMENT_VERSION( 1, 0, 0 ),
    "FORZA RZA Load/Store CoProc",
    SST::RevCPU::RevCoProc
  )
  // Register the paramaters
  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Sets the verbosity", "0" }, { "clock", "Sets the clock frequency", "1Ghz" }, )

  // Register subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  // Register any ports
  SST_ELI_DOCUMENT_PORTS()

#define HZOP_STAT( stat ) { #stat, #stat " Requests", "count", 1 }

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS(
    HZOP_STAT( HZOP_8_BASE_ADD ),
    HZOP_STAT( HZOP_8_BASE_AND ),
    HZOP_STAT( HZOP_8_BASE_OR ),
    HZOP_STAT( HZOP_8_BASE_XOR ),
    HZOP_STAT( HZOP_8_BASE_SMAX ),
    HZOP_STAT( HZOP_8_BASE_MAX ),
    HZOP_STAT( HZOP_8_BASE_SMIN ),
    HZOP_STAT( HZOP_8_BASE_MIN ),
    HZOP_STAT( HZOP_8_BASE_SWAP ),
    HZOP_STAT( HZOP_8_BASE_CAS ),
    HZOP_STAT( HZOP_8_BASE_THRESH ),
    HZOP_STAT( HZOP_8_BASE_FADD ),
    HZOP_STAT( HZOP_8_BASE_FSUB ),
    HZOP_STAT( HZOP_8_BASE_FRSUB ),

    HZOP_STAT( HZOP_16_BASE_ADD ),
    HZOP_STAT( HZOP_16_BASE_AND ),
    HZOP_STAT( HZOP_16_BASE_OR ),
    HZOP_STAT( HZOP_16_BASE_XOR ),
    HZOP_STAT( HZOP_16_BASE_SMAX ),
    HZOP_STAT( HZOP_16_BASE_MAX ),
    HZOP_STAT( HZOP_16_BASE_SMIN ),
    HZOP_STAT( HZOP_16_BASE_MIN ),
    HZOP_STAT( HZOP_16_BASE_SWAP ),
    HZOP_STAT( HZOP_16_BASE_CAS ),
    HZOP_STAT( HZOP_16_BASE_THRESH ),
    HZOP_STAT( HZOP_16_BASE_FADD ),
    HZOP_STAT( HZOP_16_BASE_FSUB ),
    HZOP_STAT( HZOP_16_BASE_FRSUB ),

    HZOP_STAT( HZOP_32_BASE_ADD ),
    HZOP_STAT( HZOP_32_BASE_AND ),
    HZOP_STAT( HZOP_32_BASE_OR ),
    HZOP_STAT( HZOP_32_BASE_XOR ),
    HZOP_STAT( HZOP_32_BASE_SMAX ),
    HZOP_STAT( HZOP_32_BASE_MAX ),
    HZOP_STAT( HZOP_32_BASE_SMIN ),
    HZOP_STAT( HZOP_32_BASE_MIN ),
    HZOP_STAT( HZOP_32_BASE_SWAP ),
    HZOP_STAT( HZOP_32_BASE_CAS ),
    HZOP_STAT( HZOP_32_BASE_THRESH ),
    HZOP_STAT( HZOP_32_BASE_FADD ),
    HZOP_STAT( HZOP_32_BASE_FSUB ),
    HZOP_STAT( HZOP_32_BASE_FRSUB ),

    HZOP_STAT( HZOP_64_BASE_ADD ),
    HZOP_STAT( HZOP_64_BASE_AND ),
    HZOP_STAT( HZOP_64_BASE_OR ),
    HZOP_STAT( HZOP_64_BASE_XOR ),
    HZOP_STAT( HZOP_64_BASE_SMAX ),
    HZOP_STAT( HZOP_64_BASE_MAX ),
    HZOP_STAT( HZOP_64_BASE_SMIN ),
    HZOP_STAT( HZOP_64_BASE_MIN ),
    HZOP_STAT( HZOP_64_BASE_SWAP ),
    HZOP_STAT( HZOP_64_BASE_CAS ),
    HZOP_STAT( HZOP_64_BASE_THRESH ),
    HZOP_STAT( HZOP_64_BASE_FADD ),
    HZOP_STAT( HZOP_64_BASE_FSUB ),
    HZOP_STAT( HZOP_64_BASE_FRSUB ),

    HZOP_STAT( HZOP_8_M_ADD ),
    HZOP_STAT( HZOP_8_M_AND ),
    HZOP_STAT( HZOP_8_M_OR ),
    HZOP_STAT( HZOP_8_M_XOR ),
    HZOP_STAT( HZOP_8_M_SMAX ),
    HZOP_STAT( HZOP_8_M_MAX ),
    HZOP_STAT( HZOP_8_M_SMIN ),
    HZOP_STAT( HZOP_8_M_MIN ),
    HZOP_STAT( HZOP_8_M_SWAP ),
    HZOP_STAT( HZOP_8_M_CAS ),
    HZOP_STAT( HZOP_8_M_THRESH ),
    HZOP_STAT( HZOP_8_M_FADD ),
    HZOP_STAT( HZOP_8_M_FSUB ),
    HZOP_STAT( HZOP_8_M_FRSUB ),

    HZOP_STAT( HZOP_16_M_ADD ),
    HZOP_STAT( HZOP_16_M_AND ),
    HZOP_STAT( HZOP_16_M_OR ),
    HZOP_STAT( HZOP_16_M_XOR ),
    HZOP_STAT( HZOP_16_M_SMAX ),
    HZOP_STAT( HZOP_16_M_MAX ),
    HZOP_STAT( HZOP_16_M_SMIN ),
    HZOP_STAT( HZOP_16_M_MIN ),
    HZOP_STAT( HZOP_16_M_SWAP ),
    HZOP_STAT( HZOP_16_M_CAS ),
    HZOP_STAT( HZOP_16_M_THRESH ),
    HZOP_STAT( HZOP_16_M_FADD ),
    HZOP_STAT( HZOP_16_M_FSUB ),
    HZOP_STAT( HZOP_16_M_FRSUB ),

    HZOP_STAT( HZOP_32_M_ADD ),
    HZOP_STAT( HZOP_32_M_AND ),
    HZOP_STAT( HZOP_32_M_OR ),
    HZOP_STAT( HZOP_32_M_XOR ),
    HZOP_STAT( HZOP_32_M_SMAX ),
    HZOP_STAT( HZOP_32_M_MAX ),
    HZOP_STAT( HZOP_32_M_SMIN ),
    HZOP_STAT( HZOP_32_M_MIN ),
    HZOP_STAT( HZOP_32_M_SWAP ),
    HZOP_STAT( HZOP_32_M_CAS ),
    HZOP_STAT( HZOP_32_M_THRESH ),
    HZOP_STAT( HZOP_32_M_FADD ),
    HZOP_STAT( HZOP_32_M_FSUB ),
    HZOP_STAT( HZOP_32_M_FRSUB ),

    HZOP_STAT( HZOP_64_M_ADD ),
    HZOP_STAT( HZOP_64_M_AND ),
    HZOP_STAT( HZOP_64_M_OR ),
    HZOP_STAT( HZOP_64_M_XOR ),
    HZOP_STAT( HZOP_64_M_SMAX ),
    HZOP_STAT( HZOP_64_M_MAX ),
    HZOP_STAT( HZOP_64_M_SMIN ),
    HZOP_STAT( HZOP_64_M_MIN ),
    HZOP_STAT( HZOP_64_M_SWAP ),
    HZOP_STAT( HZOP_64_M_CAS ),
    HZOP_STAT( HZOP_64_M_THRESH ),
    HZOP_STAT( HZOP_64_M_FADD ),
    HZOP_STAT( HZOP_64_M_FSUB ),
    HZOP_STAT( HZOP_64_M_FRSUB ),

    HZOP_STAT( HZOP_8_S_ADD ),
    HZOP_STAT( HZOP_8_S_AND ),
    HZOP_STAT( HZOP_8_S_OR ),
    HZOP_STAT( HZOP_8_S_XOR ),
    HZOP_STAT( HZOP_8_S_SMAX ),
    HZOP_STAT( HZOP_8_S_MAX ),
    HZOP_STAT( HZOP_8_S_SMIN ),
    HZOP_STAT( HZOP_8_S_MIN ),
    HZOP_STAT( HZOP_8_S_SWAP ),
    HZOP_STAT( HZOP_8_S_CAS ),
    HZOP_STAT( HZOP_8_S_THRESH ),
    HZOP_STAT( HZOP_8_S_FADD ),
    HZOP_STAT( HZOP_8_S_FSUB ),
    HZOP_STAT( HZOP_8_S_FRSUB ),

    HZOP_STAT( HZOP_16_S_ADD ),
    HZOP_STAT( HZOP_16_S_AND ),
    HZOP_STAT( HZOP_16_S_OR ),
    HZOP_STAT( HZOP_16_S_XOR ),
    HZOP_STAT( HZOP_16_S_SMAX ),
    HZOP_STAT( HZOP_16_S_MAX ),
    HZOP_STAT( HZOP_16_S_SMIN ),
    HZOP_STAT( HZOP_16_S_MIN ),
    HZOP_STAT( HZOP_16_S_SWAP ),
    HZOP_STAT( HZOP_16_S_CAS ),
    HZOP_STAT( HZOP_16_S_THRESH ),
    HZOP_STAT( HZOP_16_S_FADD ),
    HZOP_STAT( HZOP_16_S_FSUB ),
    HZOP_STAT( HZOP_16_S_FRSUB ),

    HZOP_STAT( HZOP_32_S_ADD ),
    HZOP_STAT( HZOP_32_S_AND ),
    HZOP_STAT( HZOP_32_S_OR ),
    HZOP_STAT( HZOP_32_S_XOR ),
    HZOP_STAT( HZOP_32_S_SMAX ),
    HZOP_STAT( HZOP_32_S_MAX ),
    HZOP_STAT( HZOP_32_S_SMIN ),
    HZOP_STAT( HZOP_32_S_MIN ),
    HZOP_STAT( HZOP_32_S_SWAP ),
    HZOP_STAT( HZOP_32_S_CAS ),
    HZOP_STAT( HZOP_32_S_THRESH ),
    HZOP_STAT( HZOP_32_S_FADD ),
    HZOP_STAT( HZOP_32_S_FSUB ),
    HZOP_STAT( HZOP_32_S_FRSUB ),

    HZOP_STAT( HZOP_64_S_ADD ),
    HZOP_STAT( HZOP_64_S_AND ),
    HZOP_STAT( HZOP_64_S_OR ),
    HZOP_STAT( HZOP_64_S_XOR ),
    HZOP_STAT( HZOP_64_S_SMAX ),
    HZOP_STAT( HZOP_64_S_MAX ),
    HZOP_STAT( HZOP_64_S_SMIN ),
    HZOP_STAT( HZOP_64_S_MIN ),
    HZOP_STAT( HZOP_64_S_SWAP ),
    HZOP_STAT( HZOP_64_S_CAS ),
    HZOP_STAT( HZOP_64_S_THRESH ),
    HZOP_STAT( HZOP_64_S_FADD ),
    HZOP_STAT( HZOP_64_S_FSUB ),
    HZOP_STAT( HZOP_64_S_FRSUB ),

    HZOP_STAT( HZOP_8_MS_ADD ),
    HZOP_STAT( HZOP_8_MS_AND ),
    HZOP_STAT( HZOP_8_MS_OR ),
    HZOP_STAT( HZOP_8_MS_XOR ),
    HZOP_STAT( HZOP_8_MS_SMAX ),
    HZOP_STAT( HZOP_8_MS_MAX ),
    HZOP_STAT( HZOP_8_MS_SMIN ),
    HZOP_STAT( HZOP_8_MS_MIN ),
    HZOP_STAT( HZOP_8_MS_SWAP ),
    HZOP_STAT( HZOP_8_MS_CAS ),
    HZOP_STAT( HZOP_8_MS_THRESH ),
    HZOP_STAT( HZOP_8_MS_FADD ),
    HZOP_STAT( HZOP_8_MS_FSUB ),
    HZOP_STAT( HZOP_8_MS_FRSUB ),

    HZOP_STAT( HZOP_16_MS_ADD ),
    HZOP_STAT( HZOP_16_MS_AND ),
    HZOP_STAT( HZOP_16_MS_OR ),
    HZOP_STAT( HZOP_16_MS_XOR ),
    HZOP_STAT( HZOP_16_MS_SMAX ),
    HZOP_STAT( HZOP_16_MS_MAX ),
    HZOP_STAT( HZOP_16_MS_SMIN ),
    HZOP_STAT( HZOP_16_MS_MIN ),
    HZOP_STAT( HZOP_16_MS_SWAP ),
    HZOP_STAT( HZOP_16_MS_CAS ),
    HZOP_STAT( HZOP_16_MS_THRESH ),
    HZOP_STAT( HZOP_16_MS_FADD ),
    HZOP_STAT( HZOP_16_MS_FSUB ),
    HZOP_STAT( HZOP_16_MS_FRSUB ),

    HZOP_STAT( HZOP_32_MS_ADD ),
    HZOP_STAT( HZOP_32_MS_AND ),
    HZOP_STAT( HZOP_32_MS_OR ),
    HZOP_STAT( HZOP_32_MS_XOR ),
    HZOP_STAT( HZOP_32_MS_SMAX ),
    HZOP_STAT( HZOP_32_MS_MAX ),
    HZOP_STAT( HZOP_32_MS_SMIN ),
    HZOP_STAT( HZOP_32_MS_MIN ),
    HZOP_STAT( HZOP_32_MS_SWAP ),
    HZOP_STAT( HZOP_32_MS_CAS ),
    HZOP_STAT( HZOP_32_MS_THRESH ),
    HZOP_STAT( HZOP_32_MS_FADD ),
    HZOP_STAT( HZOP_32_MS_FSUB ),
    HZOP_STAT( HZOP_32_MS_FRSUB ),

    HZOP_STAT( HZOP_64_MS_ADD ),
    HZOP_STAT( HZOP_64_MS_AND ),
    HZOP_STAT( HZOP_64_MS_OR ),
    HZOP_STAT( HZOP_64_MS_XOR ),
    HZOP_STAT( HZOP_64_MS_SMAX ),
    HZOP_STAT( HZOP_64_MS_MAX ),
    HZOP_STAT( HZOP_64_MS_SMIN ),
    HZOP_STAT( HZOP_64_MS_MIN ),
    HZOP_STAT( HZOP_64_MS_SWAP ),
    HZOP_STAT( HZOP_64_MS_CAS ),
    HZOP_STAT( HZOP_64_MS_THRESH ),
    HZOP_STAT( HZOP_64_MS_FADD ),
    HZOP_STAT( HZOP_64_MS_FSUB ),
    HZOP_STAT( HZOP_64_MS_FRSUB )
  )

  enum class hzopStats : uint32_t {
    HZOP_8_BASE_ADD,
    HZOP_8_BASE_AND,
    HZOP_8_BASE_OR,
    HZOP_8_BASE_XOR,
    HZOP_8_BASE_SMAX,
    HZOP_8_BASE_MAX,
    HZOP_8_BASE_SMIN,
    HZOP_8_BASE_MIN,
    HZOP_8_BASE_SWAP,
    HZOP_8_BASE_CAS,
    HZOP_8_BASE_THRESH,
    HZOP_8_BASE_FADD,
    HZOP_8_BASE_FSUB,
    HZOP_8_BASE_FRSUB,

    HZOP_16_BASE_ADD,
    HZOP_16_BASE_AND,
    HZOP_16_BASE_OR,
    HZOP_16_BASE_XOR,
    HZOP_16_BASE_SMAX,
    HZOP_16_BASE_MAX,
    HZOP_16_BASE_SMIN,
    HZOP_16_BASE_MIN,
    HZOP_16_BASE_SWAP,
    HZOP_16_BASE_CAS,
    HZOP_16_BASE_THRESH,
    HZOP_16_BASE_FADD,
    HZOP_16_BASE_FSUB,
    HZOP_16_BASE_FRSUB,

    HZOP_32_BASE_ADD,
    HZOP_32_BASE_AND,
    HZOP_32_BASE_OR,
    HZOP_32_BASE_XOR,
    HZOP_32_BASE_SMAX,
    HZOP_32_BASE_MAX,
    HZOP_32_BASE_SMIN,
    HZOP_32_BASE_MIN,
    HZOP_32_BASE_SWAP,
    HZOP_32_BASE_CAS,
    HZOP_32_BASE_THRESH,
    HZOP_32_BASE_FADD,
    HZOP_32_BASE_FSUB,
    HZOP_32_BASE_FRSUB,

    HZOP_64_BASE_ADD,
    HZOP_64_BASE_AND,
    HZOP_64_BASE_OR,
    HZOP_64_BASE_XOR,
    HZOP_64_BASE_SMAX,
    HZOP_64_BASE_MAX,
    HZOP_64_BASE_SMIN,
    HZOP_64_BASE_MIN,
    HZOP_64_BASE_SWAP,
    HZOP_64_BASE_CAS,
    HZOP_64_BASE_THRESH,
    HZOP_64_BASE_FADD,
    HZOP_64_BASE_FSUB,
    HZOP_64_BASE_FRSUB,

    HZOP_8_M_ADD,
    HZOP_8_M_AND,
    HZOP_8_M_OR,
    HZOP_8_M_XOR,
    HZOP_8_M_SMAX,
    HZOP_8_M_MAX,
    HZOP_8_M_SMIN,
    HZOP_8_M_MIN,
    HZOP_8_M_SWAP,
    HZOP_8_M_CAS,
    HZOP_8_M_THRESH,
    HZOP_8_M_FADD,
    HZOP_8_M_FSUB,
    HZOP_8_M_FRSUB,

    HZOP_16_M_ADD,
    HZOP_16_M_AND,
    HZOP_16_M_OR,
    HZOP_16_M_XOR,
    HZOP_16_M_SMAX,
    HZOP_16_M_MAX,
    HZOP_16_M_SMIN,
    HZOP_16_M_MIN,
    HZOP_16_M_SWAP,
    HZOP_16_M_CAS,
    HZOP_16_M_THRESH,
    HZOP_16_M_FADD,
    HZOP_16_M_FSUB,
    HZOP_16_M_FRSUB,

    HZOP_32_M_ADD,
    HZOP_32_M_AND,
    HZOP_32_M_OR,
    HZOP_32_M_XOR,
    HZOP_32_M_SMAX,
    HZOP_32_M_MAX,
    HZOP_32_M_SMIN,
    HZOP_32_M_MIN,
    HZOP_32_M_SWAP,
    HZOP_32_M_CAS,
    HZOP_32_M_THRESH,
    HZOP_32_M_FADD,
    HZOP_32_M_FSUB,
    HZOP_32_M_FRSUB,

    HZOP_64_M_ADD,
    HZOP_64_M_AND,
    HZOP_64_M_OR,
    HZOP_64_M_XOR,
    HZOP_64_M_SMAX,
    HZOP_64_M_MAX,
    HZOP_64_M_SMIN,
    HZOP_64_M_MIN,
    HZOP_64_M_SWAP,
    HZOP_64_M_CAS,
    HZOP_64_M_THRESH,
    HZOP_64_M_FADD,
    HZOP_64_M_FSUB,
    HZOP_64_M_FRSUB,

    HZOP_8_S_ADD,
    HZOP_8_S_AND,
    HZOP_8_S_OR,
    HZOP_8_S_XOR,
    HZOP_8_S_SMAX,
    HZOP_8_S_MAX,
    HZOP_8_S_SMIN,
    HZOP_8_S_MIN,
    HZOP_8_S_SWAP,
    HZOP_8_S_CAS,
    HZOP_8_S_THRESH,
    HZOP_8_S_FADD,
    HZOP_8_S_FSUB,
    HZOP_8_S_FRSUB,

    HZOP_16_S_ADD,
    HZOP_16_S_AND,
    HZOP_16_S_OR,
    HZOP_16_S_XOR,
    HZOP_16_S_SMAX,
    HZOP_16_S_MAX,
    HZOP_16_S_SMIN,
    HZOP_16_S_MIN,
    HZOP_16_S_SWAP,
    HZOP_16_S_CAS,
    HZOP_16_S_THRESH,
    HZOP_16_S_FADD,
    HZOP_16_S_FSUB,
    HZOP_16_S_FRSUB,

    HZOP_32_S_ADD,
    HZOP_32_S_AND,
    HZOP_32_S_OR,
    HZOP_32_S_XOR,
    HZOP_32_S_SMAX,
    HZOP_32_S_MAX,
    HZOP_32_S_SMIN,
    HZOP_32_S_MIN,
    HZOP_32_S_SWAP,
    HZOP_32_S_CAS,
    HZOP_32_S_THRESH,
    HZOP_32_S_FADD,
    HZOP_32_S_FSUB,
    HZOP_32_S_FRSUB,

    HZOP_64_S_ADD,
    HZOP_64_S_AND,
    HZOP_64_S_OR,
    HZOP_64_S_XOR,
    HZOP_64_S_SMAX,
    HZOP_64_S_MAX,
    HZOP_64_S_SMIN,
    HZOP_64_S_MIN,
    HZOP_64_S_SWAP,
    HZOP_64_S_CAS,
    HZOP_64_S_THRESH,
    HZOP_64_S_FADD,
    HZOP_64_S_FSUB,
    HZOP_64_S_FRSUB,

    HZOP_8_MS_ADD,
    HZOP_8_MS_AND,
    HZOP_8_MS_OR,
    HZOP_8_MS_XOR,
    HZOP_8_MS_SMAX,
    HZOP_8_MS_MAX,
    HZOP_8_MS_SMIN,
    HZOP_8_MS_MIN,
    HZOP_8_MS_SWAP,
    HZOP_8_MS_CAS,
    HZOP_8_MS_THRESH,
    HZOP_8_MS_FADD,
    HZOP_8_MS_FSUB,
    HZOP_8_MS_FRSUB,

    HZOP_16_MS_ADD,
    HZOP_16_MS_AND,
    HZOP_16_MS_OR,
    HZOP_16_MS_XOR,
    HZOP_16_MS_SMAX,
    HZOP_16_MS_MAX,
    HZOP_16_MS_SMIN,
    HZOP_16_MS_MIN,
    HZOP_16_MS_SWAP,
    HZOP_16_MS_CAS,
    HZOP_16_MS_THRESH,
    HZOP_16_MS_FADD,
    HZOP_16_MS_FSUB,
    HZOP_16_MS_FRSUB,

    HZOP_32_MS_ADD,
    HZOP_32_MS_AND,
    HZOP_32_MS_OR,
    HZOP_32_MS_XOR,
    HZOP_32_MS_SMAX,
    HZOP_32_MS_MAX,
    HZOP_32_MS_SMIN,
    HZOP_32_MS_MIN,
    HZOP_32_MS_SWAP,
    HZOP_32_MS_CAS,
    HZOP_32_MS_THRESH,
    HZOP_32_MS_FADD,
    HZOP_32_MS_FSUB,
    HZOP_32_MS_FRSUB,

    HZOP_64_MS_ADD,
    HZOP_64_MS_AND,
    HZOP_64_MS_OR,
    HZOP_64_MS_XOR,
    HZOP_64_MS_SMAX,
    HZOP_64_MS_MAX,
    HZOP_64_MS_SMIN,
    HZOP_64_MS_MIN,
    HZOP_64_MS_SWAP,
    HZOP_64_MS_CAS,
    HZOP_64_MS_THRESH,
    HZOP_64_MS_FADD,
    HZOP_64_MS_FSUB,
    HZOP_64_MS_FRSUB,

    HZOP_END
  };

  /// RZAAMOCoProc: default constructor
  RZAAMOCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RZAAMOCoProc: default destructor
  virtual ~RZAAMOCoProc();

  /// RZAAMOCoProc: clock tick function
  bool ClockTick( SST::Cycle_t cycle ) override;

  /// RZAAMOCoProc: Enqueue a new instruction
  bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) override;

  /// RZAAMOCoProc: reset the coproc
  bool Reset() final;

  /// RZAAMOCoProc: teardown function when the attached Proc is complete
  bool Teardown() override { return Reset(); }

  /// RZAAMOCoProc: determines whether the coproc is complete
  bool IsDone() override;

  /// RZAMOCoProc: injects a packet into the HZOP AMO pipeline
  bool InjectZOP( Forza::zopEvent* zev, bool& flag ) override;

  /// RZAAMOCoProc: marks a load as being complete
  void MarkLoadComplete( const MemReq& req ) override;

  /// RZAAMOCoProc: Set the memory handler
  void setMem( RevMem* M ) override { Mem = M; }

  /// RZAAMOCoProc: Set the ZOP NIC handler
  void setZNic( Forza::zopAPI* Z ) override { zNic = Z; }

private:
  RevMem*        Mem;    ///< RZAAMOCoProc: RevMem object
  Forza::zopAPI* zNic;   ///< RZAAMOCoProc: ZOPNic object
  RegAlloc       Alloc;  ///< RZAAMOCoProc: Register allocator object

  /// RZAAMOCoProc: Handle the incoming HZOP request
  bool handleHZOP( Forza::zopEvent* zev, bool& flag );

  /// RZAAMOCoProc: Register all the statistics
  void registerStats();

  /// RZAAMOCoProc: record the target statistic
  void recordStat( hzopStats Stat, uint64_t Data );

  std::vector<std::tuple<Forza::zopEvent*, uint32_t, uint32_t>> AMOQ;  ///< RZAAMOCoProc: Outstanding load queue

  std::function<void( const MemReq& )> MarkLoadCompleteFunc;  ///< RZAAMOCoProc: Hazard function

  std::vector<Statistic<uint64_t>*> stats;  ///< RZAAMOCoProc: Statistics handlers

  /// RZAAMOCoProc: checks the load queues for completed operations and clears hazards
  void CheckLSQueue();

  static const std::unordered_map<zopOpc, std::tuple<uint32_t, hzopStats, RevFlag, RevFlag>> zopAMOTable;
};  // RZAAMOCoProc

using hzopStats = RZAAMOCoProc::hzopStats;
using mzopStats = RZALSCoProc::mzopStats;

}  //namespace SST::RevCPU

#endif
