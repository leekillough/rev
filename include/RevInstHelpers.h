//
// _RevInstHelpers_h_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVINSTHELPERS_H_
#define _SST_REVCPU_REVINSTHELPERS_H_

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "RevInstTable.h"

namespace SST::RevCPU{

/// General template for converting between Floating Point and Integer.
/// FP values outside the range of the target integer type are clipped
/// at the integer type's numerical limits, whether signed or unsigned.
template<typename FP, typename INT>
bool CvtFpToInt(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  FP fp = R->GetFP<FP>(Inst.rs1); // Read the FP register
  constexpr INT max = std::numeric_limits<INT>::max();
  constexpr INT min = std::numeric_limits<INT>::min();
  INT res = std::isnan(fp) || fp > FP(max) ? max : fp < FP(min) ? min : static_cast<INT>(fp);

  // Make final result signed so sign extension occurs when sizeof(INT) < XLEN
  R->SetX(Inst.rd, static_cast<std::make_signed_t<INT>>(res));

  R->AdvancePC(Inst);
  return true;
}

/// fclass: Return FP classification like the RISC-V fclass instruction
// See: https://github.com/riscv/riscv-isa-sim/blob/master/softfloat/f32_classify.c
// Because quiet and signaling NaNs are not distinguished by the C++ standard,
// an additional argument has been added to disambiguate between quiet and
// signaling NaNs.
template<typename T>
unsigned fclass(T val, bool quietNaN = true) {
  switch(std::fpclassify(val)){
  case FP_INFINITE:
    return std::signbit(val) ? 1 : 1 << 7;
  case FP_NAN:
    return quietNaN ? 1 << 9 : 1 << 8;
  case FP_NORMAL:
    return std::signbit(val) ? 1 << 1 : 1 << 6;
  case FP_SUBNORMAL:
    return std::signbit(val) ? 1 << 2 : 1 << 5;
  case FP_ZERO:
    return std::signbit(val) ? 1 << 3 : 1 << 4;
  default:
    return 0;
  }
}

/// Load template
template<typename T>
bool load(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  if( sizeof(T) < sizeof(int64_t) && R->IsRV32 ){
    static constexpr RevFlag flags = sizeof(T) < sizeof(int32_t) ?
      std::is_signed_v<T> ? RevFlag::F_SEXT32 : RevFlag::F_ZEXT32 : RevFlag::F_NONE;
    uint64_t rs1 = R->GetX<uint64_t>(Inst.rs1); // read once for tracer
    MemReq req(rs1 + Inst.ImmSignExt(12),
               Inst.rd, RevRegClass::RegGPR,
               F->GetHartToExecID(),
               MemOp::MemOpREAD,
               true,
               R->GetMarkLoadComplete());
    R->LSQueue->insert(req.LSQHashPair());
    M->ReadVal(F->GetHartToExecID(),
               rs1 + Inst.ImmSignExt(12),
               reinterpret_cast<std::make_unsigned_t<T>*>(&R->RV32[Inst.rd]),
               std::move(req),
               flags);
    R->SetX(Inst.rd, static_cast<T>(R->RV32[Inst.rd]));

  }else{
    static constexpr RevFlag flags = sizeof(T) < sizeof(int64_t) ?
      std::is_signed_v<T> ? RevFlag::F_SEXT64 : RevFlag::F_ZEXT64 : RevFlag::F_NONE;
    uint64_t rs1 = R->GetX<uint64_t>(Inst.rs1);
    MemReq req(rs1 + Inst.ImmSignExt(12),
               Inst.rd, RevRegClass::RegGPR,
               F->GetHartToExecID(),
               MemOp::MemOpREAD,
               true,
               R->GetMarkLoadComplete());
    R->LSQueue->insert(req.LSQHashPair());
    M->ReadVal(F->GetHartToExecID(),
               rs1 + Inst.ImmSignExt(12),
               reinterpret_cast<std::make_unsigned_t<T>*>(&R->RV64[Inst.rd]),
               std::move(req),
               flags);
    R->SetX(Inst.rd, static_cast<T>(R->RV64[Inst.rd]));
  }

  // update the cost
  R->cost += M->RandCost(F->GetMinCost(), F->GetMaxCost());
  R->AdvancePC(Inst);
  return true;
}

/// Store template
template<typename T>
bool store(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  M->Write(F->GetHartToExecID(),
           R->GetX<uint64_t>(Inst.rs1) + Inst.ImmSignExt(12),
           R->GetX<T>(Inst.rs2));
  R->AdvancePC(Inst);
  return true;
}

/// Floating-point load template
template<typename T>
bool fload(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  if(std::is_same_v<T, double> || F->HasD()){
    static constexpr RevFlag flags = sizeof(T) < sizeof(double) ?
      RevFlag::F_BOXNAN : RevFlag::F_NONE;

    uint64_t rs1 = R->GetX<uint64_t>(Inst.rs1);
    MemReq req(rs1 + Inst.ImmSignExt(12),
               Inst.rd,
               RevRegClass::RegFLOAT,
               F->GetHartToExecID(),
               MemOp::MemOpREAD,
               true,
               R->GetMarkLoadComplete());
    R->LSQueue->insert(req.LSQHashPair());
    M->ReadVal(F->GetHartToExecID(),
               rs1 + Inst.ImmSignExt(12),
               reinterpret_cast<T*>(&R->DPF[Inst.rd]),
               std::move(req),
               flags);

    // Box float value into 64-bit FP register
    if(std::is_same_v<T, float>){
      double fp = R->GetFP<double>(Inst.rd);
      BoxNaN(&fp, &fp);
      R->SetFP(Inst.rd, fp);
    }
  }else{
    uint64_t rs1 = R->GetX<uint64_t>(Inst.rs1);
    MemReq req(rs1 + Inst.ImmSignExt(12),
               Inst.rd,
               RevRegClass::RegFLOAT,
               F->GetHartToExecID(),
               MemOp::MemOpREAD,
               true,
               R->GetMarkLoadComplete());
    R->LSQueue->insert(req.LSQHashPair());
    M->ReadVal(F->GetHartToExecID(),
               rs1 + Inst.ImmSignExt(12),
               &R->SPF[Inst.rd],
               std::move(req),
               RevFlag::F_NONE);
  }
  // update the cost
  R->cost += M->RandCost(F->GetMinCost(), F->GetMaxCost());
  R->AdvancePC(Inst);
  return true;
}

/// Floating-point store template
template<typename T>
bool fstore(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  T val = R->GetFP<T, true>(Inst.rs2);
  M->Write(F->GetHartToExecID(), R->GetX<uint64_t>(Inst.rs1) + Inst.ImmSignExt(12), val);
  R->AdvancePC(Inst);
  return true;
}

/// Floating-point operation template
template<typename T, template<class> class OP>
bool foper(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  R->SetFP(Inst.rd, OP()(R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2)));
  R->AdvancePC(Inst);
  return true;
}

