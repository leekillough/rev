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

  // vector load/store
  uint64_t umop                 = 0;
  uint64_t mop                  = 0;

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
  // TODO prune this or break out base class common to core and coprocessor
  // disassembly
  std::string mnemonic = "nop";  ///< RevVecInstEntry: instruction mnemonic
  uint32_t    cost     = 1;      ///< RevVecInstEntry: instruction code in cycles

  // storage
  uint8_t opcode       = 0;  ///< RevVecInstEntry: opcode
  // Vector Load/Store ops
  uint8_t mop          = 0;  ///< RevVecInstEntry: mop
  uint8_t umop         = 0;  ///< RevVecInstEntry: lumop or sumop

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

  // TODO: Handling of core registers should be based on access to scalars in the instruction queue
  // Should not be passing in the core register file here.
  /// Instruction implementation function
  bool ( *func )( const RevFeature*, RevRegFile*, RevVRegFile*, RevMem*, const RevVecInst& ){};

  /// Predicate for enabling table entries for only certain encodings
  bool ( *predicate )( uint32_t Inst ) = []( uint32_t ) { return true; };

  // Begin Set() functions to allow call chaining - all Set() must return *this
  // clang-format off
  auto& SetMnemonic(std::string m)   { this->mnemonic   = std::move(m); return *this; }
  auto& SetCost(uint32_t c)          { this->cost       = c;     return *this; }
  auto& SetOpcode(uint8_t op)        { this->opcode     = op;    return *this; }

  // Vector Load and Store ops: MOP[27:26], LUMOP/SUMOP[25:20]
  auto& SetMop(uint8_t mop)          { this->mop        = mop;   return *this; }
  auto& SetUmop(uint8_t umop)        { this->umop       = umop;  return *this; }

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

  // Use lower 4 bits of Funct3 to differentiate vector instructions
  // For all OP* types, Funct3 = inst[14:12]
  enum OPV : uint8_t {
    IVV = 0x0,
    FVV = 0x1,
    MVV = 0x2,
    IVI = 0x3,
    IVX = 0x4,
    FVF = 0x5,
    MVX = 0x6,
    CFG = 0x7,
  };

  // For LD/ST Ops: when mew=0, Funct3 selects width. 0=8b, 5=16b, 6=32b, 7=64b
  enum MSZ : uint8_t {
    u8  = 0x0,
    u16 = 0x5,
    u32 = 0x6,
    u64 = 0x7,
  };

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
    // Set vtype and configure vector execution state
    V->SetVCSR( RevCSR::vtype, newvtype );
    V->Configure( newvtype );
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

#if 1
    std::cout << "*V vl: 0x" << std::hex << newvl << std::endl;
