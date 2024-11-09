//
// _RVV_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
#ifndef _SST_REVCPU_RVVEC_H_
#define _SST_REVCPU_RVVEC_H_

#include "../RevExt.h"
#include "../RevInstHelpers.h"
#include "../RevVRegFile.h"

namespace SST::RevCPU {

//Note: I initially tried to extend the RevInstEntry struct but determined that
//the design pattern to cascade setters that return a this pointer prohibits subclassing.
//However, it might be advantageous to have a custom entry type encapsulated fully in the coprocessor.
//It provides more flexibility so I can pass in separate core and vector register files.

struct RevVecInstEntry;

// Decoded instruction fields and source data
struct RevVecInst {

  // pre-decode / construction
  uint32_t Inst                 = 0;
  uint64_t opcode               = 0;
  uint64_t funct3               = -1;
  uint64_t Enc                  = 0xff << 8;
  RevInstF format               = RevInstF::RVTypeUNKNOWN;

  // vector instruction table entry
  RevVecInstEntry* revInstEntry = nullptr;

  // core register addresses
  uint64_t rd                   = 0;
  uint64_t rs1                  = 0;
  uint64_t rs2                  = 0;

  // vector register addresses
  uint64_t vd                   = 0;
  uint64_t vs1                  = 0;
  uint64_t vs2                  = 0;
  uint64_t vs3                  = 0;

  // TODO: We should model one pipeline stage to flopping the source registers.
  //       Now we reach down out of the clear blue sky and grab them
  uint64_t src1                 = 0xbadbeef;
  uint64_t src2                 = 0xbadbeef;
  bool     read_rs1             = false;
  bool     read_rs2             = false;
  bool     read_rs3             = false;

  RevVecInst() {};
  RevVecInst( uint32_t Inst );
  //RevVecInst( uint32_t Inst, RevVRegFile* vrf );
  void DecodeBase();
  void DecodeRVVTypeOp();
  void DecodeRVVTypeLd();
  void DecodeRVVTypeSt();

  // Vector Extension
  //void ReadScalars( RevRegFile* R );
};

struct RevVecInstEntry {
  // TODO prune this
  // disassembly
  std::string mnemonic = "nop";  ///< RevVecInstEntry: instruction mnemonic
  uint32_t    cost     = 1;      ///< RevVecInstEntry: instruction code in cycles

  // storage
  uint8_t  opcode      = 0;  ///< RevVecInstEntry: opcode
  uint8_t  funct2      = 0;  ///< RevVecInstEntry: compressed funct2 value
  uint8_t  funct3      = 0;  ///< RevVecInstEntry: funct3 value
  uint8_t  funct4      = 0;  ///< RevVecInstEntry: compressed funct4 value
  uint8_t  funct6      = 0;  ///< RevVecInstEntry: compressed funct6 value
  uint8_t  funct2or7   = 0;  ///< RevVecInstEntry: uncompressed funct2 or funct7 value
  uint16_t offset      = 0;  ///< RevVecInstEntry: compressed offset value
  uint16_t jumpTarget  = 0;  ///< RevVecInstEntry: compressed jump target value

  // register encodings
  // overlay vector registers vd, vs1, vs2, vs3 onto rd, vs1, vs2, vs3, respectively
  RevRegClass rdClass  = RevRegClass::RegGPR;      ///< RevVecInstEntry: Rd register class
  RevRegClass rs1Class = RevRegClass::RegGPR;      ///< RevVecInstEntry: Rs1 register class
  RevRegClass rs2Class = RevRegClass::RegGPR;      ///< RevVecInstEntry: Rs2 register class
  RevRegClass rs3Class = RevRegClass::RegUNKNOWN;  ///< RevVecInstEntry: Rs3 register class
  uint16_t    imm12    = 0;                        ///< RevVecInstEntry: imm12 value
  RevImmFunc  imm      = RevImmFunc::FUnk;         ///< RevVecInstEntry: does the imm12 exist?

