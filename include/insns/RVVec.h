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

  // mask bit
  bool vm = false;

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
    
    uint16_t vlmax = RVVec::CalculateVLMAX( V );
    V->Configure( newvtype, vlmax );
    // Configure the vl csr
    uint64_t newvl = 0;
    if( avl != 0 ) {
      if( avl <= vlmax ) {
        newvl = avl;
      } else if( avl < ( 2 * vlmax ) ) {
        newvl = ceil( avl / 2 );
      } else {
        newvl = vlmax;
      }
    } else if( ( Inst.rd != 0 ) && ( avl == 0 ) ) {
      newvl = vlmax;
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

  // Immediate, Scalar, Vector?
  enum VecOpKind { Imm, Reg, VReg };

  // Vector load/store flavors
  enum VecMemOpKind { UnitStride, UnitStrideMask, Strided, Indexed, FaultOnlyFirst, Segment, WholeRegister };

  // Mask op modifier
  enum MaskOpMod { None, Not, NotVS1 };

  /// Vector Integer Operator Entry Template
  template<template<class> class VOP, VecOpKind KIND, bool DST_IS_MASK = false, bool FORCE_VS2_ZERO = false, bool REVERSE = false>
  static bool vec_oper( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == sew_t::e64 )
      return vec_oper_exec<uint64_t, VOP, KIND, DST_IS_MASK, FORCE_VS2_ZERO, REVERSE>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e32 )
      return vec_oper_exec<uint32_t, VOP, KIND, DST_IS_MASK, FORCE_VS2_ZERO, REVERSE>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e16 )
      return vec_oper_exec<uint16_t, VOP, KIND, DST_IS_MASK, FORCE_VS2_ZERO, REVERSE>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e8 )
      return vec_oper_exec<uint8_t, VOP, KIND, DST_IS_MASK, FORCE_VS2_ZERO,  REVERSE>( F, R, V, M, Inst );
    else {
      assert( false );
    }
    return false;
  }

  template<typename SEWTYPE, template<class> class VOP, VecOpKind KIND, bool DST_IS_MASK, bool FORCE_VS2_ZERO, bool REVERSE>
  static bool vec_oper_exec( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd   = Inst.vd;
    uint64_t vs1  = Inst.vs1;
    uint64_t vs2  = Inst.vs2;
    uint64_t mask = 0;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el  = vecElem % V->ElemsPerReg();
      if (DST_IS_MASK || V->v0mask(Inst.vm, vecElem)) {
        SEWTYPE res = 0;
        SEWTYPE s2 = FORCE_VS2_ZERO ? 0 : V->GetElem<SEWTYPE>(vs2,el);
        SEWTYPE s1;
        switch( KIND ) {
        case VReg: s1 = V->GetElem<SEWTYPE>( vs1, el ); break;
        case Imm:  s1 = (Inst.Inst >> 15) & 0x1f; break;
        case Reg:  s1 = R->GetX<SEWTYPE>( Inst.rs1 ); break;
        }
        res = REVERSE ? VOP()(s1,s2) : VOP()(s2, s1);
        if( DST_IS_MASK ) {
          assert( vecElem < 64 );  // TODO
          mask |= ( (res!=0) & 1 ) << vecElem;
        } else {
          V->SetElem<SEWTYPE>( Inst.vd, vd, vecElem, el, res );
        }
      }
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 ) {
        vd++;
        vs1++;
        vs2++;
      }
    }
    if( DST_IS_MASK ) {
      //TODO: Fix. Assumes only 64-bits of mask generated
      V->SetMaskReg( Inst.vd, mask );
    }
    return true;
  }

  /// Vector Multiply-Acc
  static bool vfmacc_oper( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == sew_t::e64 ) {
      vfmacc_oper_exec<double>(F, R, V, M, Inst);
    } else if (vt.f.vsew == sew_t::e32) {
      vfmacc_oper_exec<float>(F, R, V, M, Inst);
    } else {
      V->output()->fatal(CALL_INFO, -1, "Only supporting e64 or e32 for vector floating point operations\n");
      return false;
    }
    return true;
  }

  template<typename FTYPE>
  static bool vfmacc_oper_exec( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd   = Inst.vd;
    uint64_t vs1  = Inst.vs1;
    uint64_t vs2  = Inst.vs2;
    // vfmacc.vv vd, vs1, vs2, vm # vd[i] = +(vs1[i] * vs2[i]) + vd[i]
    // vfmacc.vf vd, rs1, vs2, vm # vd[i] = +(f[rs1] * vs2[i]) + vd[i]
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el  = vecElem % V->ElemsPerReg();
      if ( V->v0mask(Inst.vm, vecElem)) {
        FTYPE s1 = R->GetFP<FTYPE>(Inst.rs1);
        FTYPE s2 = V->GetElem<FTYPE>(vs2, el);
        FTYPE dst = V->GetElem<FTYPE>(vd, el);
        FTYPE res = s1 * s2 + dst;
        V->SetElem<FTYPE>(Inst.vd, vd, vecElem, el, res);
      }
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 ) {
        vd++;
        vs1++;
        vs2++;
      }
    }  
    return true;
  }
  
  /// Vector Mask Operator Entry Template
  template<template<class> class VOP, MaskOpMod MOD=None>
  static bool vmask_oper( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    // Mask operations affect a single vector register.
    //TODO test: The number of elements should be VLMAX so it can span multiple registers when VLMAX > VLEN.
    // To stay sane, and maybe this is the intent, traverse VLMAX elements using the programmed SEW.
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == sew_t::e64 )
      return vmask_oper_exec<uint64_t, VOP, MOD>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e32 )
      return vmask_oper_exec<uint32_t, VOP, MOD>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e16 )
      return vmask_oper_exec<uint16_t, VOP, MOD>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e8 )
      return vmask_oper_exec<uint8_t, VOP, MOD>( F, R, V, M, Inst );
    else {
      assert( false );
    }
    return false;
  }

  template<typename SEWTYPE, template<class> class VOP, MaskOpMod MOD>
  static bool vmask_oper_exec( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t vd  = Inst.vd;
    uint64_t vs1 = Inst.vs1;
    uint64_t vs2 = Inst.vs2;
    for( unsigned vecElem = 0; vecElem < V->GetVCSR( RevCSR::vl ); vecElem++ ) {
      unsigned el  = vecElem % V->ElemsPerReg();
      SEWTYPE s1 = V->GetElem<SEWTYPE>( vs1, el );
      if (MOD==NotVS1) s1 = ~s1;
      SEWTYPE  res = VOP()( V->GetElem<SEWTYPE>( vs2, el ), s1 );
      if (MOD==Not) res = ~res;
      V->SetElem<SEWTYPE>( Inst.vd, vd, vecElem, el, res );
      if( ( ( el + 1 ) % V->ElemsPerReg() ) == 0 ) {
        vd++;
        vs1++;
        vs2++;
      }
    }
    return true;
  }

  /// The vfirst instruction finds the lowest-numbered active element of the source mask vector that has the
  /// value 1 and writes that element’s index to a GPR. If no active element has the value 1, -1 is written to the
  /// GPR.
  static bool vfirstm( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    uint64_t res = V->vfirst(Inst.vs2);
    std::cout << "*V r" << Inst.rd << " <- 0x" << std::hex << res << std::endl;
    R->SetX<uint64_t>(Inst.rd,res);
    return true;
  }

  // Vector Single-Width Integer Add and Subtract
  static constexpr auto& vaddvv  = vec_oper<std::plus, VecOpKind::VReg>;
  static constexpr auto& vaddvx  = vec_oper<std::plus, VecOpKind::Reg>;
  static constexpr auto& vaddvi  = vec_oper<std::plus, VecOpKind::Imm>;
  static constexpr auto& vsubvv  = vec_oper<std::minus, VecOpKind::VReg>;
  static constexpr auto& vsubvx  = vec_oper<std::minus, VecOpKind::Reg>;
  static constexpr auto& vrsubvx = vec_oper<std::minus, VecOpKind::Reg, false, false, true>;
  static constexpr auto& vrsubvi = vec_oper<std::minus, VecOpKind::Imm, false, false, true>;
  //TODO Vector Widening Integer Add/Subtract
  //TODO Vector Integer Extension
  //TODO Vector Integer Add-with-Carry / Subtract-with-Borrow Instructions
  // Vector Bitwise logical operations
  static constexpr auto& vandvv  = vec_oper<std::bit_and, VecOpKind::VReg>;
  static constexpr auto& vandvx  = vec_oper<std::bit_and, VecOpKind::Reg>;
  static constexpr auto& vandvi  = vec_oper<std::bit_and, VecOpKind::Imm>;
  static constexpr auto& vorvv  = vec_oper<std::bit_or, VecOpKind::VReg>;
  static constexpr auto& vorvx  = vec_oper<std::bit_or, VecOpKind::Reg>;
  static constexpr auto& vorvi  = vec_oper<std::bit_or, VecOpKind::Imm>;
  static constexpr auto& vxorvv  = vec_oper<std::bit_xor, VecOpKind::VReg>;
  static constexpr auto& vxorvx  = vec_oper<std::bit_xor, VecOpKind::Reg>;
  static constexpr auto& vxorvi  = vec_oper<std::bit_xor, VecOpKind::Imm>;
  //TODO Vector Single-Width Shift Instructions
  //TODO Vector Narrowing Integer Right Shift Instructions
  // Vector Integer Compare Instructions
  static constexpr auto& vmseqvv = vec_oper<std::equal_to, VecOpKind::VReg, true>;
  static constexpr auto& vmseqvx = vec_oper<std::equal_to, VecOpKind::Reg, true>;
  static constexpr auto& vmseqvi = vec_oper<std::equal_to, VecOpKind::Imm, true>;
  static constexpr auto& vmsnevv = vec_oper<std::not_equal_to, VecOpKind::VReg, true>;
  static constexpr auto& vmsnevx = vec_oper<std::not_equal_to, VecOpKind::Reg, true>;
  static constexpr auto& vmsnevi = vec_oper<std::not_equal_to, VecOpKind::Imm, true>;
  // static constexpr auto& vmsltuvv = vec_oper<std::tbd, VecOpKind::VReg, true>;
  // static constexpr auto& vmsltuvx = vec_oper<std::tbd, VecOpKind::Reg, true>;
  // static constexpr auto& vmsltvv = vec_oper<std::tbd, VecOpKind::VReg, true>;
  // static constexpr auto& vmsltvx = vec_oper<std::tbd, VecOpKind::Reg, true>;
  // static constexpr auto& vmsleuvv = vec_oper<std::tbd, VecOpKind::VReg, true>;
  // static constexpr auto& vmsleuvx = vec_oper<std::tbd, VecOpKind::Reg, true>;
  // static constexpr auto& vmsleuvi = vec_oper<std::tbd, VecOpKind::Imm, true>;
  static constexpr auto& vmslevv = vec_oper<std::less_equal, VecOpKind::VReg, true>;
  static constexpr auto& vmslevx = vec_oper<std::less_equal, VecOpKind::Reg, true>;
  static constexpr auto& vmslevi = vec_oper<std::less_equal, VecOpKind::Imm, true>;
  // static constexpr auto& vmsgtuvx = vec_oper<std::tbd, VecOpKind::Reg, true>;
  // static constexpr auto& vmsgtuvi = vec_oper<std::tbd, VecOpKind::Imm, true>;
  // static constexpr auto& vmsgtvx = vec_oper<std::tbd, VecOpKind::Reg, true>;
  // static constexpr auto& vmsgtvi = vec_oper<std::tbd, VecOpKind::Imm, true>;
  
  //TODO Vector Integer Min/Max Instructions
  //TODO Vector Single-Width Integer Multiply Instructions
  //TODO Vector Integer Divide Instructions
  //TODO Vector Widening Integer Multiply Instructions
  //TODO Vector Single-Width Integer Multiply-Add Instructions
  //TODO Vector Widening Integer Multiply-Add Instructions
  //TODO Vector Integer Merge Instructions
  
  // Vector Integer Move Instructions
  static constexpr auto& vmvvv   = vec_oper<std::plus, VecOpKind::VReg, false, true>;
  static constexpr auto& vmvvx   = vec_oper<std::plus, VecOpKind::Reg, false, true>;
  static constexpr auto& vmvvi   = vec_oper<std::plus, VecOpKind::Imm, false, true>;

  //TODO Vector Single-Width Saturating Add and Subtract
  //TODO Vector Single-Width Averaging Add and Subtract
  //TODO Vector Single-Width Fractional Multiply with Rounding and Saturation
  //TODO Vector Single-Width Scaling Shift Instructions
  //TODO Vector Narrowing Fixed-Point Clip Instructions

  //TODO Vector Single-Width Floating-Point Add/Subtract Instructions
  //TODO Vector Widening Floating-Point Add/Subtract Instructions

  // Vector Mask Instructions
  static constexpr auto& vmandmm  = vmask_oper<std::bit_and>;                    // vd.mask[i] = vs2.mask[i] && vs1.mask[i]
  static constexpr auto& vmnandmm = vmask_oper<std::bit_and, MaskOpMod::Not>;    // vd.mask[i] = !(vs2.mask[i] && vs1.mask[i])
  static constexpr auto& vmandnmm = vmask_oper<std::bit_and, MaskOpMod::NotVS1>; // vd.mask[i] = vs2.mask[i] && !vs1.mask[i]
  static constexpr auto& vmxormm  = vmask_oper<std::bit_xor>;                    // vd.mask[i] = vs2.mask[i] ^^ vs1.mask[i]
  static constexpr auto& vmormm   = vmask_oper<std::bit_or>;                     // vd.mask[i] = vs2.mask[i] || vs1.mask[i]
  static constexpr auto& vmnormm  = vmask_oper<std::bit_or,  MaskOpMod::Not>;    // vd.mask[i] = !(vs2.mask[i] || vs1.mask[i])
  static constexpr auto& vmornmm  = vmask_oper<std::bit_or,  MaskOpMod::NotVS1>; // vd.mask[i] = vs2.mask[i] || !vs1.mask[i]
  static constexpr auto& vmxnormm = vmask_oper<std::bit_xor, MaskOpMod::Not>;    // vd.mask[i] = !(vs2.mask[i] ^^ vs1.mask[i])


  // container for parameters common to vector loads and stores
  template<typename EEW, typename SEW, VecMemOpKind KIND> 
  struct memopParams_t {
    uint64_t addr;
    uint64_t vd;
    uint64_t vs3; // stores only
    uint64_t evl;
    unsigned elemsPerReg;
    bool evm;
    memopParams_t<EEW, SEW>(RevRegFile* R, RevVRegFile* V, const RevVecInst& Inst) {
      addr = R->GetX<uint64_t>( Inst.rs1 );
      vd   = Inst.vd;
      vs3  = Inst.vs3;
      evm = Inst.vm;
      unsigned vl = V->GetVCSR( RevCSR::vl );
      if (KIND==UnitStrideMask) {
        evl = vl>0 ? ((vl-1)>>3) + 1 : 0;   // ceil(vl/8)
        elemsPerReg = evl;
      } else {
        evl = vl * sizeof(SEW) / sizeof(EEW); // todo ceiling
        elemsPerReg = V->ElemsPerReg() * sizeof(SEW) / sizeof(EEW);
        // We need to check if emul is valid but unsure if we need to use it
        V->emul<EEW,SEW>();
      }
    }
  };

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

  // Vector Load Template
  template<typename EEW, typename SEW, VecMemOpKind KIND>
  static bool vload_exec( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {

    memopParams_t params = memopParams_t<EEW, SEW, KIND>(R, V, Inst);

    if (KIND==FaultOnlyFirst) {
      //TODO
      V->output()->verbose(
        CALL_INFO, 1, 0, "Warning: vector load fault-only first not implemented and will behave has a normal vector unit stride load\n"
      );
    }

    for( unsigned vecElem = 0; vecElem < params.evl; vecElem++ ) {
      unsigned el = vecElem % params.elemsPerReg;
      if ( V->v0mask(params.evm, vecElem) ) {
        EEW  d  = 0;
        MemReq   req0( params.addr, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, R->GetMarkLoadComplete() );
        M->ReadVal<EEW>( 0, params.addr, &d, req0, RevFlag::F_NONE );
        // Writing to vector registers uses the SEW 
        std::stringstream s;
        s << "0x" << std::hex << (uint64_t) d << " <- M[0x" << params.addr << "]" << std::endl;
        V->SetElem<EEW>( Inst.vd, params.vd, vecElem, el, d );
      }
      params.addr += sizeof( EEW );
      if( ( ( el + 1 ) % params.elemsPerReg ) == 0 )
        params.vd++;
    }
    return true;
  }

  // Vector Load Entry Template
  template<typename EEW, VecMemOpKind KIND>
  static bool vload( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == sew_t::e64 )
      return vload_exec<EEW, uint64_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e32 )
      return vload_exec<EEW, uint32_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e16 )
      return vload_exec<EEW, uint16_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e8 )
      return vload_exec<EEW, uint8_t, KIND>( F, R, V, M, Inst );
    return false;
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

  // Vector Store Template
  template<typename EEW, typename SEW, VecMemOpKind KIND>
  static bool vstore_exec( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    memopParams_t params = memopParams_t<EEW, SEW, KIND>(R, V, Inst);
    for( unsigned vecElem = 0; vecElem < params.evl; vecElem++ ) {
      unsigned el = vecElem % params.elemsPerReg;
      if (V->v0mask(params.evm, vecElem)) {
        EEW  d  = V->GetElem<EEW>( params.vs3, el );
        M->WriteMem( 0, params.addr, sizeof( EEW ), &d ); 
        std::cout << "*V M[0x" << std::hex << params.addr << "] <- 0x" << std::hex << (uint64_t) d << " <- v" << std::dec << params.vs3 << "."
                  << std::dec << el << std::endl;
        params.addr += sizeof( EEW );
      }
      if( ( ( el + 1 ) % params.elemsPerReg ) == 0 )
        params.vs3++;

    }
    return true;
  }

  // Vector Store Entry Template
  template<typename EEW, VecMemOpKind KIND>
  static bool vstore( const RevFeature* F, RevRegFile* R, RevVRegFile* V, RevMem* M, const RevVecInst& Inst ) {
    reg_vtype_t vt = V->GetVCSR( RevCSR::vtype );
    if( vt.f.vsew == sew_t::e64 )
      return vstore_exec<EEW, uint64_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e32 )
      return vstore_exec<EEW, uint32_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e16 )
      return vstore_exec<EEW, uint16_t, KIND>( F, R, V, M, Inst );
    else if( vt.f.vsew == sew_t::e8 )
      return vstore_exec<EEW, uint8_t, KIND>( F, R, V, M, Inst );
    return false;
  }

  /**
   * Vector Instruction Table
   */

  // clang-format off
  std::vector<RevVecInstEntry> RVVTable = {

  /* Configuration-Setting Instructions */
  RevVecInstDefaults().SetMnemonic("vsetvli %rd, %rs1, %zimm11"    ).SetFunct3(OPV::CFG).SetImplFunc(&vsetvli ).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 31) & 0x1)  == 0x00; } ),
  RevVecInstDefaults().SetMnemonic("vsetivli %rd, %zimm, %zimm10"  ).SetFunct3(OPV::CFG).SetImplFunc(&vsetivli).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegUNKNOWN)  .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 30) & 0x3)  == 0x03; } ),
  RevVecInstDefaults().SetMnemonic("vsetvl %rd, %rs1, %rs2"        ).SetFunct3(OPV::CFG).SetImplFunc(&vsetvl  ).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegGPR    )  .SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 25) & 0x7f) == 0x40; } ),

  /* Vector Loads and Stores*/
  // Vector Unit-Stride Instructions
  RevVecInstDefaults().SetMnemonic("vle64.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u64).SetImplFunc(&vload<uint64_t,  UnitStride>    ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle32.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u32).SetImplFunc(&vload<uint32_t,  UnitStride>    ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle16.v %vd, (%rs1), %vm"      ).SetFunct3(MSZ::u16).SetImplFunc(&vload<uint16_t,  UnitStride>    ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vle8.v %vd, (%rs1), %vm"       ).SetFunct3(MSZ::u8 ).SetImplFunc(&vload<uint8_t,   UnitStride>    ).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse64.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u64).SetImplFunc(&vstore<uint64_t, UnitStride>    ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse32.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u32).SetImplFunc(&vstore<uint32_t, UnitStride>    ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse16.v %vs3, (%rs1), %vm"     ).SetFunct3(MSZ::u16).SetImplFunc(&vstore<uint16_t, UnitStride>    ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  RevVecInstDefaults().SetMnemonic("vse8.v %vs3, (%rs1), %vm "     ).SetFunct3(MSZ::u8 ).SetImplFunc(&vstore<uint8_t,  UnitStride>    ).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b00000),
  // Vector Unit-Stride Mask Load/Start
  RevVecInstDefaults().SetMnemonic("vlm.v %vd, (%rs1)"             ).SetFunct3(MSZ::u8 ).SetImplFunc(&vload<uint8_t,   UnitStrideMask>).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                     .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b01011),
  RevVecInstDefaults().SetMnemonic("vsm.v %vs3, (%rs1) "           ).SetFunct3(MSZ::u8 ).SetImplFunc(&vstore<uint8_t,  UnitStrideMask>).SetrdClass(RevRegClass::RegUNKNOWN).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)  .Setrs3Class(RevRegClass::RegVEC)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111).SetMop(0b00).SetUmop(0b01011),

  //TODO Vector Strided Instructions
  //TODO Vector Indexed Instructions
  // Unit Stride Load Fault-only first
  RevVecInstDefaults().SetMnemonic("vle64ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u64).SetImplFunc(&vload<uint64_t, FaultOnlyFirst>).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle32ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u32).SetImplFunc(&vload<uint32_t, FaultOnlyFirst>).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle16ff.v %vd, (%rs1), %vm"    ).SetFunct3(MSZ::u16).SetImplFunc(&vload<uint16_t, FaultOnlyFirst>).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  RevVecInstDefaults().SetMnemonic("vle8ff.v %vd, (%rs1), %vm"     ).SetFunct3(MSZ::u8 ).SetImplFunc(&vload<uint8_t,  FaultOnlyFirst>).SetrdClass(RevRegClass::RegVEC    ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegUNKNOWN)                                   .SetFormat(RVVTypeLd).SetOpcode(0b0000111).SetMop(0b00).SetUmop(0b10000),
  //TODO Vector Load/Store Segment Instructions
  //TODO Vector Load/Store Whole Register Instructions

  /* Vector Arithemetic Instructions */
  // Vector Single-Width Integer Add and Subtract
  RevVecInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b000000).SetImplFunc(&vaddvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vadd.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b000000).SetImplFunc(&vaddvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vadd.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b000000).SetImplFunc(&vaddvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vsub.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b000010).SetImplFunc(&vsubvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vsub.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b000010).SetImplFunc(&vsubvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vrsub.vx %vd, %vs2, %vs1, %vm" ).SetFunct3(OPV::IVV).SetFunct6(0b000011).SetImplFunc(&vrsubvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vrsub.vi %vd, %vs2, %rs1, %vm" ).SetFunct3(OPV::IVX).SetFunct6(0b000011).SetImplFunc(&vrsubvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  //TODO Vector Widening Integer Add/Subtract
  //TODO Vector Integer Extension
  //TODO Vector Integer Add-with-Carry / Subtract-with-Borrow Instructions
  // Vector Bitwise logical operations
  RevVecInstDefaults().SetMnemonic("vand.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b001001).SetImplFunc(&vandvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vand.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b001001).SetImplFunc(&vandvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vand.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b001001).SetImplFunc(&vandvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vor.vv %vd, %vs2, %vs1, %vm"   ).SetFunct3(OPV::IVV).SetFunct6(0b001010).SetImplFunc(&vorvv ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vor.vx %vd, %vs2, %rs1, %vm"   ).SetFunct3(OPV::IVX).SetFunct6(0b001010).SetImplFunc(&vorvx ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vor.vi %vd, %vs2, %zimm5, %vm" ).SetFunct3(OPV::IVI).SetFunct6(0b001010).SetImplFunc(&vorvi ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vxor.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b001011).SetImplFunc(&vxorvv).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vxor.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b001011).SetImplFunc(&vxorvx).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vxor.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b001011).SetImplFunc(&vxorvi).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),


  //TODO Vector Single-Width Shift Instructions
  //TODO Vector Narrowing Integer Right Shift Instructions
  // Vector Integer Compare Instructions
  RevVecInstDefaults().SetMnemonic("vmseq.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b011000).SetImplFunc(&vmseqvv).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmseq.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b011000).SetImplFunc(&vmseqvx).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmseq.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b011000).SetImplFunc(&vmseqvi).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsne.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b011001).SetImplFunc(&vmsnevv).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsne.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b011001).SetImplFunc(&vmsnevx).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsne.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b011001).SetImplFunc(&vmsnevi).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsle.vv %vd, %vs2, %vs1, %vm"  ).SetFunct3(OPV::IVV).SetFunct6(0b011101).SetImplFunc(&vmslevv).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsle.vx %vd, %vs2, %rs1, %vm"  ).SetFunct3(OPV::IVX).SetFunct6(0b011101).SetImplFunc(&vmslevx).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmsle.vi %vd, %vs2, %zimm5, %vm").SetFunct3(OPV::IVI).SetFunct6(0b011101).SetImplFunc(&vmslevi).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  //TODO Vector Integer Min/Max Instructions
  //TODO Vector Single-Width Integer Multiply Instructions
  //TODO Vector Integer Divide Instructions
  //TODO Vector Widening Integer Multiply Instructions
  //TODO Vector Single-Width Integer Multiply-Add Instructions
  //TODO Vector Widening Integer Multiply-Add Instructions
  //TODO Vector Integer Merge Instructions
  // Vector Integer Move Instructions
  RevVecInstDefaults().SetMnemonic("vmv.v.v %vd, %vs1"             ).SetFunct3(OPV::IVV).SetFunct6(0b010111).SetImplFunc(&vmvvv ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 20) & 0x1f)  == 0; } ),
  RevVecInstDefaults().SetMnemonic("vmv.v.x %vd, %rs1"             ).SetFunct3(OPV::IVX).SetFunct6(0b010111).SetImplFunc(&vmvvx ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegGPR    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 20) & 0x1f)  == 0; } ),
  RevVecInstDefaults().SetMnemonic("vmv.v.i %vd, zimm5"            ).SetFunct3(OPV::IVI).SetFunct6(0b010111).SetImplFunc(&vmvvi ).SetrdClass(RevRegClass::RegVEC ).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 20) & 0x1f)  == 0; } ),
  
  /* Vector Fixed-Point Arithmetic Instructions */
  //TODO Vector Single-Width Saturating Add and Subtract
  //TODO Vector Single-Width Averaging Add and Subtract
  //TODO Vector Single-Width Fractional Multiply with Rounding and Saturation
  //TODO Vector Single-Width Scaling Shift Instructions
  //TODO Vector Narrowing Fixed-Point Clip Instructions

  /* Vector Floating-Point Instructions */
  //TODO Vector Single-Width Floating-Point Add/Subtract Instructions
  //TODO Vector Widening Floating-Point Add/Subtract Instructions
  //TODO Vector Single-Width Floating-Point Multiply/Divide Instructions
  //TODO Vector Widening Floating-Point Multiply
  //TODO Vector Single-Width Floating-Point Fused Multiply-Add Instructions
  //RevVecInstDefaults().SetMnemonic("vfmacc.vv %vd, %vs1, %vs2, %vm").SetFunct3(OPV::FVV).SetFunct6(0b101100).SetImplFunc(&vfmacc_oper).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC     ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vfmacc.vf %vd, %rs1, %vs2, %vm").SetFunct3(OPV::FVF).SetFunct6(0b101100).SetImplFunc(&vfmacc_oper).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegFLOAT   ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  //TODO Vector Widening Floating-Point Fused Multiply-Add Instructions
  //TODO Vector Floating-Point Square-Root Instruction
  //TODO Vector Floating-Point Reciprocal Square-Root Estimate Instruction
  //TODO Vector Floating-Point Reciprocal Estimate Instruction
  //TODO Vector Floating-Point MIN/MAX Instructions
  //TODO Vector Floating-Point Sign-Injection Instructions
  //TODO Vector Floating-Point Compare Instructions
  //TODO Vector Floating-Point Classify Instruction
  //TODO Vector Floating-Point Merge Instruction
  //TODO Vector Floating-Point Move Instruction
  //TODO Single-Width Floating-Point/Integer Type-Convert Instructions
  //TODO Widening Floating-Point/Integer Type-Convert Instructions
  //TODO Narrowing Floating-Point/Integer Type-Convert Instructions

  /* Vector Reduction Operations*/
  //TODO Vector Single-Width Integer Reduction Instructions
  //TODO Vector Widening Integer Reduction Instructions
  //TODO Vector Single-Width Floating-Point Reduction Instructions
  //TODO Vector Widening Floating-Point Reduction Instructions

  /* Vector Mask Instructions */

  // Vector Mask-Register Logical Instructions
  RevVecInstDefaults().SetMnemonic("vmand.mm %vd, %vs2, %vs1"      ).SetFunct3(OPV::MVV).SetFunct6(0b011001).SetImplFunc(&vmandmm ).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmnand.mm %vd, %vs2, %vs1"     ).SetFunct3(OPV::MVV).SetFunct6(0b011101).SetImplFunc(&vmnandmm).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmandn.mm %vd, %vs2, %vs1"     ).SetFunct3(OPV::MVV).SetFunct6(0b011000).SetImplFunc(&vmandnmm).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmxor.mm %vd, %vs2, %vs1"      ).SetFunct3(OPV::MVV).SetFunct6(0b011011).SetImplFunc(&vmxormm ).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmor.mm %vd, %vs2, %vs1"       ).SetFunct3(OPV::MVV).SetFunct6(0b011010).SetImplFunc(&vmormm  ).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmnor.mm %vd, %vs2, %vs1"      ).SetFunct3(OPV::MVV).SetFunct6(0b011110).SetImplFunc(&vmnormm ).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmorn.mm %vd, %vs2, %vs1"      ).SetFunct3(OPV::MVV).SetFunct6(0b011100).SetImplFunc(&vmornmm ).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  RevVecInstDefaults().SetMnemonic("vmxnor.mm %vd, %vs2, %vs1"     ).SetFunct3(OPV::MVV).SetFunct6(0b011111).SetImplFunc(&vmxnormm).SetrdClass(RevRegClass::RegVEC).Setrs1Class(RevRegClass::RegVEC    ).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111),
  //TODO Vector count population in mask vcpop.m
  // vfirst find-first-set mask bit
  // -- funct6=010000 V VWXUNARY0 {VS1,op} {0b00000,vmv.x.s} {0b10000,vcpop} {0b10001,vfirst}
  RevVecInstDefaults().SetMnemonic("vfirst.m %rd, %vs2, %vm"       ).SetFunct3(OPV::MVV).SetFunct6(0b010000).SetImplFunc(&vfirstm).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegUNKNOWN).Setrs2Class(RevRegClass::RegVEC).SetFormat(RVVTypeOpv).SetOpcode(0b1010111) .SetPredicate( []( uint32_t Inst ){ return ((Inst >> 15) & 0x1f)  == 0b10001; } ),
  //TODO vmsbf.m set-before-first mask bit
  //TODO vmsif.m set-including-first mask bit
  //TODO vmsof.m set-only-first mask bit
  //TODO Example using vector mask instructions
  //TODO Vector Iota Instruction
  //TODO Vector Element Index Instruction

  /* Vector Permutation Instructions */
  //TODO Integer Scalar Move Instructions
  //TODO Floating-Point Scalar Move Instructions
  //TODO Vector Slideup Instructions
  //TODO Vector Slidedown Instructions
  //TODO Vector Slide1up
  //TODO Vector Floating-Point Slide1up Instruction
  //TODO Vector Slide1down Instruction
  //TODO Vector Floating-Point Slide1down Instruction
  //TODO Vector Register Gather Instructions
  //TODO Vector Compress Instruction
  //TODO Whole Vector Register Move 

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
