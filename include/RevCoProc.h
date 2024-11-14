//
// _RevCoProc_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVCOPROC_H_
#define _SST_REVCPU_REVCOPROC_H_

// -- C++ Headers
#include <algorithm>
#include <ctime>
#include <list>
#include <mutex>

#include <queue>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <tuple>
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
class RevCore;

#define _RA_NUM_REG 4096
#define _UNDEF_REG  ( ( ( _RA_NUM_REG ) ) + 1 )

// ----------------------------------------
// RegAlloc
// ----------------------------------------
#define _H_CLEAR    0
#define _H_SET      1
#define _H_DIRTY    2

class RegAlloc {
public:
  /// RegAlloc: constructor
  RegAlloc() {
    for( unsigned i = 0; i < _RA_NUM_REG; i++ ) {
      hazard[i] = _H_CLEAR;
      regs[i]   = 0x00ull;
    }
  }

  /// RegAlloc: destructor
  ~RegAlloc() {}

  /// RegAlloc: retrieve a single operand register
  bool getRegs( unsigned& rs1 ) {
    unsigned t_rs1 = _RA_NUM_REG + 1;

    unsigned cur   = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rs1       = cur;
        break;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    rs1 = t_rs1;
    return true;
  }

  /// RegAlloc: retrieve a two operand register set
  bool getRegs( unsigned& rs1, unsigned& rs2 ) {
    unsigned t_rs1 = _RA_NUM_REG + 1;
    unsigned t_rs2 = _RA_NUM_REG + 1;

    // find two register slots
    unsigned cur   = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rs1       = cur;
        break;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    cur = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rs2       = cur;
        break;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    rs1 = t_rs1;
    rs2 = t_rs2;
    return true;
  }

  /// RegAlloc: retrieve a three operand register set
  bool getRegs( unsigned& rd, unsigned& rs1, unsigned& rs2 ) {
    unsigned t_rd  = _RA_NUM_REG + 1;
    unsigned t_rs1 = _RA_NUM_REG + 1;
    unsigned t_rs2 = _RA_NUM_REG + 1;

    // find three register slots
    // -- Rd
    unsigned cur   = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rd        = cur;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    // -- Rs1
    cur = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rs1       = cur;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    // -- Rs2
    cur = 0;
    while( cur < _RA_NUM_REG ) {
      if( hazard[cur] == _H_CLEAR ) {
        hazard[cur] = _H_SET;
        t_rs2       = cur;
      } else {
        cur++;
        if( cur == _RA_NUM_REG ) {
          return false;
        }
      }
    }

    rd  = t_rd;
    rs1 = t_rs1;
    rs2 = t_rs2;
    return true;
  }

  /// RegAlloc: clear the hazard on the target register
  void clearReg( unsigned reg ) {
    if( reg < _RA_NUM_REG ) {
      hazard[reg] = _H_CLEAR;
      regs[reg]   = 0x00ull;
    }
  }

  /// RegAlloc: set the target value
  void SetX( unsigned Idx, uint64_t Val ) {
    if( Idx < _RA_NUM_REG - 1 ) {
      regs[Idx] = Val;
    }
  }

  /// RegAlloc: get the target value
  uint64_t GetX( unsigned Idx ) {
    if( Idx < _RA_NUM_REG - 1 ) {
      return regs[Idx];
    }
    return 0x00ull;
  }

  /// RegAlloc: get the address for the target reigster
  uint64_t* getRegAddr( unsigned Idx ) {
    if( Idx < _RA_NUM_REG - 1 ) {
      return ( &regs[Idx] );
    }
    return nullptr;
  }

  /// RegAlloc: get the hazard state
  uint8_t getState( unsigned Idx ) {
    if( Idx < _RA_NUM_REG - 1 ) {
      return hazard[Idx];
    }
    return _H_DIRTY;
  }

  /// RegAlloc: set the target register as dirty
  void setDirty( unsigned Idx ) {
    if( Idx < _RA_NUM_REG - 1 ) {
      hazard[Idx] = _H_DIRTY;
    }
  }

