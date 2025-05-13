//
// _XForza_h_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_XFORZA_H_
#define _SST_REVCPU_XFORZA_H_

#include "../RevExt.h"
#include "../RevInstHelpers.h"

namespace SST::RevCPU {

class XForza : public RevExt {

  // ------------------------------------------------------------
  // Thread Instructions
  // ------------------------------------------------------------
  static bool spawn( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    // -- RS1 = initial program counter for the newly created (Child) thread
    // -- RS2 = is copied into register X31 of the child and is expected to
    //          contain a pointer to an arbitrary block of data in memory to
    //          be imported by the Child.

    // If the FSM is not active, insert a new starting state into the ThreadQ
    if( !M->ThreadQIsActive(F->GetHartToExecID()) ){
      // I believe a Fence is required.  Remove this call if it's not
      M->FenceMem( F->GetHartToExecID() );
      M->ThreadQInsert(F->GetHartToExecID(),
                       R->GetX<uint64_t>( Inst.rs1 ),
                       R->GetX<uint64_t>( Inst.rs2 ));
    }

    // If the FSM is complete, advance the PC.  Otherwise, the PC should remain as is
    // This will stall the pipeline of the calling Hart
    if( M->ThreadQIsComplete(F->GetHartToExecID()) ){
      R->cost += M->RandCost( F->GetMinCost(), F->GetMaxCost() );
      R->AdvancePC( Inst );
    }

    return true;
  }

  static bool quit( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    M->FenceMem( F->GetHartToExecID() );
    
    M->IssueThreadQuit( F->GetHartToExecID() );
    return true;
  }

  static bool resched( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    // update the cost
    M->FenceMem( F->GetHartToExecID() );
    R->AdvancePC( Inst );
    return true;
  }

  // ------------------------------------------------------------
  // Remote Atomics
  // ------------------------------------------------------------
  template<typename TYPE, RevFlag Op, RevFlag Rtn>
  static bool forzaamo_rem( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    RevFlag flags{ Op };
    RevFlagSet( flags, Rtn );
    RevFlagSet( flags, std::is_floating_point_v<TYPE> ? RevFlag::F_BOXNAN : RevFlag::F_SEXT64 );
    RevFlagSet( flags, RevFlag::F_RL );

    MemReq req(
      R->RV64[Inst.rs1], Inst.rd, RevRegClass::RegGPR, F->GetHartToExecID(), MemOp::MemOpREAD, true, R->GetMarkLoadComplete()
    );
    R->LSQueue->insert( req.LSQHashPair() );
    M->AMOVal(
      F->GetHartToExecID(),
      R->RV64[Inst.rs1],
      reinterpret_cast<TYPE*>( &R->RV64[Inst.rs2] ),
      reinterpret_cast<TYPE*>( &R->RV64[Inst.rd] ),
      req,
      flags
    );

    // update the cost
    R->cost += M->RandCost( F->GetMinCost(), F->GetMaxCost() );
    R->AdvancePC( Inst );
    return true;
  }

  // ------------------------------------------------------------
  // Migrating Atomics
  // ------------------------------------------------------------
  template<typename TYPE, RevFlag Op, RevFlag Rtn>
  static bool forzaamo_migr( const RevFeature* F, RevRegFile* R, RevMem* M, const RevInst& Inst ) {
    RevFlag flags{ Op };
    RevFlagSet( flags, Rtn );
    RevFlagSet( flags, std::is_floating_point_v<TYPE> ? RevFlag::F_BOXNAN : RevFlag::F_SEXT64 );
    RevFlagSet( flags, RevFlag::F_RL );

    MemReq req(
      R->RV64[Inst.rs1], Inst.rd, RevRegClass::RegGPR, F->GetHartToExecID(), MemOp::MemOpREAD, true, R->GetMarkLoadComplete()
    );
    R->LSQueue->insert( req.LSQHashPair() );
    M->AMOVal(
      F->GetHartToExecID(),
      R->RV64[Inst.rs1],
      reinterpret_cast<TYPE*>( &R->RV64[Inst.rs2] ),
      reinterpret_cast<TYPE*>( &R->RV64[Inst.rd] ),
      req,
      flags
    );

    // update the cost
    R->cost += M->RandCost( F->GetMinCost(), F->GetMaxCost() );
    R->AdvancePC( Inst );
    return true;
  }

  // XForza Encoding Notes
  // All the Forza atomic instructions implemented in the RZA have four
  // different modes of operation.  Most instruction mnemonics represent
  // the mode formats as follows:
  //
  //    amo_r_[OP][SIZE].FMT
  //
  // where, FMT is encoded as one of the following values:
  // [Mnemonic]   [Decimal Value]  [Implementation]
  // - U              0          update memory; ACK returned (Atomic Update)
  // - MIGR_NN        1          migrate && rd = OP([rs2], M[rs1]); M[rs1] = OP([rs2], M[rs1])
  // - MIGR_ON        2          migrate && rd = OP([rs2], M[rs1]);   M[rs1] unchanged
  // - MIGR_NO        3          migrate && rd = M[rs1]; M[rs1] = OP([rs2], M[rs1])
  // - REM_NN         5          remotely execute: rd = OP([rs2], M[rs1]); M[rs1] = OP([rs2], M[rs1])
  // - REM_ON         6          remotely execute: rd = OP([rs2], M[rs1]);   M[rs1] unchanged
  // - REM_NO         7          remotely execute: rd = M[rs1]; M[rs1] = OP([rs2], M[rs1])

