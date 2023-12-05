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
#include <queue>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <tuple>
#include <vector>
#include <mutex>

// -- SST Headers
#include "SST.h"

// -- RevCPU Headers
#include "RevCore.h"
#include "RevCorePasskey.h"
#include "RevFeature.h"
#include "RevInstTable.h"
#include "ZOPNET.h"
#include "RevMem.h"
#include "RevOpts.h"

namespace SST::RevCPU {
class RevCore;

#define _RA_NUM_REG 4096
#define _UNDEF_REG  (((_RA_NUM_REG))+1)

// ----------------------------------------
// RegAlloc
// ----------------------------------------
class RegAlloc{
public:
  /// RegAlloc: constructor
  RegAlloc(){
    for( unsigned i=0; i<_RA_NUM_REG; i++ ){
      hazard[i] = false;
      regs[i] = 0x00ull;
    }
  }

  /// RegAlloc: destructor
  ~RegAlloc(){}

  /// RegAlloc: retrieve a single operand register
  bool getRegs(unsigned &rs1){
    unsigned t_rs1 = _RA_NUM_REG+1;

    unsigned cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rs1 = cur;
        break;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    rs1 = t_rs1;
    return true;
  }

  /// RegAlloc: retrieve a two operand register set
  bool getRegs(unsigned &rs1, unsigned &rs2){
    unsigned t_rs1 = _RA_NUM_REG+1;
    unsigned t_rs2 = _RA_NUM_REG+1;

    // find two register slots
    unsigned cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rs1 = cur;
        break;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rs2 = cur;
        break;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    rs1 = t_rs1;
    rs2 = t_rs2;
    return true;
  }

  /// RegAlloc: retrieve a three operand register set
  bool getRegs(unsigned &rd, unsigned &rs1, unsigned &rs2){
    unsigned t_rd = _RA_NUM_REG+1;
    unsigned t_rs1 = _RA_NUM_REG+1;
    unsigned t_rs2 = _RA_NUM_REG+1;

    // find three register slots
    // -- Rd
    unsigned cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rd = cur;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    // -- Rs1
    cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rs1 = cur;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    // -- Rs2
    cur = 0;
    while( cur < _RA_NUM_REG ){
      if( !hazard[cur] ){
        hazard[cur] = true;
        t_rs2 = cur;
      }else{
        cur++;
        if( cur == _RA_NUM_REG ){
          return false;
        }
      }
    }

    rd = t_rd;
    rs1 = t_rs1;
    rs2 = t_rs2;
    return true;
  }

  /// RegAlloc: clear the hazard on the target register
  void clearReg(unsigned reg){
    if( reg < _RA_NUM_REG ){
      hazard[reg] = false;
      regs[reg] = 0x00ull;
    }
  }

  /// RegAlloc: set the target value
  void SetX(unsigned Idx, uint64_t Val){
    if( Idx < _RA_NUM_REG-1 ){
      regs[Idx] = Val;
    }
  }

  /// RegAlloc: get the target value
  uint64_t GetX(unsigned Idx){
    if( Idx < _RA_NUM_REG-1 ){
      return regs[Idx];
    }
  }

private:
  bool hazard[_RA_NUM_REG];
  uint64_t regs[_RA_NUM_REG];
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
  virtual bool IssueInst( RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) = 0;

  /// ReCoProc: Reset - called on startup
  virtual bool Reset()                                                             = 0;

  /// RevCoProc: Teardown - called when associated RevCore completes
  virtual bool Teardown()                                                          = 0;

  /// RevCoProc: Clock - can be called by SST or by overriding RevCPU
  virtual bool ClockTick( SST::Cycle_t cycle )                                     = 0;

  /// RevCoProc: Returns true when co-processor has completed execution
  ///            - used for proper exiting of associated RevCore
  virtual bool IsDone()                                                            = 0;

  // --------------------
  // FORZA virtual methods
  // --------------------
  /// RevCoProc: injects a zop packet into the coproc pipeline
  virtual bool InjectZOP(Forza::zopEvent *zev, bool &flag){ return true; }

  /// RevCoProc: Set the memory handler
  virtual void setMem(RevMem *M){}