/// Floating-point minimum functor
template<typename = void>
struct FMin{
  template<typename T>
  auto operator()(T x, T y) const { return std::fmin(x, y); }
};

/// Floating-point maximum functor
template<typename = void>
struct FMax{
  template<typename T>
  auto operator()(T x, T y) const { return std::fmax(x, y); }
};

/// Floating-point conditional operation template
template<typename T, template<class> class OP>
bool fcondop(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  bool res = OP()(R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2));
  R->SetX(Inst.rd, res);
  R->AdvancePC(Inst);
  return true;
}

/// Operand Kind (immediate or register)
enum class OpKind { Imm, Reg };

/// Integer arithmetic operator template
// The First parameter is the operator functor (such as std::plus)
// The second parameter is the operand kind (OpKind::Imm or OpKind::Reg)
// The third parameter is std::make_unsigned_t or std::make_signed_t (default)
// The optional fourth parameter indicates W mode (32-bit on XLEN == 64)
template<template<class> class OP, OpKind KIND,
  template<class> class SIGN = std::make_signed_t, bool W_MODE = false>
  bool oper(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  if( !W_MODE && R->IsRV32 ){
    using T = SIGN<int32_t>;
    T rs1 = R->GetX<T>(Inst.rs1);
    T rs2 = KIND == OpKind::Imm ? T(Inst.ImmSignExt(12)) : R->GetX<T>(Inst.rs2);
    T res = OP()(rs1, rs2);
    R->SetX(Inst.rd, res);
  }else{
    using T = SIGN<std::conditional_t<W_MODE, int32_t, int64_t>>;
    T rs1 = R->GetX<T>(Inst.rs1);
    T rs2 = KIND == OpKind::Imm ? T(Inst.ImmSignExt(12)) : R->GetX<T>(Inst.rs2);
    T res = OP()(rs1, rs2);
    // In W_MODE, cast the result to int32_t so that it's sign-extended
    R->SetX(Inst.rd, std::conditional_t<W_MODE, int32_t, T>(res));
  }
  R->AdvancePC(Inst);
  return true;
}

/// Left shift functor
  template<typename = void>
  struct ShiftLeft{
    template<typename T>
    constexpr T operator()(T val, unsigned shift) const {
      return val << (sizeof(T) == 4 ? shift & 0x1f : shift & 0x3f);
    }
  };

/// Right shift functor
template<typename = void>
  struct ShiftRight{
    template<typename T>
    constexpr T operator()(T val, unsigned shift) const {
      return val >> (sizeof(T) == 4 ? shift & 0x1f : shift & 0x3f);
    }
  };

