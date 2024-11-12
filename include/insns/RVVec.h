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

  static uint16_t CalculateVLMAX( RevVRegFile* V ) {
    reg_vtype_t _vtype( V->GetVCSR( RevCSR::vtype ) );
    uint8_t     sew = 0;
    switch( _vtype.f.vsew ) {
    case 0: sew = 8; break;
    case 1: sew = 16; break;
    case 2: sew = 32; break;
    case 3: sew = 64; break;
    default: assert( false ); break;
    }
    uint16_t vlmax = 0;
    uint64_t lenb  = V->GetVCSR( RevCSR::vlenb );
    switch( _vtype.f.vlmul ) {
    case 0: vlmax = ( lenb * 8 ) / sew; break;
    case 1: vlmax = 2 * ( ( lenb * 8 ) / sew ); break;
    case 2: vlmax = 4 * ( ( lenb * 8 ) / sew ); break;
    case 3: vlmax = 8 * ( ( lenb * 8 ) / sew ); break;
    case 5: vlmax = ( ( lenb * 8 ) / sew ) / 8; break;
    case 6: vlmax = ( ( lenb * 8 ) / sew ) / 4; break;
    case 7: vlmax = ( ( lenb * 8 ) / sew ) / 2; break;
    default: assert( false ); break;
    }
    return vlmax;
  }

  static bool _vsetvl( uint16_t avl, uint64_t newvtype, RevRegFile* R, RevVRegFile* V, const RevVecInst& Inst ) {
    // Configure the vtype csr
    reg_vtype_t vt = newvtype & 0xffULL;
    V->SetVCSR( RevCSR::vtype, vt.v );
    // Configure the vl csr
    uint64_t newvl = 0;
    if( avl != 0 ) {
      uint16_t vlmax = RVVec::CalculateVLMAX( V );
      if( avl <= vlmax ) {
        newvl = avl;
      } else if( avl < ( 2 * vlmax ) ) {
        newvl = ceil( avl / 2 );
      } else {
        newvl = vlmax;
      }
    } else if( ( Inst.rd != 0 ) && ( avl == 0 ) ) {
      newvl = RVVec::CalculateVLMAX( V );
    } else if( ( Inst.rd == 0 ) && ( avl == 0 ) ) {
      newvl = V->GetVCSR( RevCSR::vl );
    }
    V->SetVCSR( RevCSR::vl, newvl );
    R->SetX( Inst.rd, newvl );

    // Configure vector register file based on new vtype
    V->Configure( vt );
    return true;
  }

  // 31 30        20  19 15 14 12 11 7 6       0
  // 0  vtypei[10:0]   rs1  0b111  rd  0b1010111
  static bool vsetvli( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint16_t avl      = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t newvtype = ( Inst.Inst >> 20 ) & 0x7ff;
    _vsetvl( avl, newvtype, R, V, Inst );
    return true;
  }

  // vsetivli
  //   31 30 29       20   19     15 14  12 11 7 6      0
  //   0b11   vtypei[9:0]  uimm[4:0]  0b111  rd  0b1010111
  static bool vsetivli( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint16_t avl      = ( Inst.Inst >> 15 ) & 0x1f;
    uint64_t newvtype = ( Inst.Inst >> 20 ) & 0x3ff;
    _vsetvl( avl, newvtype, R, V, Inst );
    return true;
  }

  // vsetvl
  // 1  0b000000  rs2  rs1  0b111    rd  0b1010111
  // 1     6       5    5      3      5      7
  static bool vsetvl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint16_t avl      = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t newvtype = R->GetX<uint64_t>( Inst.rs2 );
    _vsetvl( avl, newvtype, R, V, Inst );
    return true;
  }

  // Func3=0x0: OPIVV
  // 31  26  25   24 20  19 15 14 12 11 7  6     0
  // func6   vm    vs2    vs1   0    vd   b1010111
  static bool vadd( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd  = Inst.vd;
    uint64_t vs1 = Inst.vs1;
    uint64_t vs2 = Inst.vs2;
    for( unsigned i = 0; i < V->Iters(); i++ ) {
      for( unsigned e = 0; e < V->ElemsPerReg(); e++ ) {
        uint64_t res = V->GetElem( vs1, e ) + V->GetElem( vs2, e );
        V->SetElem( vd, e, res );
      }
      vd++;
      vs1++;
      vs2++;
    }
    return true;
  }

  // VS* Unit Stride
  // 31 29  28  27 26 25 24 20  19 15 14 12  11 7   6     0
  // nf     mew  mop  vm  sumop  rs1   width  vs3  b0100111
  //  3      1    2    1    5     5     3      5     7
  static bool vs( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t vs3  = Inst.vs3;
    for( unsigned i = 0; i < V->Iters(); i++ ) {
      for( unsigned e = 0; e < V->ElemsPerReg(); e++ ) {
        unsigned bytes = V->ElemBytes( e );  // for fractional LMUL
        if( bytes ) {
          uint64_t d = V->GetElem( vs3, e );
          M->WriteMem( 0, addr, bytes, &d );
          std::cout << "*V M[0x" << std::hex << addr << "] <- 0x" << std::hex << d << " <- v" << std::dec << vs3 << "." << std::dec
                    << d << std::endl;
        }
        addr += sizeof( uint64_t );
      }
      vs3++;
    }
    return true;
  }

  // VL* Unit Stride
  // nf   mew  mop  vm  lumop  rs1   width  vd  b0000111
  //  3    1    2    1    5     5     3      5     7
  static bool vl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t vd   = Inst.vd;
    for( unsigned i = 0; i < V->Iters(); i++ ) {
      for( unsigned e = 0; e < V->ElemsPerReg(); e++ ) {
        uint64_t d = 0;
        MemReq   req0( addr, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
        M->ReadVal<uint64_t>( 0, addr, &d, req0, RevFlag::F_NONE );
        V->SetElem( vd, e, d );
        std::cout << "*V v" << std::dec << vd << "." << e << " <- 0x" << std::hex << d << " <- M[0x" << addr << "]" << std::endl;
        addr += sizeof( uint64_t );
      }
      vd++;
    }
    return true;
  }

  /**
   * Vector Instruction Table
   */

  // clang-format off
  std::vector<RevVecInstEntry> RVVTable = {

  // Use lower 4 bits of Func3 to differentiate vector instructions
  //   For all OP* types, Func3 = inst[14:12]
  // Func3=0x1: OPFVV
  // Func3=0x2: OPMVV
  // Func3=0x3: OPIVI
  // Func3=0x4: OPIVX
  // Func3=0x5: OPFVF
  // Func3=0x6: OPMVX
  // Func3=0x7: Formats for Vector Configuration Instructions under OP-V major opcode.
  // Func3=0x8: Synthesized here to differentate LOAD-FP and STORE-FP,
  RevVecInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs2, %vm" ).SetFunct3(0x0).SetImplFunc(&vadd    ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC    )                                     .SetFormat(RVVTypeOp).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vsetvli %rd, %rs1, %zimm11"   ).SetFunct3(0x7).SetImplFunc(&vsetvli ).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 31) & 0x1)  == 0x00; } ),
  RevVecInstDefaults().SetMnemonic("vsetivli %rd, %zimm, %zimm10" ).SetFunct3(0x7).SetImplFunc(&vsetivli).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 30) & 0x3)  == 0x03; } ),
  RevVecInstDefaults().SetMnemonic("vsetvl %rd, %rs1, %rs2"       ).SetFunct3(0x7).SetImplFunc(&vsetvl  ).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegGPR    )                                     .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 25) & 0x7f) == 0x40; } ),
  RevVecInstDefaults().SetMnemonic("vl*.v %vd, (%rs1), %vm"       ).SetFunct3(0x8).SetImplFunc(&vl      ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111),
  RevVecInstDefaults().SetMnemonic("vs*.v %vs3, (%rs1), %vm"      ).SetFunct3(0x8).SetImplFunc(&vs      ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111),
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
