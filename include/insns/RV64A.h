//
// _RV64A_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_RV64A_H_
#define _SST_REVCPU_RV64A_H_

#include "../RevExt.h"
#include "../RevInstHelpers.h"

#include <vector>

namespace SST::RevCPU {

class RV64A : public RevExt {

  static bool
    lrd( RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    unsigned Zone     = 0x00;
    unsigned Precinct = 0x00;
    if( !M->isLocalAddr( R->GetX< uint64_t >( Inst.rs1 ), Zone, Precinct ) ) {
      // trigger the migration
      std::vector< uint64_t > P;
      P.push_back( R->GetPC() );
      for( unsigned i = 1; i < 32; i++ ) {
        P.push_back( R->GetX< uint64_t >( i ) );
      }
      for( unsigned i = 0; i < 32; i++ ) {
        uint64_t t = 0x00ull;
        double   s = R->DPF[i];
        memcpy( &t, &s, sizeof( t ) );
        P.push_back( t );
      }
      P.push_back( static_cast< uint64_t >( R->GetThreadID() ) );
      R->SetSCAUSE( RevExceptionCause::THREAD_MIGRATED );
      return M->ZOP_ThreadMigrate( F->GetHartToExecID(), P, Zone, Precinct );
    }
    MemReq req( R->RV64[Inst.rs1],
                Inst.rd,
                RevRegClass::RegGPR,
                F->GetHartToExecID(),
                MemOp::MemOpAMO,
                true,
                R->GetMarkLoadComplete() );
    R->LSQueue->insert( req.LSQHashPair() );
    M->LR( F->GetHartToExecID(),
           R->RV64[Inst.rs1],
           &R->RV64[Inst.rd],
           Inst.aq,
           Inst.rl,
           req,
           RevFlag::F_SEXT64 );
    R->AdvancePC( Inst );
    return true;
  }

  static bool
    scd( RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    unsigned Zone     = 0x00;
    unsigned Precinct = 0x00;
    if( !M->isLocalAddr( R->GetX< uint64_t >( Inst.rs1 ), Zone, Precinct ) ) {
      // trigger the migration
      std::vector< uint64_t > P;
      P.push_back( R->GetPC() );
      for( unsigned i = 1; i < 32; i++ ) {
        P.push_back( R->GetX< uint64_t >( i ) );
      }
      for( unsigned i = 0; i < 32; i++ ) {
        uint64_t t = 0x00ull;
        double   s = R->DPF[i];
        memcpy( &t, &s, sizeof( t ) );
        P.push_back( t );
      }
      P.push_back( static_cast< uint64_t >( R->GetThreadID() ) );
      R->SetSCAUSE( RevExceptionCause::THREAD_MIGRATED );
      return M->ZOP_ThreadMigrate( F->GetHartToExecID(), P, Zone, Precinct );
    }
    M->SC( F->GetHartToExecID(),
           R->RV64[Inst.rs1],
           &R->RV64[Inst.rs2],
           &R->RV64[Inst.rd],
           Inst.aq,
           Inst.rl,
           RevFlag::F_SEXT64 );
    R->AdvancePC( Inst );
    return true;
  }

  /// Atomic Memory Operations
  template< RevFlag F_AMO >
  static bool
    amooperd( RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    unsigned Zone     = 0x00;
    unsigned Precinct = 0x00;
    if( !M->isLocalAddr( R->GetX< uint64_t >( Inst.rs1 ), Zone, Precinct ) ) {
      // trigger the migration
      std::vector< uint64_t > P;
      P.push_back( R->GetPC() );
      for( unsigned i = 1; i < 32; i++ ) {
        P.push_back( R->GetX< uint64_t >( i ) );
      }
      for( unsigned i = 0; i < 32; i++ ) {
        uint64_t t = 0x00ull;
        double   s = R->DPF[i];
        memcpy( &t, &s, sizeof( t ) );
        P.push_back( t );
      }

      P.push_back( static_cast< uint64_t >( R->GetThreadID() ) );
      R->SetSCAUSE( RevExceptionCause::THREAD_MIGRATED );
      return M->ZOP_ThreadMigrate( F->GetHartToExecID(), P, Zone, Precinct );
    }
    uint32_t flags = static_cast< uint32_t >( F_AMO );

    if( Inst.aq && Inst.rl ) {
      flags |= uint32_t( RevFlag::F_AQ ) | uint32_t( RevFlag::F_RL );
    } else if( Inst.aq ) {
      flags |= uint32_t( RevFlag::F_AQ );
    } else if( Inst.rl ) {
      flags |= uint32_t( RevFlag::F_RL );
    }

    MemReq req( R->RV64[Inst.rs1],
                Inst.rd,
                RevRegClass::RegGPR,
                F->GetHartToExecID(),
                MemOp::MemOpAMO,
                true,
                R->GetMarkLoadComplete() );
    R->LSQueue->insert( req.LSQHashPair() );
    M->AMOVal( F->GetHartToExecID(),
               R->RV64[Inst.rs1],
               &R->RV64[Inst.rs2],
               &R->RV64[Inst.rd],
               req,
               RevFlag{ flags } );

    R->AdvancePC( Inst );

    // update the cost
    R->cost += M->RandCost( F->GetMinCost(), F->GetMaxCost() );
    return true;
  }