  // formatting
  RevInstF format      = RVTypeR;  ///< RevVecInstEntry: instruction format
  bool     compressed  = false;    ///< RevVecInstEntry: compressed instruction
  uint8_t  rs2fcvtOp   = 0;        ///< RevVecInstEntry: Stores the rs2 field in R-instructions
  bool     raisefpe    = false;    ///< RevVecInstEntry: Whether FP exceptions are raised

  /// Instruction implementation function
  bool ( *func )( const RevFeature*, RevRegFile*, RevVRegFile*, RevMem*, const RevVecInst& ){};

  /// Predicate for enabling table entries for only certain encodings
  bool ( *predicate )( uint32_t Inst ) = []( uint32_t ) { return true; };

  // Begin Set() functions to allow call chaining - all Set() must return *this
  // clang-format off
  auto& SetMnemonic(std::string m)   { this->mnemonic   = std::move(m); return *this; }
  auto& SetCost(uint32_t c)          { this->cost       = c;     return *this; }
  auto& SetOpcode(uint8_t op)        { this->opcode     = op;    return *this; }
  auto& SetFunct2(uint8_t f2)        { this->funct2     = f2;    return *this; }
  auto& SetFunct3(uint8_t f3)        { this->funct3     = f3;    return *this; }
  auto& SetFunct4(uint8_t f4)        { this->funct4     = f4;    return *this; }
  auto& SetFunct6(uint8_t f6)        { this->funct6     = f6;    return *this; }
  auto& SetFunct2or7(uint8_t f27)    { this->funct2or7  = f27;   return *this; }
  auto& SetOffset(uint16_t off)      { this->offset     = off;   return *this; }
  auto& SetJumpTarget(uint16_t jt)   { this->jumpTarget = jt;    return *this; }
  auto& SetrdClass(RevRegClass rd)   { this->rdClass    = rd;    return *this; }
  auto& Setrs1Class(RevRegClass rs1) { this->rs1Class   = rs1;   return *this; }
  auto& Setrs2Class(RevRegClass rs2) { this->rs2Class   = rs2;   return *this; }
  auto& Setrs3Class(RevRegClass rs3) { this->rs3Class   = rs3;   return *this; }
  auto& Setimm12(uint16_t imm12)     { this->imm12      = imm12; return *this; }
  auto& Setimm(RevImmFunc imm)       { this->imm        = imm;   return *this; }
  auto& SetFormat(RevInstF format)   { this->format     = format;return *this; }
  auto& SetCompressed(bool c)        { this->compressed = c;     return *this; }
  auto& Setrs2fcvtOp(uint8_t op)     { this->rs2fcvtOp  = op;    return *this; }
  auto& SetRaiseFPE(bool c)          { this->raisefpe   = c;     return *this; }
  auto& SetImplFunc( bool func( const RevFeature *, RevRegFile *, RevVRegFile *, RevMem *, const RevVecInst& ) )
                                     { this->func       = func;  return *this; }
  auto& SetPredicate( bool pred( uint32_t ) )
                                     { this->predicate  = pred;  return *this; }

  // clang-format on
};  // RevVecInstEntry

// The default initialization for RevVecInstDefaults is the same as RevVecInstEntry
using RevVecInstDefaults = RevVecInstEntry;

class RVVec : public RevExt {

public:
  // Because we are using RevVecInstEntry instead of RevInstEntry we need to define a new table
  std::vector<RevVecInstEntry> VecTable{};  ///< RVVec: vector instruction table

  void SetVecTable( std::vector<RevVecInstEntry>&& InstVect ) { VecTable = std::move( InstVect ); }

  const std::vector<RevVecInstEntry>& GetVecTable() const { return VecTable; }

  /**
   * Vector Instruction Implementations
   */

  static bool vsetivli( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    std::cout << "vsetivli: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vsetvli( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    R->SetX( RevReg::t0, 0x4 );
    return true;
  }