  uint64_t regs[_RA_NUM_REG];  ///< RegAlloc: register array

private:
  uint8_t hazard[_RA_NUM_REG];
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
  virtual const std::vector<uint8_t> getRawData() {
    output->fatal( CALL_INFO, -1, "Error : no override method defined for getRawData()\n" );

    // inserting code to quiesce warnings
    std::vector<uint8_t> D;
    return D;
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
class RevSimpleCoProc : public RevCoProc {
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
  enum CoProcStats {
    InstRetired = 0,
  };

  /// RevSimpleCoProc: constructor
  RevSimpleCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RevSimpleCoProc: destructor
  virtual ~RevSimpleCoProc()                           = default;

  /// RevSimpleCoProc: disallow copying and assignment
  RevSimpleCoProc( const RevSimpleCoProc& )            = delete;
  RevSimpleCoProc& operator=( const RevSimpleCoProc& ) = delete;

  /// RevSimpleCoProc: clock tick function - currently not registeres with SST, called by RevCPU
  virtual bool ClockTick( SST::Cycle_t cycle );

  void registerStats();

  /// RevSimpleCoProc: Enqueue Inst into the InstQ and return
  virtual bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst );

  /// RevSimpleCoProc: Reset the co-processor by emmptying the InstQ
  virtual bool Reset();

  /// RevSimpleCoProv: Called when the attached RevCore completes simulation. Could be used to
  ///                   also signal to SST that the co-processor is done if ClockTick is registered
  ///                   to SSTCore vs. being driven by RevCPU
  virtual bool Teardown() { return Reset(); };

  /// RevSimpleCoProc: Returns true if instruction queue is empty
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

  /// RevSimpleCoProc: Total number of instructions retired
  Statistic<uint64_t>* num_instRetired{};

  /// Queue of instructions sent from attached RevCore
  std::queue<RevCoProcInst> InstQ{};

  SST::Cycle_t cycleCount{};

};  //class RevSimpleCoProc

// ----------------------------------------
// RZALSCoProc
// ----------------------------------------
class RZALSCoProc : public RevCoProc {
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

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS(
    { "MZOP_LB", "MZOP LB Requests", "count", 1 },
    { "MZOP_LH", "MZOP LH Requests", "count", 1 },
    { "MZOP_LW", "MZOP LW Requests", "count", 1 },
    { "MZOP_LD", "MZOP LD Requests", "count", 1 },
    { "MZOP_LSB", "MZOP LSB Requests", "count", 1 },
    { "MZOP_LSH", "MZOP LSH Requests", "count", 1 },
    { "MZOP_LSW", "MZOP LSW Requests", "count", 1 },
    { "MZOP_LDMA", "MZOP LDMA Requests", "count", 1 },
    { "MZOP_SB", "MZOP SB Requests", "count", 1 },
    { "MZOP_SH", "MZOP SH Requests", "count", 1 },
    { "MZOP_SW", "MZOP SW Requests", "count", 1 },
    { "MZOP_SD", "MZOP SD Requests", "count", 1 },
    { "MZOP_SSB", "MZOP SSB Requests", "count", 1 },
    { "MZOP_SSH", "MZOP SSH Requests", "count", 1 },
    { "MZOP_SSW", "MZOP SSW Requests", "count", 1 },
    { "MZOP_SDMA", "MZOP SDMA Requests", "count", 1 },
  )

  enum mzopStats : uint32_t {
    MZOP_LB   = 0,
    MZOP_LH   = 1,
    MZOP_LW   = 2,
    MZOP_LD   = 3,
    MZOP_LSB  = 4,
    MZOP_LSH  = 5,
    MZOP_LSW  = 6,
    MZOP_LDMA = 7,
    MZOP_SB   = 8,
    MZOP_SH   = 9,
    MZOP_SW   = 10,
    MZOP_SD   = 11,
    MZOP_SSB  = 12,
    MZOP_SSH  = 13,
    MZOP_SSW  = 14,
    MZOP_SDMA = 15,
    MZOP_END  = 16,
  };

  /// RZALSCoProc: default constructor
  RZALSCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RZALSCoProc: default destructor
  virtual ~RZALSCoProc();

  /// RZALSCoProc: clock tick function
  virtual bool ClockTick( SST::Cycle_t cycle ) override;

  /// RZALSCoProc: Enqueue a new instruction
  virtual bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) override;

  /// RZALSCoProc: reset the coproc
  virtual bool Reset() override;

  /// RZALSCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() override { return Reset(); }

  /// RZALSCoProc: determines whether the coproc is complete
  virtual bool IsDone() override;

  /// RZALSCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP( Forza::zopEvent* zev, bool& flag ) override;

  /// RZALSCoProc: marks a load as being complete
  virtual void MarkLoadComplete( const MemReq& req ) override;

  /// RZALSCoProc: Set the memory handler
  virtual void setMem( RevMem* M ) override { Mem = M; }

  /// RZALSCoProc: Set the ZOP NIC handler
  virtual void setZNic( Forza::zopAPI* Z ) override { zNic = Z; }

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

#define LOADQ_ZEV 0
#define LOADQ_RS2 1
  std::vector<std::pair<Forza::zopEvent*, unsigned>> LoadQ;  ///< RZALSCoProc: Outstanding load queue

  std::function<void( const MemReq& )> MarkLoadCompleteFunc;  ///< RZALSCoProc: Hazard function

  std::vector<Statistic<uint64_t>*> stats;  ///< RZALSCoProc: Statistics handlers

  /// RZALSCoProc: checks the load queues for completed operations and clears hazards
  void CheckLSQueue();

};  // RZALSCoProc

// ----------------------------------------
// RZAAMOCoProc
// ----------------------------------------
class RZAAMOCoProc : public RevCoProc {
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