// Computes the UPPER half of multiplication, based on signedness
template<bool rs1_is_signed, bool rs2_is_signed>
bool uppermul(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  if( R->IsRV32 ){
    uint32_t rs1 = R->GetX<uint32_t>(Inst.rs1);
    uint32_t rs2 = R->GetX<uint32_t>(Inst.rs2);
    uint32_t mul = static_cast<uint32_t>(rs1 * int64_t(rs2) >> 32);
    if (rs1_is_signed && (rs1 & (uint32_t{1}<<31)) != 0) mul -= rs2;
    if (rs2_is_signed && (rs2 & (uint32_t{1}<<31)) != 0) mul -= rs1;
    R->SetX(Inst.rd, mul);
  }else{
    uint64_t rs1 = R->GetX<uint64_t>(Inst.rs1);
    uint64_t rs2 = R->GetX<uint64_t>(Inst.rs2);
    uint64_t mul = static_cast<uint64_t>(rs1 * __int128(rs2) >> 64);
    if (rs1_is_signed && (rs1 & (uint64_t{1}<<63)) != 0) mul -= rs2;
    if (rs2_is_signed && (rs2 & (uint64_t{1}<<63)) != 0) mul -= rs1;
    R->SetX(Inst.rd, mul);
  }
  R->AdvancePC(Inst);
  return true;
}

enum class DivRem { Div, Rem };

/// Division/Remainder template
// The first parameter is DivRem::Div or DivRem::Rem
// The second parameter is std::make_signed_t or std::make_unsigned_t
// The optional third parameter indicates W mode (32-bit on XLEN == 64)
template<DivRem DIVREM, template<class> class SIGN, bool W_MODE = false>
  bool divrem(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  if( !W_MODE && R->IsRV32 ){
    using T = SIGN<int32_t>;
    T rs1 = R->GetX<T>(Inst.rs1);
    T rs2 = R->GetX<T>(Inst.rs2);
    T res;
    if constexpr(DIVREM == DivRem::Div){
        res = std::is_signed_v<T> && rs1 == std::numeric_limits<T>::min() &&
          rs2 == -T{1} ? rs1 : rs2 ? rs1 / rs2 : -T{1};
      }else{
      res = std::is_signed_v<T> && rs1 == std::numeric_limits<T>::min() &&
        rs2 == -T{1} ? 0 : rs2 ? rs1 % rs2 : rs1;
    }
    R->SetX(Inst.rd, res);
  } else {
    using T = SIGN<std::conditional_t<W_MODE, int32_t, int64_t>>;
    T rs1 = R->GetX<T>(Inst.rs1);
    T rs2 = R->GetX<T>(Inst.rs2);
    T res;
    if constexpr(DIVREM == DivRem::Div){
        res = std::is_signed_v<T> && rs1 == std::numeric_limits<T>::min() &&
          rs2 == -T{1} ? rs1 : rs2 ? rs1 / rs2 : -T{1};
      }else{
      res = std::is_signed_v<T> && rs1 == std::numeric_limits<T>::min() &&
        rs2 == -T{1} ? 0 : rs2 ? rs1 % rs2 : rs1;
    }
    // In W_MODE, cast the result to int32_t so that it's sign-extended
    R->SetX(Inst.rd, std::conditional_t<W_MODE, int32_t, T>(res));
  }
  R->AdvancePC(Inst);
  return true;
}

/// Conditional branch template
// The first template parameter is the comparison functor
// The second template parameter is std::make_signed_t or std::make_unsigned_t
template<template<class> class OP, template<class> class SIGN = std::make_unsigned_t>
bool bcond(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  bool cond;
  if( R->IsRV32 ){
    cond = OP()(R->GetX<SIGN<int32_t>>(Inst.rs1), R->GetX<SIGN<int32_t>>(Inst.rs2));
  }else{
    cond = OP()(R->GetX<SIGN<int64_t>>(Inst.rs1), R->GetX<SIGN<int64_t>>(Inst.rs2));
  }
  if(cond){
    R->SetPC(R->GetPC() + Inst.ImmSignExt(13));
  }else{
    R->AdvancePC(Inst);
  }
  return true;
}

/// Fused Multiply-Add
template<typename T>
bool fmadd(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  R->SetFP(Inst.rd, std::fma(R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2), R->GetFP<T>(Inst.rs3)));
  R->AdvancePC(Inst);
  return true;
}

/// Fused Multiply-Subtract
template<typename T>
bool fmsub(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  R->SetFP(Inst.rd, std::fma(R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2), -R->GetFP<T>(Inst.rs3)));
  R->AdvancePC(Inst);
  return true;
}

/// Fused Negated (Multiply-Subtract)
template<typename T>
bool fnmsub(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst)
{
  R->SetFP(Inst.rd, std::fma(-R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2), R->GetFP<T>(Inst.rs3)));
  R->AdvancePC(Inst);
  return true;
}

/// Fused Negated (Multiply-Add)
template<typename T>
bool fnmadd(RevFeature *F, RevRegFile *R, RevMem *M, RevInst Inst) {
  R->SetFP(Inst.rd, -std::fma(R->GetFP<T>(Inst.rs1), R->GetFP<T>(Inst.rs2), R->GetFP<T>(Inst.rs3)));
  R->AdvancePC(Inst);
  return true;
}

} // namespace SST:RevCPU

#endif
