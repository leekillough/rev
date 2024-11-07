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

namespace SST::RevCPU {

class RVVec : public RevExt {

  static bool vsetivli( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vsetivli: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vsetvli( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vsetvli: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vsetvl( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vsetvl: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vadd( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vadd: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vs( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vs: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  static bool vl( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    std::cout << "vl: no need to go further\n" << std::endl;
    assert( false );
    return true;
  }

  // clang-format off
  std::vector<RevInstEntry> RVVTable = {

  // Use lower 4 bits of Func3 to differentiate vector instructions
  //   For all OP* types, Func3 = inst[14:12]
  //   For LOAD-FP and STORE-FP, Func3 = 0x8

  // Func3=0x0: OPIVV
  RevInstDefaults().SetMnemonic("vadd.vv %vd, %vs2, %vs2, %vm" ).SetFunct3(0x0).SetImplFunc(&vadd    ).SetrdClass(RevRegClass::RegGPR)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111),

  // Func3=0x1: OPFVV
  // Func3=0x2: OPMVV
  // Func3=0x3: OPIVI
  // Func3=0x4: OPIVX
  // Func3=0x5: OPFVF
  // Func3=0x6: OPMVX

  // Func3=0x7: Formats for Vector Configuration Instructions under OP-V major opcode.
  RevInstDefaults().SetMnemonic("vsetvli %rd, %rs1, %zimm11"   ).SetFunct3(0x7).SetImplFunc(&vsetvli ).SetrdClass(RevRegClass::RegGPR)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 31) & 0x1)  == 0; } ),
  RevInstDefaults().SetMnemonic("vsetivli %rd, %zimm, %zimm10" ).SetFunct3(0x7).SetImplFunc(&vsetivli).SetrdClass(RevRegClass::RegGPR)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 30) & 0x3)  == 0xb11; } ),
  RevInstDefaults().SetMnemonic("vsetvl %rd, %rs1, %rs2"       ).SetFunct3(0x7).SetImplFunc(&vsetvl  ).SetrdClass(RevRegClass::RegGPR)  .SetFormat(RVVTypeOp).SetOpcode(0b1010111).SetPredicate( []( uint32_t Inst ){ return ((Inst >> 25) & 0x7f) == 0xb1000000; } ),

  // Func3=0x8: Vector Loads and Stores
  RevInstDefaults().SetMnemonic("vle32.v %vd, (%rs1), %vm"  ).SetFunct3(0x8).SetImplFunc(&vl    ).Setrs1Class(RevRegClass::RegGPR)  .SetFormat(RVVTypeLd).SetOpcode(0b0000111),
  RevInstDefaults().SetMnemonic("vse32.v %vs3, (%rs1), %vm" ).SetFunct3(0x8).SetImplFunc(&vs    ).Setrs1Class(RevRegClass::RegGPR)  .SetFormat(RVVTypeSt).SetOpcode(0b0100111),

  };
  // clang-format on

public:
  /// RVVec: standard constructor
  /// TODO Feature, RevMem not needed if we assume coprocessor is fixed ISA (may not be)
  RVVec( const RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) : RevExt( "RVV", Feature, RevMem, Output ) {
    SetTable( std::move( RVVTable ) );
  }

};  //class RVVec

}  //namespace SST::RevCPU

#endif  //_SST_REVCPU_RVVEC_H_