  static bool vsetvl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    std::cout << "vsetvl: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vadd( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    //TODO this only does 64 bits at a time. Overflow will not occur for this test so using simple adds
    V->vreg[Inst.vd][0] = V->vreg[Inst.vs1][0] + V->vreg[Inst.vs2][0];
    V->vreg[Inst.vd][1] = V->vreg[Inst.vs1][1] + V->vreg[Inst.vs2][1];
    return true;
  }

  static bool vs( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    M->WriteMem( 0, addr, 8, &( V->vreg[Inst.vs3][0] ) );
    M->WriteMem( 0, addr + 8, 8, &( V->vreg[Inst.vs3][1] ) );
    return true;
  }

  static bool vl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    MemReq   req0( addr, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
    M->ReadVal<uint64_t>( 0, addr, &( V->vreg[Inst.vd][0] ), req0, RevFlag::F_NONE );
    MemReq req1( addr + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
    M->ReadVal<uint64_t>( 0, addr + 8, &( V->vreg[Inst.vd][1] ), req1, RevFlag::F_NONE );
    return true;
  }

  /**
   * Vector Instruction Table
   */

  // clang-format off
  std::vector<RevVecInstEntry> RVVTable = {

  // Use lower 4 bits of Func3 to differentiate vector instructions
  //   For all OP* types, Func3 = inst[14:12]
  //   For LOAD-FP and STORE-FP, Func3 = 0x8

  // Func3=0x0: OPIVV
  // func6   vm    vs2    vs1  0  vd   b1010111
  //   6      1     5      5   3   5     6
  RevVecInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs2, %vm" ).SetFunct3(0x0).SetImplFunc(&vadd).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC).Setrs2Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111),

  // Func3=0x1: OPFVV
  // Func3=0x2: OPMVV
  // Func3=0x3: OPIVI
  // Func3=0x4: OPIVX
  // Func3=0x5: OPFVF
  // Func3=0x6: OPMVX

  // Func3=0x7: Formats for Vector Configuration Instructions under OP-V major opcode.
  RevVecInstDefaults().SetMnemonic("vsetvli %rd, %rs1, %zimm11"   ).SetFunct3(0x7).SetImplFunc(&vsetvli ).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 31) & 0x1)  == 0; } ),
  RevVecInstDefaults().SetMnemonic("vsetivli %rd, %zimm, %zimm10" ).SetFunct3(0x7).SetImplFunc(&vsetivli).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 30) & 0x3)  == 0xb11; } ),
  RevVecInstDefaults().SetMnemonic("vsetvl %rd, %rs1, %rs2"       ).SetFunct3(0x7).SetImplFunc(&vsetvl  ).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegGPR)      .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 25) & 0x7f) == 0xb1000000; } ),

  // Func3=0x8: Vector Loads and Stores
  // Note: Func3 is synthesized to differentiate it from OP types


  // VL* Unit Stride
  // nf   mew  mop  vm  lumop  rs1   width  vd  b0000111
  //  3    1    2    1    5     5     3      5     7
  RevVecInstDefaults().SetMnemonic("vl*.v %vd, (%rs1), %vm"  ).SetFunct3(0x8).SetImplFunc(&vl).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN)   .SetFormat(RVVTypeLd).SetOpcode(0b0000111),

  // VS* Unit Stride
  // nf   mew  mop  vm  sumop  rs1   width  vs3  b0100111
  //  3    1    2    1    5     5     3      5     7
  RevVecInstDefaults().SetMnemonic("vs*.v %vs3, (%rs1), %vm" ).SetFunct3(0x8).SetImplFunc(&vs).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN).Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111),

  };
  // clang-format on

public:
  /// RVVec: standard constructor
  RVVec( const RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) : RevExt( "RVV", Feature, RevMem, Output ) {
    SetVecTable( std::move( RVVTable ) );
  }

};  //class RVVec

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_RVVEC_H_
