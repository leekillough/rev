//
// _RV64F_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_RV64F_H_
#define _SST_REVCPU_RV64F_H_

#include "../RevInstHelpers.h"
#include "../RevExt.h"

#include <vector>
#include <limits>

namespace SST::RevCPU{

class RV64F : public RevExt {
  static constexpr auto& fcvtls  = CvtFpToInt<float,  int64_t>;
  static constexpr auto& fcvtlus = CvtFpToInt<float, uint64_t>;

  static bool fcvtsl(RevFeature *F, RevRegFile *R, RevMem *M, const RevInst& Inst) {
    R->SetFP(Inst.rd, static_cast<float>(R->GetX<int64_t>(Inst.rs1)));
    R->AdvancePC(Inst);
    return true;
  }

  static bool fcvtslu(RevFeature *F, RevRegFile *R, RevMem *M, const RevInst& Inst) {
    R->SetFP(Inst.rd, static_cast<float>(R->GetX<uint64_t>(Inst.rs1)));
    R->AdvancePC(Inst);
    return true;
  }

  // ----------------------------------------------------------------------
  //
  // RISC-V RV64F Instructions
  //
  // Format:
  // <mnemonic> <cost> <opcode> <funct3> <funct7> <rdClass> <rs1Class>
  //            <rs2Class> <rs3Class> <format> <func> <nullEntry>
  // ----------------------------------------------------------------------
  struct Rev64FInstDefaults : RevInstDefaults {
    static constexpr uint8_t     opcode   = 0b1010011;
  };

  std::vector<RevInstEntry> RV64FTable = {
    {RevInstEntryBuilder<Rev64FInstDefaults>().SetMnemonic("fcvt.l.s  %rd, %rs1").SetFunct2or7( 0b1100000).SetfpcvtOp(0b10).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegFLOAT).Setrs2Class(RevRegClass::RegUNKNOWN).SetImplFunc(&fcvtls ).InstEntry},
    {RevInstEntryBuilder<Rev64FInstDefaults>().SetMnemonic("fcvt.lu.s %rd, %rs1").SetFunct2or7( 0b1100000).SetfpcvtOp(0b11).SetrdClass(RevRegClass::RegGPR).Setrs1Class(RevRegClass::RegFLOAT).Setrs2Class(RevRegClass::RegUNKNOWN).SetImplFunc(&fcvtlus ).InstEntry},
    {RevInstEntryBuilder<Rev64FInstDefaults>().SetMnemonic("fcvt.s.l %rd, %rs1" ).SetFunct2or7( 0b1101000).SetfpcvtOp(0b10).SetrdClass(RevRegClass::RegFLOAT).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN).SetImplFunc(&fcvtsl ).InstEntry},
    {RevInstEntryBuilder<Rev64FInstDefaults>().SetMnemonic("fcvt.s.lu %rd, %rs1").SetFunct2or7( 0b1101000).SetfpcvtOp(0b11).SetrdClass(RevRegClass::RegFLOAT).Setrs1Class(RevRegClass::RegGPR).Setrs2Class(RevRegClass::RegUNKNOWN).SetImplFunc(&fcvtslu ) .InstEntry},
  };

public:
  /// RV364F: standard constructor
  RV64F( RevFeature *Feature,
         RevMem *RevMem,
         SST::Output *Output )
    : RevExt( "RV64F", Feature, RevMem, Output) {
    SetTable(std::move(RV64FTable));
  }
}; // end class RV64F

} // namespace SST::RevCPU

#endif
