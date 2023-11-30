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
// ----------------------------------------
// RegAlloc
// ----------------------------------------
class RegAlloc{
#define _H_CLEAR    0
#define _H_READ     1
#define _H_WRITE    2
#define _H_DISABLE  3
#define _UNDEF_REG  33
public:
  /// RegAlloc: constructor
  RegAlloc(){
    hazard[0] = _H_DISABLE;
    for( unsigned i=1; i<32; i++ ){
      hazard[i] = _H_CLEAR;
    }
  }

  /// RegAlloc: destructor
  ~RegAlloc(){}

  /// RegAlloc: retrieve a two operand register set
  bool getRegs(unsigned &rs1, unsigned &rs2){
    unsigned t_rs1 = 33;
    unsigned t_rs2 = 33;

    // find two register slots
    unsigned cur = 1;
    while( cur < 32 ){
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_READ;
        t_rs1 = cur;
        break;
      }else{
        cur++;
        if( cur == 32 ){
          return false;
        }
      }
    }

    cur = 1;
    while( cur < 32 ){
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_READ;
        t_rs2 = cur;
        break;
      }else{
        cur++;
        if( cur == 32 ){
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
    unsigned t_rd = 33;
    unsigned t_rs1 = 33;
    unsigned t_rs2 = 33;

    // find three register slots
    // -- Rd
    unsigned cur = 1;
    while( t_rd == 33 ){
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_WRITE;
        t_rd = cur;
      }else{
        cur++;
        if( t_rd == 32 ){
          return false;
        }
      }
    }

    // -- Rs1
    cur = 1;
    while( t_rs1 == 33 ){
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_READ;
        t_rs1 = cur;
      }else{
        cur++;
        if( t_rs1 == 32 ){
          return false;
        }
      }
    }

    // -- Rs2
    cur = 1;
    while( t_rs2 == 33 ){
      if( hazard[cur] == _H_CLEAR ){
        hazard[cur] = _H_READ;
        t_rs2 = cur;
      }else{
        cur++;
        if( t_rs2 == 32 ){
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
    if( reg != 0 ){
      hazard[reg] = _H_CLEAR;
    }
  }

private:
  uint8_t hazard[32];
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
} //namespace SST::RevCPU

#endif