  // clang-format off
#define FORZA_AMO_FUNC( name, op )                                                                   \
  static constexpr auto& amo_r_##name##8u        = forzaamo_rem<uint8_t,   op, RevFlag::F_NONE>;     \
  static constexpr auto& amo_r_##name##16u       = forzaamo_rem<uint16_t,  op, RevFlag::F_NONE>;     \
  static constexpr auto& amo_r_##name##32u       = forzaamo_rem<uint32_t,  op, RevFlag::F_NONE>;     \
  static constexpr auto& amo_r_##name##64u       = forzaamo_rem<uint64_t,  op, RevFlag::F_NONE>;     \
  static constexpr auto& amo_r_##name##8migr_nn  = forzaamo_migr<uint8_t,  op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##16migr_nn = forzaamo_migr<uint16_t, op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##32migr_nn = forzaamo_migr<uint32_t, op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##64migr_nn = forzaamo_migr<uint64_t, op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##8migr_on  = forzaamo_migr<uint8_t,  op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##16migr_on = forzaamo_migr<uint16_t, op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##32migr_on = forzaamo_migr<uint32_t, op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##64migr_on = forzaamo_migr<uint64_t, op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##8migr_no  = forzaamo_migr<uint8_t,  op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##16migr_no = forzaamo_migr<uint16_t, op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##32migr_no = forzaamo_migr<uint32_t, op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##64migr_no = forzaamo_migr<uint64_t, op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##8rem_nn   = forzaamo_rem<uint8_t,   op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##16rem_nn  = forzaamo_rem<uint16_t,  op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##32rem_nn  = forzaamo_rem<uint32_t,  op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##64rem_nn  = forzaamo_rem<uint64_t,  op, RevFlag::F_AMONN>;  \
  static constexpr auto& amo_r_##name##8rem_on   = forzaamo_rem<uint8_t,   op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##16rem_on  = forzaamo_rem<uint16_t,  op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##32rem_on  = forzaamo_rem<uint32_t,  op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##64rem_on  = forzaamo_rem<uint64_t,  op, RevFlag::F_AMOON>;  \
  static constexpr auto& amo_r_##name##8rem_no   = forzaamo_rem<uint8_t,   op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##16rem_no  = forzaamo_rem<uint16_t,  op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##32rem_no  = forzaamo_rem<uint32_t,  op, RevFlag::F_AMONO>;  \
  static constexpr auto& amo_r_##name##64rem_no  = forzaamo_rem<uint64_t,  op, RevFlag::F_AMONO>;

  FORZA_AMO_FUNC( add,  RevFlag::F_AMOADD    )
  FORZA_AMO_FUNC( sub,  RevFlag::F_AMOSUB  )
  FORZA_AMO_FUNC( and,  RevFlag::F_AMOAND    )
  FORZA_AMO_FUNC( or,   RevFlag::F_AMOOR     )
  FORZA_AMO_FUNC( xor,  RevFlag::F_AMOXOR    )
  FORZA_AMO_FUNC( smax, RevFlag::F_AMOMAX    )
  FORZA_AMO_FUNC( umax, RevFlag::F_AMOMAXU   )
  FORZA_AMO_FUNC( smin, RevFlag::F_AMOMIN    )
  FORZA_AMO_FUNC( umin, RevFlag::F_AMOMINU   )
  FORZA_AMO_FUNC( swap, RevFlag::F_AMOSWAP   )
  FORZA_AMO_FUNC( thrs, RevFlag::F_AMOTHRES )

  // clang-format on

  // ----------------------------------------------------------------------
  //
  // RISC-V Forza Instructions
  //
  // ----------------------------------------------------------------------
  struct XForzaInstDefaults : RevInstDefaults {
    XForzaInstDefaults() {
      SetOpcode( 0b0001011 );
      SetrdClass( RevRegClass::RegGPR );
      Setrs1Class( RevRegClass::RegGPR );
      Setrs2Class( RevRegClass::RegGPR );
    }
  };

  struct XForzaInstRMACDefaults : RevInstDefaults {
    XForzaInstRMACDefaults() { SetOpcode( 0b0111111 ); }
  };

  struct XForzaInstMCPYDefaults : RevInstDefaults {
    XForzaInstMCPYDefaults() { SetOpcode( 0b1101011 ); }
  };

  struct XForzaInstThreadDefaults : RevInstDefaults {
    XForzaInstThreadDefaults() {
      SetOpcode( 0b1110111 );
      SetrdClass( RevRegClass::RegGPR );
      Setrs1Class( RevRegClass::RegGPR );
      Setrs2Class( RevRegClass::RegGPR );
    }
  };

