//
// _RV64D_h_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_RV64D_H_
#define _SST_REVCPU_RV64D_H_

#include "../RevExt.h"
#include "../RevInstHelpers.h"

#include <cstring>
#include <vector>

namespace SST::RevCPU {

class RV64D : public RevExt {
  // Conversion from double to integer
  static constexpr auto& fcvtld  = float_or_posit<fcvtif<int64_t, double>, fcvtif<int64_t, posit64>>;
  static constexpr auto& fcvtlud = float_or_posit<fcvtif<uint64_t, double>, fcvtif<uint64_t, posit64>>;

  // Conversion from integer to double
  static constexpr auto& fcvtdl  = float_or_posit<fcvtfi<double, int64_t>, fcvtfi<posit64, int64_t>>;
  static constexpr auto& fcvtdlu = float_or_posit<fcvtfi<double, uint64_t>, fcvtfi<posit64, uint64_t>>;

  // Moves between FP and integer registers
  static constexpr auto& fmvxd   = float_or_posit<fmvif<double>, fmvif<posit64>>;
  static constexpr auto& fmvdx   = float_or_posit<fmvfi<double>, fmvfi<posit64>>;

  // ----------------------------------------------------------------------
  //
  // RISC-V RV64D Instructions
  //
  // ----------------------------------------------------------------------
  struct Rev64DInstDefaults : RevInstDefaults {
    Rev64DInstDefaults() {
      SetOpcode( 0b1010011 );
      Setrs2Class( RevRegClass::RegUNKNOWN );
      SetRaiseFPE( true );
    }
  };

  // clang-format off
  std::vector<RevInstEntry> RV64DTable = {
    Rev64DInstDefaults().SetMnemonic("fcvt.l.d %rd, %rs1" ).SetFunct2or7(0b1100001).SetImplFunc(fcvtld ).SetrdClass(RevRegClass::RegGPR  ).Setrs1Class(RevRegClass::RegFLOAT).Setrs2fcvtOp(0b10),
    Rev64DInstDefaults().SetMnemonic("fcvt.lu.d %rd, %rs1").SetFunct2or7(0b1100001).SetImplFunc(fcvtlud).SetrdClass(RevRegClass::RegGPR  ).Setrs1Class(RevRegClass::RegFLOAT).Setrs2fcvtOp(0b11),
    Rev64DInstDefaults().SetMnemonic("fcvt.d.l %rd, %rs1" ).SetFunct2or7(0b1101001).SetImplFunc(fcvtdl ).SetrdClass(RevRegClass::RegFLOAT).Setrs1Class(RevRegClass::RegGPR  ).Setrs2fcvtOp(0b10),
    Rev64DInstDefaults().SetMnemonic("fcvt.d.lu %rd, %rs1").SetFunct2or7(0b1101001).SetImplFunc(fcvtdlu).SetrdClass(RevRegClass::RegFLOAT).Setrs1Class(RevRegClass::RegGPR  ).Setrs2fcvtOp(0b11),
    Rev64DInstDefaults().SetMnemonic("fmv.x.d %rd, %rs1"  ).SetFunct2or7(0b1110001).SetImplFunc(fmvxd  ).SetrdClass(RevRegClass::RegGPR  ).Setrs1Class(RevRegClass::RegFLOAT).SetRaiseFPE(false),
    Rev64DInstDefaults().SetMnemonic("fmv.d.x %rd, %rs1"  ).SetFunct2or7(0b1111001).SetImplFunc(fmvdx  ).SetrdClass(RevRegClass::RegFLOAT).Setrs1Class(RevRegClass::RegGPR  ).SetRaiseFPE(false),
  };
  // clang-format on

public:
  /// RV364D: standard constructor
  RV64D( const RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) : RevExt( "RV64D", Feature, RevMem, Output ) {
    SetTable( std::move( RV64DTable ) );
  }
};  // end class RV32I

}  // namespace SST::RevCPU

#endif
