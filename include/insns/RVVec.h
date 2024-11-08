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

  // local vector register file
  // The functions take a core register file pointer but we want to keep
  // the vector register file encapsulated with the coprocessor. A pointer
  // to this object is passed along with the core register file.
  // Crude but effective.
  // RevVRegFile* vregfile      = nullptr;

  // core register addresses
  uint64_t rd                   = 0;
  uint64_t rs1                  = 0;
  uint64_t rs2                  = 0;

  // TODO: We should model one pipeline stage to flopping the source registers.
  //       Now we reach down out of the clear blue sky and grab them
  uint64_t src1                 = 0xbadbeef;
  uint64_t src2                 = 0xbadbeef;
  bool     read_rs1             = false;
  bool     read_rs2             = false;

  RevVecInst() {};
  RevVecInst( uint32_t Inst );
  //RevVecInst( uint32_t Inst, RevVRegFile* vrf );
  void DecodeBase();
  void DecodeRVVTypeOp();
  void DecodeRVVTypeLd();
  void DecodeRVVTypeSt();
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
#if 1
    // this does 64 bits at a time. Overflow will not occur for this test
    V->vreg[2][0] = V->vreg[0][0] + V->vreg[1][0];
    V->vreg[2][1] = V->vreg[0][1] + V->vreg[1][1];
    // output->verbose(
    //   CALL_INFO,
    //   5,
    //   0,
    //   "*V"
    //   " 0x%" PRIx64 " <- v[0][0] 0x%" PRIx64 " <- v[0][1]"
    //   " 0x%" PRIx64 " <- v[1][0] 0x%" PRIx64 " <- v[1][1]"
    //   " v[2][0] <- 0x%" PRIx64 " v[2][1] <- 0x%" PRIx64 "\n",
    //   V->vreg[0][0],
    //   V->vreg[0][1],
    //   V->vreg[1][0],
    //   V->vreg[1][1],
    //   V->vreg[2][0],
    //   V->vreg[2][1]
#else
    std::cout << "vsetvl: no need to go further\n" << std::endl;
    assert( false );
#endif
    return true;
  }

  static bool vs( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t a3 = R->GetX<uint64_t>( RevReg::a3 );
    M->WriteMem( 0, a3, 8, &( V->vreg[2][0] ) );
    M->WriteMem( 0, a3 + 8, 8, &( V->vreg[2][1] ) );
    return true;
  }

  static bool vl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {

    uint64_t src = R->GetX<uint64_t>( Inst.rs1 );
    uint16_t vd  = Inst.rs1 == 11 ? 0 : 1;
    MemReq   req0( src, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
    M->ReadVal<uint64_t>( 0, src, &( V->vreg[vd][0] ), req0, RevFlag::F_NONE );
    MemReq req1( src + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
    M->ReadVal<uint64_t>( 0, src + 8, &( V->vreg[vd][1] ), req1, RevFlag::F_NONE );
    // output->verbose( CALL_INFO, 5, 0, "*V v[vd][0] <- 0x%" PRIx64 " v[vd][1] <- 0x%" PRIx64 "\n", vreg[vd][0], vreg[vd][1] );
    return true;
  }

  // clang-format off
  std::vector<RevVecInstEntry> RVVTable = {

  // Use lower 4 bits of Func3 to differentiate vector instructions
  //   For all OP* types, Func3 = inst[14:12]
  //   For LOAD-FP and STORE-FP, Func3 = 0x8

  // Func3=0x0: OPIVV
  RevVecInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs2, %vm" ).SetFunct3(0x0).SetImplFunc(&vadd    ).SetrdClass(RevRegClass::RegGPR)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111),

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
  // Note: RS2 will be for strided loads/stores
  RevVecInstDefaults().SetMnemonic("vle32.v %vd, (%rs1), %vm"  ).SetFunct3(0x8).SetImplFunc(&vl    ).Setrs1Class(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeLd).SetOpcode(0b0000111),
  RevVecInstDefaults().SetMnemonic("vse32.v %vs3, (%rs1), %vm" ).SetFunct3(0x8).SetImplFunc(&vs    ).Setrs1Class(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111),

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