  // Register statistics
  SST_ELI_DOCUMENT_STATISTICS(
    { "HZOP_8_BASE_ADD", "HZOP_8_BASE_ADD Requests", "count", 1 },
    { "HZOP_8_BASE_AND", "HZOP_8_BASE_AND Requests", "count", 1 },
    { "HZOP_8_BASE_OR", "HZOP_8_BASE_OR Requests", "count", 1 },
    { "HZOP_8_BASE_XOR", "HZOP_8_BASE_XOR Requests", "count", 1 },
    { "HZOP_8_BASE_SMAX", "HZOP_8_BASE_SMAX Requests", "count", 1 },
    { "HZOP_8_BASE_MAX", "HZOP_8_BASE_MAX Requests", "count", 1 },
    { "HZOP_8_BASE_SMIN", "HZOP_8_BASE_SMIN Requests", "count", 1 },
    { "HZOP_8_BASE_MIN", "HZOP_8_BASE_MIN Requests", "count", 1 },
    { "HZOP_8_BASE_SWAP", "HZOP_8_BASE_SWAP Requests", "count", 1 },
    { "HZOP_8_BASE_CAS", "HZOP_8_BASE_CAS Requests", "count", 1 },
    { "HZOP_8_BASE_FADD", "HZOP_8_BASE_FADD Requests", "count", 1 },
    { "HZOP_8_BASE_FSUB", "HZOP_8_BASE_FSUB Requests", "count", 1 },
    { "HZOP_8_BASE_FRSUB", "HZOP_8_BASE_FRSUB Requests", "count", 1 },
    { "HZOP_16_BASE_ADD", "HZOP_16_BASE_ADD Requests", "count", 1 },
    { "HZOP_16_BASE_AND", "HZOP_16_BASE_AND Requests", "count", 1 },
    { "HZOP_16_BASE_OR", "HZOP_16_BASE_OR Requests", "count", 1 },
    { "HZOP_16_BASE_XOR", "HZOP_16_BASE_XOR Requests", "count", 1 },
    { "HZOP_16_BASE_SMAX", "HZOP_16_BASE_SMAX Requests", "count", 1 },
    { "HZOP_16_BASE_MAX", "HZOP_16_BASE_MAX Requests", "count", 1 },
    { "HZOP_16_BASE_SMIN", "HZOP_16_BASE_SMIN Requests", "count", 1 },
    { "HZOP_16_BASE_MIN", "HZOP_16_BASE_MIN Requests", "count", 1 },
    { "HZOP_16_BASE_SWAP", "HZOP_16_BASE_SWAP Requests", "count", 1 },
    { "HZOP_16_BASE_CAS", "HZOP_16_BASE_CAS Requests", "count", 1 },
    { "HZOP_16_BASE_FADD", "HZOP_16_BASE_FADD Requests", "count", 1 },
    { "HZOP_16_BASE_FSUB", "HZOP_16_BASE_FSUB Requests", "count", 1 },
    { "HZOP_16_BASE_FRSUB", "HZOP_16_BASE_FRSUB Requests", "count", 1 },
    { "HZOP_32_BASE_ADD", "HZOP_32_BASE_ADD Requests", "count", 1 },
    { "HZOP_32_BASE_AND", "HZOP_32_BASE_AND Requests", "count", 1 },
    { "HZOP_32_BASE_OR", "HZOP_32_BASE_OR Requests", "count", 1 },
    { "HZOP_32_BASE_XOR", "HZOP_32_BASE_XOR Requests", "count", 1 },
    { "HZOP_32_BASE_SMAX", "HZOP_32_BASE_SMAX Requests", "count", 1 },
    { "HZOP_32_BASE_MAX", "HZOP_32_BASE_MAX Requests", "count", 1 },
    { "HZOP_32_BASE_SMIN", "HZOP_32_BASE_SMIN Requests", "count", 1 },
    { "HZOP_32_BASE_MIN", "HZOP_32_BASE_MIN Requests", "count", 1 },
    { "HZOP_32_BASE_SWAP", "HZOP_32_BASE_SWAP Requests", "count", 1 },
    { "HZOP_32_BASE_CAS", "HZOP_32_BASE_CAS Requests", "count", 1 },
    { "HZOP_32_BASE_FADD", "HZOP_32_BASE_FADD Requests", "count", 1 },
    { "HZOP_32_BASE_FSUB", "HZOP_32_BASE_FSUB Requests", "count", 1 },
    { "HZOP_32_BASE_FRSUB", "HZOP_32_BASE_FRSUB Requests", "count", 1 },
    { "HZOP_64_BASE_ADD", "HZOP_64_BASE_ADD Requests", "count", 1 },
    { "HZOP_64_BASE_AND", "HZOP_64_BASE_AND Requests", "count", 1 },
    { "HZOP_64_BASE_OR", "HZOP_64_BASE_OR Requests", "count", 1 },
    { "HZOP_64_BASE_XOR", "HZOP_64_BASE_XOR Requests", "count", 1 },
    { "HZOP_64_BASE_SMAX", "HZOP_64_BASE_SMAX Requests", "count", 1 },
    { "HZOP_64_BASE_MAX", "HZOP_64_BASE_MAX Requests", "count", 1 },
    { "HZOP_64_BASE_SMIN", "HZOP_64_BASE_SMIN Requests", "count", 1 },
    { "HZOP_64_BASE_MIN", "HZOP_64_BASE_MIN Requests", "count", 1 },
    { "HZOP_64_BASE_SWAP", "HZOP_64_BASE_SWAP Requests", "count", 1 },
    { "HZOP_64_BASE_CAS", "HZOP_64_BASE_CAS Requests", "count", 1 },
    { "HZOP_64_BASE_FADD", "HZOP_64_BASE_FADD Requests", "count", 1 },
    { "HZOP_64_BASE_FSUB", "HZOP_64_BASE_FSUB Requests", "count", 1 },
    { "HZOP_64_BASE_FRSUB", "HZOP_64_BASE_FRSUB Requests", "count", 1 },
    { "HZOP_8_M_ADD", "HZOP_8_M_ADD Requests", "count", 1 },
    { "HZOP_8_M_AND", "HZOP_8_M_AND Requests", "count", 1 },
    { "HZOP_8_M_OR", "HZOP_8_M_OR Requests", "count", 1 },
    { "HZOP_8_M_XOR", "HZOP_8_M_XOR Requests", "count", 1 },
    { "HZOP_8_M_SMAX", "HZOP_8_M_SMAX Requests", "count", 1 },
    { "HZOP_8_M_MAX", "HZOP_8_M_MAX Requests", "count", 1 },
    { "HZOP_8_M_SMIN", "HZOP_8_M_SMIN Requests", "count", 1 },
    { "HZOP_8_M_MIN", "HZOP_8_M_MIN Requests", "count", 1 },
    { "HZOP_8_M_SWAP", "HZOP_8_M_SWAP Requests", "count", 1 },
    { "HZOP_8_M_CAS", "HZOP_8_M_CAS Requests", "count", 1 },
    { "HZOP_8_M_FADD", "HZOP_8_M_FADD Requests", "count", 1 },
    { "HZOP_8_M_FSUB", "HZOP_8_M_FSUB Requests", "count", 1 },
    { "HZOP_8_M_FRSUB", "HZOP_8_M_FRSUB Requests", "count", 1 },
    { "HZOP_16_M_ADD", "HZOP_16_M_ADD Requests", "count", 1 },
    { "HZOP_16_M_AND", "HZOP_16_M_AND Requests", "count", 1 },
    { "HZOP_16_M_OR", "HZOP_16_M_OR Requests", "count", 1 },
    { "HZOP_16_M_XOR", "HZOP_16_M_XOR Requests", "count", 1 },
    { "HZOP_16_M_SMAX", "HZOP_16_M_SMAX Requests", "count", 1 },
    { "HZOP_16_M_MAX", "HZOP_16_M_MAX Requests", "count", 1 },
    { "HZOP_16_M_SMIN", "HZOP_16_M_SMIN Requests", "count", 1 },
    { "HZOP_16_M_MIN", "HZOP_16_M_MIN Requests", "count", 1 },
    { "HZOP_16_M_SWAP", "HZOP_16_M_SWAP Requests", "count", 1 },
    { "HZOP_16_M_CAS", "HZOP_16_M_CAS Requests", "count", 1 },
    { "HZOP_16_M_FADD", "HZOP_16_M_FADD Requests", "count", 1 },
    { "HZOP_16_M_FSUB", "HZOP_16_M_FSUB Requests", "count", 1 },
    { "HZOP_16_M_FRSUB", "HZOP_16_M_FRSUB Requests", "count", 1 },
    { "HZOP_32_M_ADD", "HZOP_32_M_ADD Requests", "count", 1 },
    { "HZOP_32_M_AND", "HZOP_32_M_AND Requests", "count", 1 },
    { "HZOP_32_M_OR", "HZOP_32_M_OR Requests", "count", 1 },
    { "HZOP_32_M_XOR", "HZOP_32_M_XOR Requests", "count", 1 },
    { "HZOP_32_M_SMAX", "HZOP_32_M_SMAX Requests", "count", 1 },
    { "HZOP_32_M_MAX", "HZOP_32_M_MAX Requests", "count", 1 },
    { "HZOP_32_M_SMIN", "HZOP_32_M_SMIN Requests", "count", 1 },
    { "HZOP_32_M_MIN", "HZOP_32_M_MIN Requests", "count", 1 },
    { "HZOP_32_M_SWAP", "HZOP_32_M_SWAP Requests", "count", 1 },
    { "HZOP_32_M_CAS", "HZOP_32_M_CAS Requests", "count", 1 },
    { "HZOP_32_M_FADD", "HZOP_32_M_FADD Requests", "count", 1 },
    { "HZOP_32_M_FSUB", "HZOP_32_M_FSUB Requests", "count", 1 },
    { "HZOP_32_M_FRSUB", "HZOP_32_M_FRSUB Requests", "count", 1 },
    { "HZOP_64_M_ADD", "HZOP_64_M_ADD Requests", "count", 1 },
    { "HZOP_64_M_AND", "HZOP_64_M_AND Requests", "count", 1 },
    { "HZOP_64_M_OR", "HZOP_64_M_OR Requests", "count", 1 },
    { "HZOP_64_M_XOR", "HZOP_64_M_XOR Requests", "count", 1 },
    { "HZOP_64_M_SMAX", "HZOP_64_M_SMAX Requests", "count", 1 },
    { "HZOP_64_M_MAX", "HZOP_64_M_MAX Requests", "count", 1 },
    { "HZOP_64_M_SMIN", "HZOP_64_M_SMIN Requests", "count", 1 },
    { "HZOP_64_M_MIN", "HZOP_64_M_MIN Requests", "count", 1 },
    { "HZOP_64_M_SWAP", "HZOP_64_M_SWAP Requests", "count", 1 },
    { "HZOP_64_M_CAS", "HZOP_64_M_CAS Requests", "count", 1 },
    { "HZOP_64_M_FADD", "HZOP_64_M_FADD Requests", "count", 1 },
    { "HZOP_64_M_FSUB", "HZOP_64_M_FSUB Requests", "count", 1 },
    { "HZOP_64_M_FRSUB", "HZOP_64_M_FRSUB Requests", "count", 1 },
    { "HZOP_8_S_ADD", "HZOP_8_S_ADD Requests", "count", 1 },
    { "HZOP_8_S_AND", "HZOP_8_S_AND Requests", "count", 1 },
    { "HZOP_8_S_OR", "HZOP_8_S_OR Requests", "count", 1 },
    { "HZOP_8_S_XOR", "HZOP_8_S_XOR Requests", "count", 1 },
    { "HZOP_8_S_SMAX", "HZOP_8_S_SMAX Requests", "count", 1 },
    { "HZOP_8_S_MAX", "HZOP_8_S_MAX Requests", "count", 1 },
    { "HZOP_8_S_SMIN", "HZOP_8_S_SMIN Requests", "count", 1 },
    { "HZOP_8_S_MIN", "HZOP_8_S_MIN Requests", "count", 1 },
    { "HZOP_8_S_SWAP", "HZOP_8_S_SWAP Requests", "count", 1 },
    { "HZOP_8_S_CAS", "HZOP_8_S_CAS Requests", "count", 1 },
    { "HZOP_8_S_FADD", "HZOP_8_S_FADD Requests", "count", 1 },
    { "HZOP_8_S_FSUB", "HZOP_8_S_FSUB Requests", "count", 1 },
    { "HZOP_8_S_FRSUB", "HZOP_8_S_FRSUB Requests", "count", 1 },
    { "HZOP_16_S_ADD", "HZOP_16_S_ADD Requests", "count", 1 },
    { "HZOP_16_S_AND", "HZOP_16_S_AND Requests", "count", 1 },
    { "HZOP_16_S_OR", "HZOP_16_S_OR Requests", "count", 1 },
    { "HZOP_16_S_XOR", "HZOP_16_S_XOR Requests", "count", 1 },
    { "HZOP_16_S_SMAX", "HZOP_16_S_SMAX Requests", "count", 1 },
    { "HZOP_16_S_MAX", "HZOP_16_S_MAX Requests", "count", 1 },
    { "HZOP_16_S_SMIN", "HZOP_16_S_SMIN Requests", "count", 1 },
    { "HZOP_16_S_MIN", "HZOP_16_S_MIN Requests", "count", 1 },
    { "HZOP_16_S_SWAP", "HZOP_16_S_SWAP Requests", "count", 1 },
    { "HZOP_16_S_CAS", "HZOP_16_S_CAS Requests", "count", 1 },
    { "HZOP_16_S_FADD", "HZOP_16_S_FADD Requests", "count", 1 },
    { "HZOP_16_S_FSUB", "HZOP_16_S_FSUB Requests", "count", 1 },
    { "HZOP_16_S_FRSUB", "HZOP_16_S_FRSUB Requests", "count", 1 },
    { "HZOP_32_S_ADD", "HZOP_32_S_ADD Requests", "count", 1 },
    { "HZOP_32_S_AND", "HZOP_32_S_AND Requests", "count", 1 },
    { "HZOP_32_S_OR", "HZOP_32_S_OR Requests", "count", 1 },
    { "HZOP_32_S_XOR", "HZOP_32_S_XOR Requests", "count", 1 },
    { "HZOP_32_S_SMAX", "HZOP_32_S_SMAX Requests", "count", 1 },
    { "HZOP_32_S_MAX", "HZOP_32_S_MAX Requests", "count", 1 },
    { "HZOP_32_S_SMIN", "HZOP_32_S_SMIN Requests", "count", 1 },
    { "HZOP_32_S_MIN", "HZOP_32_S_MIN Requests", "count", 1 },
    { "HZOP_32_S_SWAP", "HZOP_32_S_SWAP Requests", "count", 1 },
    { "HZOP_32_S_CAS", "HZOP_32_S_CAS Requests", "count", 1 },
    { "HZOP_32_S_FADD", "HZOP_32_S_FADD Requests", "count", 1 },
    { "HZOP_32_S_FSUB", "HZOP_32_S_FSUB Requests", "count", 1 },
    { "HZOP_32_S_FRSUB", "HZOP_32_S_FRSUB Requests", "count", 1 },
    { "HZOP_64_S_ADD", "HZOP_64_S_ADD Requests", "count", 1 },
    { "HZOP_64_S_AND", "HZOP_64_S_AND Requests", "count", 1 },
    { "HZOP_64_S_OR", "HZOP_64_S_OR Requests", "count", 1 },
    { "HZOP_64_S_XOR", "HZOP_64_S_XOR Requests", "count", 1 },
    { "HZOP_64_S_SMAX", "HZOP_64_S_SMAX Requests", "count", 1 },
    { "HZOP_64_S_MAX", "HZOP_64_S_MAX Requests", "count", 1 },
    { "HZOP_64_S_SMIN", "HZOP_64_S_SMIN Requests", "count", 1 },
    { "HZOP_64_S_MIN", "HZOP_64_S_MIN Requests", "count", 1 },
    { "HZOP_64_S_SWAP", "HZOP_64_S_SWAP Requests", "count", 1 },
    { "HZOP_64_S_CAS", "HZOP_64_S_CAS Requests", "count", 1 },
    { "HZOP_64_S_FADD", "HZOP_64_S_FADD Requests", "count", 1 },
    { "HZOP_64_S_FSUB", "HZOP_64_S_FSUB Requests", "count", 1 },
    { "HZOP_64_S_FRSUB", "HZOP_64_S_FRSUB Requests", "count", 1 },
    { "HZOP_8_MS_ADD", "HZOP_8_MS_ADD Requests", "count", 1 },
    { "HZOP_8_MS_AND", "HZOP_8_MS_AND Requests", "count", 1 },
    { "HZOP_8_MS_OR", "HZOP_8_MS_OR Requests", "count", 1 },
    { "HZOP_8_MS_XOR", "HZOP_8_MS_XOR Requests", "count", 1 },
    { "HZOP_8_MS_SMAX", "HZOP_8_MS_SMAX Requests", "count", 1 },
    { "HZOP_8_MS_MAX", "HZOP_8_MS_MAX Requests", "count", 1 },
    { "HZOP_8_MS_SMIN", "HZOP_8_MS_SMIN Requests", "count", 1 },
    { "HZOP_8_MS_MIN", "HZOP_8_MS_MIN Requests", "count", 1 },
    { "HZOP_8_MS_SWAP", "HZOP_8_MS_SWAP Requests", "count", 1 },
    { "HZOP_8_MS_CAS", "HZOP_8_MS_CAS Requests", "count", 1 },
    { "HZOP_8_MS_FADD", "HZOP_8_MS_FADD Requests", "count", 1 },
    { "HZOP_8_MS_FSUB", "HZOP_8_MS_FSUB Requests", "count", 1 },
    { "HZOP_8_MS_FRSUB", "HZOP_8_MS_FRSUB Requests", "count", 1 },
    { "HZOP_16_MS_ADD", "HZOP_16_MS_ADD Requests", "count", 1 },
    { "HZOP_16_MS_AND", "HZOP_16_MS_AND Requests", "count", 1 },
    { "HZOP_16_MS_OR", "HZOP_16_MS_OR Requests", "count", 1 },
    { "HZOP_16_MS_XOR", "HZOP_16_MS_XOR Requests", "count", 1 },
    { "HZOP_16_MS_SMAX", "HZOP_16_MS_SMAX Requests", "count", 1 },
    { "HZOP_16_MS_MAX", "HZOP_16_MS_MAX Requests", "count", 1 },
    { "HZOP_16_MS_SMIN", "HZOP_16_MS_SMIN Requests", "count", 1 },
    { "HZOP_16_MS_MIN", "HZOP_16_MS_MIN Requests", "count", 1 },
    { "HZOP_16_MS_SWAP", "HZOP_16_MS_SWAP Requests", "count", 1 },
    { "HZOP_16_MS_CAS", "HZOP_16_MS_CAS Requests", "count", 1 },
    { "HZOP_16_MS_FADD", "HZOP_16_MS_FADD Requests", "count", 1 },
    { "HZOP_16_MS_FSUB", "HZOP_16_MS_FSUB Requests", "count", 1 },
    { "HZOP_16_MS_FRSUB", "HZOP_16_MS_FRSUB Requests", "count", 1 },
    { "HZOP_32_MS_ADD", "HZOP_32_MS_ADD Requests", "count", 1 },
    { "HZOP_32_MS_AND", "HZOP_32_MS_AND Requests", "count", 1 },
    { "HZOP_32_MS_OR", "HZOP_32_MS_OR Requests", "count", 1 },
    { "HZOP_32_MS_XOR", "HZOP_32_MS_XOR Requests", "count", 1 },
    { "HZOP_32_MS_SMAX", "HZOP_32_MS_SMAX Requests", "count", 1 },
    { "HZOP_32_MS_MAX", "HZOP_32_MS_MAX Requests", "count", 1 },
    { "HZOP_32_MS_SMIN", "HZOP_32_MS_SMIN Requests", "count", 1 },
    { "HZOP_32_MS_MIN", "HZOP_32_MS_MIN Requests", "count", 1 },
    { "HZOP_32_MS_SWAP", "HZOP_32_MS_SWAP Requests", "count", 1 },
    { "HZOP_32_MS_CAS", "HZOP_32_MS_CAS Requests", "count", 1 },
    { "HZOP_32_MS_FADD", "HZOP_32_MS_FADD Requests", "count", 1 },
    { "HZOP_32_MS_FSUB", "HZOP_32_MS_FSUB Requests", "count", 1 },
    { "HZOP_32_MS_FRSUB", "HZOP_32_MS_FRSUB Requests", "count", 1 },
    { "HZOP_64_MS_ADD", "HZOP_64_MS_ADD Requests", "count", 1 },
    { "HZOP_64_MS_AND", "HZOP_64_MS_AND Requests", "count", 1 },
    { "HZOP_64_MS_OR", "HZOP_64_MS_OR Requests", "count", 1 },
    { "HZOP_64_MS_XOR", "HZOP_64_MS_XOR Requests", "count", 1 },
    { "HZOP_64_MS_SMAX", "HZOP_64_MS_SMAX Requests", "count", 1 },
    { "HZOP_64_MS_MAX", "HZOP_64_MS_MAX Requests", "count", 1 },
    { "HZOP_64_MS_SMIN", "HZOP_64_MS_SMIN Requests", "count", 1 },
    { "HZOP_64_MS_MIN", "HZOP_64_MS_MIN Requests", "count", 1 },
    { "HZOP_64_MS_SWAP", "HZOP_64_MS_SWAP Requests", "count", 1 },
    { "HZOP_64_MS_CAS", "HZOP_64_MS_CAS Requests", "count", 1 },
    { "HZOP_64_MS_FADD", "HZOP_64_MS_FADD Requests", "count", 1 },
    { "HZOP_64_MS_FSUB", "HZOP_64_MS_FSUB Requests", "count", 1 },
    { "HZOP_64_MS_FRSUB", "HZOP_64_MS_FRSUB Requests", "count", 1 },
  )