  static constexpr auto& amoaddd  = amooperd< RevFlag::F_AMOADD >;
  static constexpr auto& amoswapd = amooperd< RevFlag::F_AMOSWAP >;
  static constexpr auto& amoxord  = amooperd< RevFlag::F_AMOXOR >;
  static constexpr auto& amoandd  = amooperd< RevFlag::F_AMOAND >;
  static constexpr auto& amoord   = amooperd< RevFlag::F_AMOOR >;
  static constexpr auto& amomind  = amooperd< RevFlag::F_AMOMIN >;
  static constexpr auto& amomaxd  = amooperd< RevFlag::F_AMOMAX >;
  static constexpr auto& amominud = amooperd< RevFlag::F_AMOMINU >;
  static constexpr auto& amomaxud = amooperd< RevFlag::F_AMOMAXU >;

  // ----------------------------------------------------------------------
  //
  // RISC-V RV64A Instructions
  //
  // ----------------------------------------------------------------------

  struct Rev64AInstDefaults : RevInstDefaults {
    Rev64AInstDefaults() {
      SetOpcode( 0b0101111 );
      SetFunct3( 0b011 );
    }
  };

  // clang-format off
  std::vector<RevInstEntry> RV64ATable = {
    { Rev64AInstDefaults().SetMnemonic("lr.d %rd, (%rs1)"         ).SetFunct2or7(0b0000010).SetImplFunc(lrd     ).Setrs2Class(RevRegClass::RegUNKNOWN) },
    { Rev64AInstDefaults().SetMnemonic("sc.d %rd, %rs1, %rs2"     ).SetFunct2or7(0b0000011).SetImplFunc(scd     ) },
    { Rev64AInstDefaults().SetMnemonic("amoswap.d %rd, %rs1, %rs2").SetFunct2or7(0b0000001).SetImplFunc(amoswapd) },
    { Rev64AInstDefaults().SetMnemonic("amoadd.w %rd, %rs1, %rs2" ).SetFunct2or7(0b0000000).SetImplFunc(amoaddd ) },
    { Rev64AInstDefaults().SetMnemonic("amoxor.w %rd, %rs1, %rs2" ).SetFunct2or7(0b0000100).SetImplFunc(amoxord ) },
    { Rev64AInstDefaults().SetMnemonic("amoand.w %rd, %rs1, %rs2" ).SetFunct2or7(0b0001100).SetImplFunc(amoandd ) },
    { Rev64AInstDefaults().SetMnemonic("amoor.w %rd, %rs1, %rs2"  ).SetFunct2or7(0b0001000).SetImplFunc(amoord  ) },
    { Rev64AInstDefaults().SetMnemonic("amomin.w %rd, %rs1, %rs2" ).SetFunct2or7(0b0010000).SetImplFunc(amomind ) },
    { Rev64AInstDefaults().SetMnemonic("amomax.w %rd, %rs1, %rs2" ).SetFunct2or7(0b0010100).SetImplFunc(amomaxd ) },
    { Rev64AInstDefaults().SetMnemonic("amominu.w %rd, %rs1, %rs2").SetFunct2or7(0b0011000).SetImplFunc(amominud) },
    { Rev64AInstDefaults().SetMnemonic("amomaxu.w %rd, %rs1, %rs2").SetFunct2or7(0b0011100).SetImplFunc(amomaxud) },
  };
  // clang-format on

public:
  /// RV64A: standard constructor
  RV64A( RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) :
    RevExt( "RV64A", Feature, RevMem, Output ) {
    SetTable( std::move( RV64ATable ) );
  }
};  // end class RV64A

}  // namespace SST::RevCPU

#endif