#endif

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

  // Immediate, Scalar, Vector
  enum VecOpKind { Imm, Reg, VReg };

  template<typename SEWTYPE, template<class> class VOP, VecOpKind KIND, bool DST_IS_MASK>
  static bool vec_int_op( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd   = Inst.vd;
    uint64_t vs1  = Inst.vs1;
    uint64_t vs2  = Inst.vs2;
    uint64_t mask = 0;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el  = vecElem % V->ElemsPerReg();
      SEWTYPE  res = 0;
      switch( KIND ) {
      case VReg: res = VOP()( V->GetElem<SEWTYPE>( vs2, el ), V->GetElem<SEWTYPE>( vs1, el ) ); break;
      case Imm: res = VOP()( V->GetElem<SEWTYPE>( vs2, el ), ( Inst.Inst >> 15 ) & 0x1f ); break;
      case Reg: res = VOP()( V->GetElem<SEWTYPE>( vs2, el ), R->GetX<SEWTYPE>( Inst.rs1 ) ); break;
      }
      if( DST_IS_MASK ) {
        assert( vecElem < 64 );  // TODO
        //std::cout << "*V " << vecElem << " " << std::hex <<  V->GetElem<SEWTYPE>( vs2, el ) << std::endl;
        mask |= ( res & 1 ) << vecElem;
      } else {
        V->SetElem<SEWTYPE>( vd, el, res );
        std::cout << "*V v" << std::dec << vd << "." << el << " <- 0x" << std::hex << (uint64_t) res << std::endl;
      }
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 ) {
        vd++;
        vs1++;
        vs2++;
      }
    }
    if( DST_IS_MASK ) {
      V->SetMaskReg( Inst.vd, mask );
    }

    return true;
  }

  /// Vector Integer Operation template
  template<template<class> class VOP, VecOpKind KIND, bool DST_IS_MASK>
  static bool vec_oper( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == vsew::e64 )
      return vec_int_op<uint64_t, VOP, KIND, DST_IS_MASK>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e32 )
      return vec_int_op<uint32_t, VOP, KIND, DST_IS_MASK>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e16 )
      return vec_int_op<uint16_t, VOP, KIND, DST_IS_MASK>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e8 )
      return vec_int_op<uint8_t, VOP, KIND, DST_IS_MASK>( F, R, V, M, Inst );
    else {
      assert( false );
    }
  }

  // Arithmetic operators
  static constexpr auto& vaddvv  = vec_oper<std::plus, VecOpKind::VReg, false>;
  static constexpr auto& vaddvi  = vec_oper<std::plus, VecOpKind::Imm, false>;
  static constexpr auto& vaddvx  = vec_oper<std::plus, VecOpKind::Reg, false>;
  static constexpr auto& vsubvv  = vec_oper<std::minus, VecOpKind::VReg, false>;
  static constexpr auto& vsubvi  = vec_oper<std::minus, VecOpKind::Imm, false>;
  static constexpr auto& vsubvx  = vec_oper<std::minus, VecOpKind::Reg, false>;
  // Comparisons
  static constexpr auto& vmseqvv = vec_oper<std::equal_to, VecOpKind::VReg, true>;
  static constexpr auto& vmseqvi = vec_oper<std::equal_to, VecOpKind::Imm, true>;
  static constexpr auto& vmseqvx = vec_oper<std::equal_to, VecOpKind::Reg, true>;
  static constexpr auto& vmsnevv = vec_oper<std::not_equal_to, VecOpKind::VReg, true>;
  static constexpr auto& vmsnevi = vec_oper<std::not_equal_to, VecOpKind::Imm, true>;
  static constexpr auto& vmsnevx = vec_oper<std::not_equal_to, VecOpKind::Reg, true>;

  template<typename SEWTYPE>
  static SEWTYPE ElemIntOp( uint8_t funct6, SEWTYPE s1, SEWTYPE s2 ) {
    switch( funct6 ) {
    case( 0b000000 ): return s2 + s1;           // vadd
    case( 0b000010 ): return s2 - s1;           // vsub
    case( 0b011000 ): return s2 == s1 ? 1 : 0;  // vmseq // TODO these write to the vector mask bit
    case( 0b011001 ): return s2 != s1 ? 1 : 0;  // vmsne
    default: assert( false );
    }
    assert( false );
    return 0;
  }

  template<typename SEWTYPE>
  static SEWTYPE ElemMskOp( uint8_t funct6, SEWTYPE s1, SEWTYPE s2 ) {
    switch( funct6 ) {
    case( 0b011010 ): return s2 | s1;  // vmor
    default: assert( false );
    }
    assert( false );
    return 0;
  }

  /// Old Vector Integer Operation template
  template<typename SEWTYPE>
  static bool VecIntOp_OLD( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd     = Inst.vd;
    uint64_t vs1    = Inst.vs1;
    uint64_t vs2    = Inst.vs2;
    uint64_t uimm5  = ( Inst.Inst >> 15 ) & 0x1f;
    uint8_t  funct6 = ( Inst.Inst >> 26 ) & 0x3f;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el  = vecElem % V->ElemsPerReg();
      SEWTYPE  res = 0;
      switch( Inst.funct3 ) {
      case OPV::IVV: res = ElemIntOp( funct6, V->GetElem<SEWTYPE>( vs2, el ), V->GetElem<SEWTYPE>( vs1, el ) ); break;
      case OPV::IVI: res = ElemIntOp( funct6, V->GetElem<SEWTYPE>( vs2, el ), (SEWTYPE) uimm5 ); break;
      case OPV::IVX: res = ElemIntOp( funct6, V->GetElem<SEWTYPE>( vs2, el ), R->GetX<SEWTYPE>( Inst.rs1 ) ); break;
      case OPV::MVV: res = ElemMskOp( funct6, V->GetElem<SEWTYPE>( vs2, el ), V->GetElem<SEWTYPE>( vs1, el ) ); break;
      default: V->Output()->fatal( CALL_INFO, -1, "Vector arithmetic func3 %" PRIu64 " not supported\n", Inst.funct3 );
      }
      // if (destIsMaskReg) {
      //   mskreg  |= ((res & 1) << mskpos++);
      // } else {
      V->SetElem<SEWTYPE>( vd, el, res );
      std::cout << "*V v" << std::dec << vd << "." << el << " <- 0x" << std::hex << (uint64_t) res << std::endl;
      // }
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 ) {
        vd++;
        vs1++;
        vs2++;
      }
    }
    return true;
  }

  static bool vintop_old( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == vsew::e64 )
      return VecIntOp_OLD<uint64_t>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e32 )
      return VecIntOp_OLD<uint32_t>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e16 )
      return VecIntOp_OLD<uint16_t>( F, R, V, M, Inst );
    else if( vt.f.vsew == vsew::e8 )
      return VecIntOp_OLD<uint8_t>( F, R, V, M, Inst );
    else {
      assert( false );
    }
  }

  static bool vmor_mm( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    return vintop_old( F, R, V, M, Inst );
  }

  // VS*
  // 31 29  28  27 26  25  24 20  19 15  14 12  11 7   6     0
  //   nf  mew   mop   vm  lumop   rs1   width   vd    b0000111
  //
  // mop
  //  00 - unit stride
  //  01 - indexed-unordered
  //  10 - strided
  //  11 - index-ordered
  //
  // sumop
  // 0 0 0 0 0 unit-stride
  // 0 1 0 0 0 unit-stride, whole register load
  // 0 1 0 1 1 unit-stride, mask load, EEW=8
  // others reserved

  template<typename SEWTYPE>
  static bool vs( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t vs3  = Inst.vs3;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el = vecElem % V->ElemsPerReg();
      SEWTYPE  d  = V->GetElem<SEWTYPE>( vs3, el );
      M->WriteMem( 0, addr, sizeof( SEWTYPE ), &d );
      std::cout << "*V M[0x" << std::hex << addr << "] <- 0x" << std::hex << (uint64_t) d << " <- v" << std::dec << vs3 << "."
                << std::dec << el << std::endl;
      addr += sizeof( SEWTYPE );
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 )
        vs3++;
    }
    return true;
  }

  // VL*
  // 31 29  28  27 26  25  24 20  19 15  14 12  11 7   6     0
  //   nf  mew   mop   vm  lumop   rs1   width   vd    b0000111
  //
  // mop
  //  00 - unit stride
  //  01 - indexed-unordered
  //  10 - strided
  //  11 - index-ordered
  //
  // lumop
  // 0 0 0 0 0 unit-stride
  // 0 1 0 0 0 unit-stride, whole register
  // 0 1 0 1 1 unit-stride, masked, EEW=8
  // 1 0 0 0 0 unit-stride fault-only-first
  // others reserved

  template<typename SEWTYPE>
  static bool _vl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t addr = R->GetX<uint64_t>( Inst.rs1 );
    uint64_t vd   = Inst.vd;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el = vecElem % V->ElemsPerReg();
      SEWTYPE  d  = 0;
      MemReq   req0( addr, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
      M->ReadVal<SEWTYPE>( 0, addr, &d, req0, RevFlag::F_NONE );
      std::stringstream s;
      s << "0x" << std::hex << (uint64_t) d << " <- M[0x" << addr << "]";
      V->SetElem<SEWTYPE>( vd, el, d );
      std::cout << "*V v" << std::dec << vd << "." << el << " <- 0x" << std::hex << (uint64_t) d << " " << s.str() << std::endl;
      addr += sizeof( SEWTYPE );
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 )
        vd++;
    }
    return true;
  }

  // load unit stride
  template<typename SEWTYPE>
  static bool vl( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    return _vl<SEWTYPE>( F, R, V, M, Inst );
  }

  // load unit stride fault-only first
  template<typename SEWTYPE>
  static bool vlff( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    // TODO fault-only first behavior
    V->Output()->verbose(
      CALL_INFO, 1, 0, "Warning: vector load fault-only first not implemented and will behave has a normal vector load\n"
    );
    return _vl<SEWTYPE>( F, R, V, M, Inst );
  }

  /**
   * Vector Instruction Table
   */

  //TODO we need Funct6 and all the mnemonics spelled out here
  // clang-format off
  std::vector<RevVecInstEntry> RVVTable = {

  // Vector Arithmetic OPVIVV, OPVIVI, OPVIVX
  RevVecInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b000000).SetImplFunc(&vaddvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vadd.vi %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVI).SetFunct6(0b000000).SetImplFunc(&vaddvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vadd.vx %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b000000).SetImplFunc(&vaddvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),

  RevVecInstDefaults().SetMnemonic("vsub.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b000010).SetImplFunc(&vsubvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vsub.vi %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVI).SetFunct6(0b000010).SetImplFunc(&vsubvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vsub.vx %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b000010).SetImplFunc(&vsubvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),

  RevVecInstDefaults().SetMnemonic("vmseq.vv %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVV).SetFunct6(0b011000).SetImplFunc(&vmseqvv).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmseq.vi %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVI).SetFunct6(0b011000).SetImplFunc(&vmseqvi).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmseq.vx %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVX).SetFunct6(0b011000).SetImplFunc(&vmseqvx).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),

  RevVecInstDefaults().SetMnemonic("vmsne.vv %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVV).SetFunct6(0b011001).SetImplFunc(&vmsnevv).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsne.vi %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVI).SetFunct6(0b011001).SetImplFunc(&vmsnevi).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsne.vx %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVX).SetFunct6(0b011001).SetImplFunc(&vmsnevx).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),

  // Vector Mask Inst  OPMVV: Funct3=0x2
  RevVecInstDefaults().SetMnemonic("vmor.mm %vd, %vs2, %vs1"       ).SetFunct3(OPV::MVV).SetFunct6(0b011010).SetImplFunc(&vmor_mm).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
// Vector Config     OPV:   Funct3=0x7
  RevVecInstDefaults().SetMnemonic("vsetvli %rd, %rs1, %zimm11"    ).SetFunct3(OPV::CFG).SetImplFunc(&vsetvli       ).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)      .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 31) & 0x1)  == 0x00; } ),
  RevVecInstDefaults().SetMnemonic("vsetivli %rd, %zimm, %zimm10"  ).SetFunct3(OPV::CFG).SetImplFunc(&vsetivli      ).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegUNKNOWN)      .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 30) & 0x3)  == 0x03; } ),
  RevVecInstDefaults().SetMnemonic("vsetvl %rd, %rs1, %rs2"        ).SetFunct3(OPV::CFG).SetImplFunc(&vsetvl        ).SetrdClass(RevRegClass::RegGPR    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegGPR    )      .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 25) & 0x7f) == 0x40; } ),

  // Unit Stride Load
  RevVecInstDefaults().SetMnemonic("vle64.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u64).SetImplFunc(&vl<uint64_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle32.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u32).SetImplFunc(&vl<uint32_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle16.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u16).SetImplFunc(&vl<uint16_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle8.v %vd, (%rs1), %vm"       ).SetFunct3(MSZ::u8 ).SetImplFunc(&vl<uint8_t>   ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),

  // Unit Stride Store
  RevVecInstDefaults().SetMnemonic("vse64.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u64).SetImplFunc(&vs<uint64_t>  ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse32.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u32).SetImplFunc(&vs<uint32_t>  ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse16.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u16).SetImplFunc(&vs<uint16_t>  ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse8.v %vs3, (%rs1), %vm "     ).SetFunct3(MSZ::u8 ).SetImplFunc(&vs<uint8_t>   ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),

  // Unit Stride Load Fault-only first
  RevVecInstDefaults().SetMnemonic("vle64ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u64).SetImplFunc(&vlff<uint64_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle32ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u32).SetImplFunc(&vlff<uint32_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle16ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u16).SetImplFunc(&vlff<uint16_t>  ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle8ff.v %vd, (%rs1), %vm"     ).SetFunct3(MSZ::u8 ).SetImplFunc(&vlff<uint8_t>   ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),

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