  enum hzopStats : uint32_t {
    HZOP_8_BASE_ADD    = 0,
    HZOP_8_BASE_AND    = 1,
    HZOP_8_BASE_OR     = 2,
    HZOP_8_BASE_XOR    = 3,
    HZOP_8_BASE_SMAX   = 4,
    HZOP_8_BASE_MAX    = 5,
    HZOP_8_BASE_SMIN   = 6,
    HZOP_8_BASE_MIN    = 7,
    HZOP_8_BASE_SWAP   = 8,
    HZOP_8_BASE_CAS    = 9,
    HZOP_8_BASE_FADD   = 10,
    HZOP_8_BASE_FSUB   = 11,
    HZOP_8_BASE_FRSUB  = 12,
    HZOP_16_BASE_ADD   = 13,
    HZOP_16_BASE_AND   = 14,
    HZOP_16_BASE_OR    = 15,
    HZOP_16_BASE_XOR   = 16,
    HZOP_16_BASE_SMAX  = 17,
    HZOP_16_BASE_MAX   = 18,
    HZOP_16_BASE_SMIN  = 19,
    HZOP_16_BASE_MIN   = 20,
    HZOP_16_BASE_SWAP  = 21,
    HZOP_16_BASE_CAS   = 22,
    HZOP_16_BASE_FADD  = 23,
    HZOP_16_BASE_FSUB  = 24,
    HZOP_16_BASE_FRSUB = 25,
    HZOP_32_BASE_ADD   = 26,
    HZOP_32_BASE_AND   = 27,
    HZOP_32_BASE_OR    = 28,
    HZOP_32_BASE_XOR   = 29,
    HZOP_32_BASE_SMAX  = 30,
    HZOP_32_BASE_MAX   = 31,
    HZOP_32_BASE_SMIN  = 32,
    HZOP_32_BASE_MIN   = 33,
    HZOP_32_BASE_SWAP  = 34,
    HZOP_32_BASE_CAS   = 35,
    HZOP_32_BASE_FADD  = 36,
    HZOP_32_BASE_FSUB  = 37,
    HZOP_32_BASE_FRSUB = 38,
    HZOP_64_BASE_ADD   = 39,
    HZOP_64_BASE_AND   = 40,
    HZOP_64_BASE_OR    = 41,
    HZOP_64_BASE_XOR   = 42,
    HZOP_64_BASE_SMAX  = 43,
    HZOP_64_BASE_MAX   = 44,
    HZOP_64_BASE_SMIN  = 45,
    HZOP_64_BASE_MIN   = 46,
    HZOP_64_BASE_SWAP  = 47,
    HZOP_64_BASE_CAS   = 48,
    HZOP_64_BASE_FADD  = 49,
    HZOP_64_BASE_FSUB  = 50,
    HZOP_64_BASE_FRSUB = 51,
    HZOP_8_M_ADD       = 52,
    HZOP_8_M_AND       = 53,
    HZOP_8_M_OR        = 54,
    HZOP_8_M_XOR       = 55,
    HZOP_8_M_SMAX      = 56,
    HZOP_8_M_MAX       = 57,
    HZOP_8_M_SMIN      = 58,
    HZOP_8_M_MIN       = 59,
    HZOP_8_M_SWAP      = 60,
    HZOP_8_M_CAS       = 61,
    HZOP_8_M_FADD      = 62,
    HZOP_8_M_FSUB      = 63,
    HZOP_8_M_FRSUB     = 64,
    HZOP_16_M_ADD      = 65,
    HZOP_16_M_AND      = 66,
    HZOP_16_M_OR       = 67,
    HZOP_16_M_XOR      = 68,
    HZOP_16_M_SMAX     = 69,
    HZOP_16_M_MAX      = 70,
    HZOP_16_M_SMIN     = 71,
    HZOP_16_M_MIN      = 72,
    HZOP_16_M_SWAP     = 73,
    HZOP_16_M_CAS      = 74,
    HZOP_16_M_FADD     = 75,
    HZOP_16_M_FSUB     = 76,
    HZOP_16_M_FRSUB    = 77,
    HZOP_32_M_ADD      = 78,
    HZOP_32_M_AND      = 79,
    HZOP_32_M_OR       = 80,
    HZOP_32_M_XOR      = 81,
    HZOP_32_M_SMAX     = 82,
    HZOP_32_M_MAX      = 83,
    HZOP_32_M_SMIN     = 84,
    HZOP_32_M_MIN      = 85,
    HZOP_32_M_SWAP     = 86,
    HZOP_32_M_CAS      = 87,
    HZOP_32_M_FADD     = 88,
    HZOP_32_M_FSUB     = 89,
    HZOP_32_M_FRSUB    = 90,
    HZOP_64_M_ADD      = 91,
    HZOP_64_M_AND      = 92,
    HZOP_64_M_OR       = 93,
    HZOP_64_M_XOR      = 94,
    HZOP_64_M_SMAX     = 95,
    HZOP_64_M_MAX      = 96,
    HZOP_64_M_SMIN     = 97,
    HZOP_64_M_MIN      = 98,
    HZOP_64_M_SWAP     = 99,
    HZOP_64_M_CAS      = 100,
    HZOP_64_M_FADD     = 101,
    HZOP_64_M_FSUB     = 102,
    HZOP_64_M_FRSUB    = 103,
    HZOP_8_S_ADD       = 104,
    HZOP_8_S_AND       = 105,
    HZOP_8_S_OR        = 106,
    HZOP_8_S_XOR       = 107,
    HZOP_8_S_SMAX      = 108,
    HZOP_8_S_MAX       = 109,
    HZOP_8_S_SMIN      = 110,
    HZOP_8_S_MIN       = 111,
    HZOP_8_S_SWAP      = 112,
    HZOP_8_S_CAS       = 113,
    HZOP_8_S_FADD      = 114,
    HZOP_8_S_FSUB      = 115,
    HZOP_8_S_FRSUB     = 116,
    HZOP_16_S_ADD      = 117,
    HZOP_16_S_AND      = 118,
    HZOP_16_S_OR       = 119,
    HZOP_16_S_XOR      = 120,
    HZOP_16_S_SMAX     = 121,
    HZOP_16_S_MAX      = 122,
    HZOP_16_S_SMIN     = 123,
    HZOP_16_S_MIN      = 124,
    HZOP_16_S_SWAP     = 125,
    HZOP_16_S_CAS      = 126,
    HZOP_16_S_FADD     = 127,
    HZOP_16_S_FSUB     = 128,
    HZOP_16_S_FRSUB    = 129,
    HZOP_32_S_ADD      = 130,
    HZOP_32_S_AND      = 131,
    HZOP_32_S_OR       = 132,
    HZOP_32_S_XOR      = 133,
    HZOP_32_S_SMAX     = 134,
    HZOP_32_S_MAX      = 135,
    HZOP_32_S_SMIN     = 136,
    HZOP_32_S_MIN      = 137,
    HZOP_32_S_SWAP     = 138,
    HZOP_32_S_CAS      = 139,
    HZOP_32_S_FADD     = 140,
    HZOP_32_S_FSUB     = 141,
    HZOP_32_S_FRSUB    = 142,
    HZOP_64_S_ADD      = 143,
    HZOP_64_S_AND      = 144,
    HZOP_64_S_OR       = 145,
    HZOP_64_S_XOR      = 146,
    HZOP_64_S_SMAX     = 147,
    HZOP_64_S_MAX      = 148,
    HZOP_64_S_SMIN     = 149,
    HZOP_64_S_MIN      = 150,
    HZOP_64_S_SWAP     = 151,
    HZOP_64_S_CAS      = 152,
    HZOP_64_S_FADD     = 153,
    HZOP_64_S_FSUB     = 154,
    HZOP_64_S_FRSUB    = 155,
    HZOP_8_MS_ADD      = 156,
    HZOP_8_MS_AND      = 157,
    HZOP_8_MS_OR       = 158,
    HZOP_8_MS_XOR      = 159,
    HZOP_8_MS_SMAX     = 160,
    HZOP_8_MS_MAX      = 161,
    HZOP_8_MS_SMIN     = 162,
    HZOP_8_MS_MIN      = 163,
    HZOP_8_MS_SWAP     = 164,
    HZOP_8_MS_CAS      = 165,
    HZOP_8_MS_FADD     = 166,
    HZOP_8_MS_FSUB     = 167,
    HZOP_8_MS_FRSUB    = 168,
    HZOP_16_MS_ADD     = 169,
    HZOP_16_MS_AND     = 170,
    HZOP_16_MS_OR      = 171,
    HZOP_16_MS_XOR     = 172,
    HZOP_16_MS_SMAX    = 173,
    HZOP_16_MS_MAX     = 174,
    HZOP_16_MS_SMIN    = 175,
    HZOP_16_MS_MIN     = 176,
    HZOP_16_MS_SWAP    = 177,
    HZOP_16_MS_CAS     = 178,
    HZOP_16_MS_FADD    = 179,
    HZOP_16_MS_FSUB    = 180,
    HZOP_16_MS_FRSUB   = 181,
    HZOP_32_MS_ADD     = 182,
    HZOP_32_MS_AND     = 183,
    HZOP_32_MS_OR      = 184,
    HZOP_32_MS_XOR     = 185,
    HZOP_32_MS_SMAX    = 186,
    HZOP_32_MS_MAX     = 187,
    HZOP_32_MS_SMIN    = 188,
    HZOP_32_MS_MIN     = 189,
    HZOP_32_MS_SWAP    = 190,
    HZOP_32_MS_CAS     = 191,
    HZOP_32_MS_FADD    = 192,
    HZOP_32_MS_FSUB    = 193,
    HZOP_32_MS_FRSUB   = 194,
    HZOP_64_MS_ADD     = 195,
    HZOP_64_MS_AND     = 196,
    HZOP_64_MS_OR      = 197,
    HZOP_64_MS_XOR     = 198,
    HZOP_64_MS_SMAX    = 199,
    HZOP_64_MS_MAX     = 200,
    HZOP_64_MS_SMIN    = 201,
    HZOP_64_MS_MIN     = 202,
    HZOP_64_MS_SWAP    = 203,
    HZOP_64_MS_CAS     = 204,
    HZOP_64_MS_FADD    = 205,
    HZOP_64_MS_FSUB    = 206,
    HZOP_64_MS_FRSUB   = 207,
    HZOP_END           = 208,
  };