  /// RevCoProc: Set the ZOP NIC handler
  virtual void setZNic(Forza::zopAPI *Z){}

  // --------------------
  // FORZA methods
  // --------------------
  /// RevCoProc: Sends a successful response ZOP
  bool sendSuccessResp(Forza::zopAPI *zNic,
                       Forza::zopEvent *zev,
                       uint16_t Hart);

  /// RevCoProc: Sends a successful LOAD data response ZOP
  bool sendSuccessResp(Forza::zopAPI *zNic,
                       Forza::zopEvent *zev,
                       uint16_t Hart,
                       uint64_t Data);

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
  virtual bool IssueInst( RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst );

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
    RevCoProcInst( uint32_t inst, RevFeature* F, RevRegFile* R, RevMem* M ) : Inst( inst ), Feature( F ), RegFile( R ), Mem( M ) {}

    uint32_t const    Inst;
    RevFeature* const Feature;
    RevRegFile* const RegFile;
    RevMem* const     Mem;
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

  /// RZALSCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP(Forza::zopEvent *zev, bool &flag) override;

  /// RZALSCoProc: Set the memory handler
  virtual void setMem(RevMem *M) override{
    Mem = M;
  }

  /// RZALSCoProc: Set the ZOP NIC handler
  virtual void setZNic(Forza::zopAPI *Z) override{
    zNic = Z;
  }

private:
  RevMem *Mem;          ///< RZALSCoProc: RevMem object
  Forza::zopAPI *zNic;  ///< RZALSCoProc: ZOPNic object
  RevFeature *Feature;  ///< RZALSCoProc: Feature object
  RevRegFile *RegFile;  ///< RZALSCoProc: Regfile object
  RegAlloc Alloc;       ///< RZALSCoProc: Register allocator object

  /// Handle the incoming MZOP request
  bool handleMZOP(Forza::zopEvent *zev, bool &flag);

#define LOADQ_ZEV   0
#define LOADQ_RS1   1
#define LOADQ_RS2   2
  std::vector<std::tuple<Forza::zopEvent *,unsigned, unsigned>> LoadQ; ///< RZALSCoProc: Outstanding load queue

  std::shared_ptr<std::unordered_map<uint64_t, MemReq>> LSQueue; ///< RZALSCoProc: Load / Store queue used to track memory operations. Currently only tracks outstanding loads.

  std::function<void(const MemReq&)> MarkLoadCompleteFunc;  ///< RZALSCoProc: Hazard function

  /// RZALSCoProc: checks the load queues for completed operations and clears hazards
  void CheckLSQueue();
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

  /// RZAMOCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP(Forza::zopEvent *zev, bool &flag) override;

  /// RZAAMOCoProc: Set the memory handler
  virtual void setMem(RevMem *M) override{
    Mem = M;
  }

  /// RZAAMOCoProc: Set the ZOP NIC handler
  virtual void setZNic(Forza::zopAPI *Z)override{
    zNic = Z;
  }

private:
  RevMem *Mem;          ///< RZAAMOCoProc: RevMem object
  Forza::zopAPI *zNic;  ///< RZAAMOCoProc: ZOPNic object
  RevFeature *Feature;  ///< RZAAMOCoProc: Feature object
  RevRegFile *RegFile;  ///< RZAAMOCoProc: Regfile object
  RegAlloc Alloc;       ///< RZAAMOCoProc: Register allocator object

#define AMOQ_ZEV   0
#define AMOQ_RS1   1
#define AMOQ_RS2   2
  std::vector<std::tuple<Forza::zopEvent *,unsigned, unsigned>> AMOQ; ///< RZAAMOCoProc: Outstanding load queue

  std::function<void(const MemReq&)> MarkLoadCompleteFunc;  ///< RZAAMOCoProc: Hazard function

  std::shared_ptr<std::unordered_map<uint64_t, MemReq>> LSQueue; ///< RZAAMOCoProc: Load / Store queue used to track memory operations. Currently only tracks outstanding loads.
};  // RZAAMOCoProc
}  //namespace SST::RevCPU

#endif
