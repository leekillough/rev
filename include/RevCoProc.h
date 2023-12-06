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
#include <mutex>

// -- SST Headers
#include "SST.h"

// -- RevCPU Headers
#include "RevOpts.h"
#include "RevFeature.h"
#include "RevMem.h"
#include "RevInstTable.h"
#include "RevProcPasskey.h"
#include "RevProc.h"
#include "ZOPNET.h"

namespace SST::RevCPU{
class RevProc;

#define _RA_NUM_REG 4096
#define _UNDEF_REG  (((_RA_NUM_REG))+1)

// ----------------------------------------
// RegAlloc
// ----------------------------------------
#define _H_CLEAR  0
#define _H_SET    1
#define _H_DIRTY  2
class RegAlloc{
public:
  /// RegAlloc: constructor
  RegAlloc(){
    for( unsigned i=0; i<_RA_NUM_REG; i++ ){
      hazard[i] = _H_CLEAR;
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
      if( hazard[cur] == _H_CLEAR){
        hazard[cur] = _H_SET;
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
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_SET;
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
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_SET;
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
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_SET;
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
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_SET;
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
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_SET;
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
      hazard[reg] = _H_CLEAR;
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
    return 0x00ull;
  }

  /// RegAlloc: get the address for the target reigster
  uint64_t *getRegAddr(unsigned Idx){
    if( Idx < _RA_NUM_REG-1 ){
      return (&regs[Idx]);
    }
    return nullptr;
  }

  /// RegAlloc: get the hazard state
  uint8_t getState( unsigned Idx ){
    if( Idx < _RA_NUM_REG-1 ){
      return hazard[Idx];
    }
    return _H_DIRTY;
  }

  /// RegAlloc: set the target register as dirty
  void setDirty( unsigned Idx ){
    if( Idx < _RA_NUM_REG-1 ){
      hazard[Idx] = _H_DIRTY;
    }
  }

  uint64_t regs[_RA_NUM_REG]; ///< RegAlloc: register array

private:
  uint8_t hazard[_RA_NUM_REG];
};

// ----------------------------------------
// RevCoProc
// ----------------------------------------
class RevCoProc : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API(SST::RevCPU::RevCoProc, RevProc*);
  SST_ELI_DOCUMENT_PARAMS({ "verbose", "Set the verbosity of output for the attached co-processor", "0" });

  // --------------------
  // Virtual methods
  // --------------------

  /// RevCoProc: Constructor
  RevCoProc( ComponentId_t id, Params& params, RevProc* parent);

  /// RevCoProc: default destructor
  virtual ~RevCoProc();

  /// RevCoProc: send raw data to the coprocessor
  virtual bool sendRawData(std::vector<uint8_t> Data){
    return true;
  }

  /// RevCoProc: retrieve raw data from the coprocessor
  virtual const std::vector<uint8_t> getRawData(){
    output->fatal(CALL_INFO, -1,
                  "Error : no override method defined for getRawData()\n");

    // inserting code to quiesce warnings
    std::vector<uint8_t> D;
    return D;
  }

  // --------------------
  // Pure virtual methods
  // --------------------

  /// RevCoProc: Instruction interface to RevCore
  virtual bool IssueInst(RevFeature *F, RevRegFile *R, RevMem *M, uint32_t Inst) = 0;

  /// ReCoProc: Reset - called on startup
  virtual bool Reset() = 0;

  /// RevCoProc: Teardown - called when associated RevProc completes
  virtual bool Teardown() = 0;

  /// RevCoProc: Clock - can be called by SST or by overriding RevCPU
  virtual bool ClockTick(SST::Cycle_t cycle) = 0;

  /// RevCoProc: Returns true when co-processor has completed execution
  ///            - used for proper exiting of associated RevProc
  virtual bool IsDone() = 0;

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

  /// RevCoProc: Virtual mark load complete method
  virtual void MarkLoadComplete(const MemReq& req){}

protected:
  SST::Output*   output;                                ///< RevCoProc: sst output object
  RevProc* const parent;                                ///< RevCoProc: Pointer to RevProc this CoProc is attached to

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
  virtual bool ClockTick(SST::Cycle_t cycle) override;

  /// RZALSCoProc: Enqueue a new instruction
  virtual bool IssueInst(RevFeature *F, RevRegFile *R,
                         RevMem *M, uint32_t Inst) override;

  /// RZALSCoProc: reset the coproc
  virtual bool Reset() override;

  /// RZALSCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() override { return Reset(); }

  /// RZALSCoProc: determines whether the coproc is complete
  virtual bool IsDone() override;

  /// RZALSCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP(Forza::zopEvent *zev, bool &flag) override;

  /// RZALSCoProc: marks a load as being complete
  virtual void MarkLoadComplete(const MemReq& req) override;

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
  RegAlloc Alloc;       ///< RZALSCoProc: Register allocator object

  /// Handle the incoming MZOP request
  bool handleMZOP(Forza::zopEvent *zev, bool &flag);

#define LOADQ_ZEV   0
#define LOADQ_RS2   1
  std::vector<std::pair<Forza::zopEvent *,unsigned>> LoadQ; ///< RZALSCoProc: Outstanding load queue

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
  virtual bool ClockTick(SST::Cycle_t cycle) override;

  /// RZAAMOCoProc: Enqueue a new instruction
  virtual bool IssueInst(RevFeature *F, RevRegFile *R,
                         RevMem *M, uint32_t Inst) override;

  /// RZAAMOCoProc: reset the coproc
  virtual bool Reset() override;

  /// RZAAMOCoProc: teardown function when the attached Proc is complete
  virtual bool Teardown() override { return Reset(); }

  /// RZAAMOCoProc: determines whether the coproc is complete
  virtual bool IsDone() override;

  /// RZAMOCoProc: injects a packet into the HZOP AMO pipeline
  virtual bool InjectZOP(Forza::zopEvent *zev, bool &flag) override;

  /// RZAAMOCoProc: marks a load as being complete
  virtual void MarkLoadComplete(const MemReq& req) override;

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
  RegAlloc Alloc;       ///< RZAAMOCoProc: Register allocator object

#define AMOQ_ZEV   0
#define AMOQ_RS1   1
#define AMOQ_RS2   2
  std::vector<std::tuple<Forza::zopEvent *,unsigned, unsigned>> AMOQ; ///< RZAAMOCoProc: Outstanding load queue

  std::function<void(const MemReq&)> MarkLoadCompleteFunc;  ///< RZAAMOCoProc: Hazard function

};  // RZAAMOCoProc
} //namespace SST::RevCPU

#endif