  struct XForzaInstThreadSpcDefaults : RevInstDefaults {
    XForzaInstThreadSpcDefaults() { SetOpcode( 0b0101011 ); }
  };

#define FORZA_AMO_MACRO( name, u4 )                      \
  XForzaInstDefaults()                                   \
    .SetMnemonic( #name "8.u %rs1, %rs2" )               \
    .SetFunct3( 0b011 )                                  \
    .SetFunct2or7( 0b##u4##000 )                         \
    .SetImplFunc( name##8u ),                            \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.u %rs1, %rs2" )            \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##000 )                       \
      .SetImplFunc( name##16u ),                         \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.u %rs1, %rs2" )            \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##000 )                       \
      .SetImplFunc( name##32u ),                         \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.u %rs1, %rs2" )            \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##000 )                       \
      .SetImplFunc( name##64u ),                         \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.migr_nn %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##001 )                       \
      .SetImplFunc( name##8migr_nn ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.migr_nn %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##001 )                       \
      .SetImplFunc( name##16migr_nn ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.migr_nn %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##001 )                       \
      .SetImplFunc( name##32migr_nn ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.migr_nn %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##001 )                       \
      .SetImplFunc( name##64migr_nn ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.migr_on %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##010 )                       \
      .SetImplFunc( name##8migr_on ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.migr_on %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##010 )                       \
      .SetImplFunc( name##16migr_on ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.migr_on %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##010 )                       \
      .SetImplFunc( name##32migr_on ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.migr_on %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##010 )                       \
      .SetImplFunc( name##64migr_on ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.migr_no %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##011 )                       \
      .SetImplFunc( name##8migr_no ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.migr_no %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##011 )                       \
      .SetImplFunc( name##16migr_no ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.migr_no %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##011 )                       \
      .SetImplFunc( name##32migr_no ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.migr_no %rd, %rs1, %rs2" ) \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##011 )                       \
      .SetImplFunc( name##64migr_no ),                   \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.rem_nn %rd, %rs1, %rs2" )   \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##101 )                       \
      .SetImplFunc( name##8rem_nn ),                     \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.rem_nn %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##101 )                       \
      .SetImplFunc( name##16rem_nn ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.rem_nn %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##101 )                       \
      .SetImplFunc( name##32rem_nn ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.rem_nn %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##101 )                       \
      .SetImplFunc( name##64rem_nn ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.rem_on %rd, %rs1, %rs2" )   \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##110 )                       \
      .SetImplFunc( name##8rem_on ),                     \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.rem_on %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##110 )                       \
      .SetImplFunc( name##16rem_on ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.rem_on %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##110 )                       \
      .SetImplFunc( name##32rem_on ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.rem_on %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##110 )                       \
      .SetImplFunc( name##64rem_on ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "8.rem_no %rd, %rs1, %rs2" )   \
      .SetFunct3( 0b011 )                                \
      .SetFunct2or7( 0b##u4##111 )                       \
      .SetImplFunc( name##8rem_no ),                     \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "16.rem_no %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b010 )                                \
      .SetFunct2or7( 0b##u4##111 )                       \
      .SetImplFunc( name##16rem_no ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "32.rem_no %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b000 )                                \
      .SetFunct2or7( 0b##u4##111 )                       \
      .SetImplFunc( name##32rem_no ),                    \
    XForzaInstDefaults()                                 \
      .SetMnemonic( #name "64.rem_no %rd, %rs1, %rs2" )  \
      .SetFunct3( 0b001 )                                \
      .SetFunct2or7( 0b##u4##111 )                       \
      .SetImplFunc( name##64rem_no ),

  // clang-format off
  std::vector<RevInstEntry> XForzaTable = {
    FORZA_AMO_MACRO( amo_r_add,  0000 )
    FORZA_AMO_MACRO( amo_r_sub,  0001 )
    FORZA_AMO_MACRO( amo_r_and,  0010 )
    FORZA_AMO_MACRO( amo_r_or,   0011 )
    FORZA_AMO_MACRO( amo_r_xor,  0100 )
    FORZA_AMO_MACRO( amo_r_smax, 0101 )
    FORZA_AMO_MACRO( amo_r_umax, 0110 )
    FORZA_AMO_MACRO( amo_r_smin, 0111 )
    FORZA_AMO_MACRO( amo_r_umin, 1000 )
    FORZA_AMO_MACRO( amo_r_swap, 1001 )
    FORZA_AMO_MACRO( amo_r_thrs, 1110 )

    XForzaInstThreadDefaults().SetMnemonic( "spawn %rd, %rs1, %rs2" ).SetFunct3( 0b000 ).SetFunct2or7( 0b0000000000 ).SetImplFunc( spawn ),
    XForzaInstThreadSpcDefaults().SetMnemonic( "quit" ).SetFunct3( 0b010 ).SetImplFunc( quit ),
    XForzaInstThreadSpcDefaults().SetMnemonic( "resched" ).SetFunct3( 0b011 ).SetImplFunc( resched ),
  };
  // clang-format on

public:
  XForza( const RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) : RevExt( "XForza", Feature, RevMem, Output ) {
    SetTable( std::move( XForzaTable ) );
  }

};  // end class XForza

}  // namespace SST::RevCPU

#endif