  /// RZAAMOCoProc: default constructor
  RZAAMOCoProc( ComponentId_t id, Params& params, RevCore* parent );

  /// RZAAMOCoProc: default destructor
  virtual ~RZAAMOCoProc();

  /// RZAAMOCoProc: clock tick function
  virtual bool ClockTick( SST::Cycle_t cycle ) override;

  /// RZAAMOCoProc: Enqueue a new instruction
  virtual bool IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) override;

  /// RZAAMOCoProc: reset the coproc
  virtual bool Reset() override;

  /// RZAAMOCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() override { return Reset(); }

  /// RZAAMOCoProc: determines whether the coproc is complete
  virtual bool IsDone() override;

  /// RZAMOCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP( Forza::zopEvent* zev, bool& flag ) override;

  /// RZAAMOCoProc: marks a load as being complete
  virtual void MarkLoadComplete( const MemReq& req ) override;

  /// RZAAMOCoProc: Set the memory handler
  virtual void setMem( RevMem* M ) override { Mem = M; }

  /// RZAAMOCoProc: Set the ZOP NIC handler
  virtual void setZNic( Forza::zopAPI* Z ) override { zNic = Z; }

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

#define AMOQ_ZEV 0
#define AMOQ_RS1 1
#define AMOQ_RS2 2
  std::vector<std::tuple<Forza::zopEvent*, unsigned, unsigned>> AMOQ;  ///< RZAAMOCoProc: Outstanding load queue

  std::function<void( const MemReq& )> MarkLoadCompleteFunc;  ///< RZAAMOCoProc: Hazard function

  std::vector<Statistic<uint64_t>*> stats;  ///< RZAAMOCoProc: Statistics handlers

  /// RZAAMOCoProc: checks the load queues for completed operations and clears hazards
  void CheckLSQueue();

};  // RZAAMOCoProc
}  //namespace SST::RevCPU

#endif
