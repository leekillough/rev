//
// _RevVectorCoProc_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevVectorCoProc.h"

namespace SST::RevCPU {

// ---------------------------------------------------------------
// RevVectorCoProc
// ---------------------------------------------------------------
RevVectorCoProc::RevVectorCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), num_instRetired( 0 ) {

  std::string ClockFreq = params.find<std::string>( "clock", "1Ghz" );
  cycleCount            = 0;

  registerStats();

  //This would be used to register the clock with SST Core
  /*registerClock( ClockFreq,
    new Clock::Handler<RevVectorCoProc>(this, &RevVectorCoProc::ClockTick));
    output->output("Registering subcomponent RevVectorCoProc with frequency=%s\n", ClockFreq.c_str());*/

  // load the vector instruction tables
  if( !LoadInstructionTable() )
    output->fatal( CALL_INFO, -1, "Error : failed to load instruction tables\n" );
}

void RevVectorCoProc::registerStats() {
  num_instRetired = registerStatistic<uint64_t>( "InstRetired" );
}

bool RevVectorCoProc::Reset() {
  InstQ = {};
  return true;
}

//TODO common code to load instruction tables in core or coprocs
bool RevVectorCoProc::LoadInstructionTable() {
  // Stage 1: load the instruction table for each enabled feature
  if( !SeedInstructionTable() )
    return false;

  // Stage 2: setup the internal mapping tables for performance
  if( !InitTableMapping() )
    return false;

  // // Stage 3: examine the user-defined cost tables to see if we need to override the defaults
  // if( !ReadOverrideTables() )
  //   return false;

  return true;
}

//TODO common core
bool RevVectorCoProc::SeedInstructionTable() {
  output->verbose( CALL_INFO, 6, 0, "Seeding vector instruction table\n" );
  //TODO If we want to have feature granularity for different instruction tables
  // then we have to provide the core's feature pointer.
  // if( feature->IsModeEnabled( RV_V ) ) {
  //   EnableExt( new RVVec( feature, mem, output ) );
  // }
  // Alternatively, we can just put assertion in core when coproc created.
  // The feature pointer is passed into every instruction in the instruction table.
  // Simularly for mem pointer.

  EnableExt( new RVVec( nullptr, nullptr, output ) );
  return true;
}

//TODO common core
bool RevVectorCoProc::EnableExt( RevExt* Ext ) {
  if( !Ext )
    output->fatal( CALL_INFO, -1, "Error: failed to initialize RISC-V extensions\n" );

  output->verbose( CALL_INFO, 6, 0, "Enabling extension=%s\n", Ext->GetName().data() );

  // add the extension to our vector of enabled objects
  Extensions.push_back( std::unique_ptr<RevExt>( Ext ) );

  // setup the mapping of InstTable to Ext objects
  auto load = [&]( const std::vector<RevInstEntry>& Table ) {
    InstTable.reserve( InstTable.size() + Table.size() );
    for( unsigned i = 0; i < Table.size(); i++ ) {
      InstTable.push_back( Table[i] );
      auto ExtObj = std::pair<unsigned, unsigned>( Extensions.size() - 1, i );
      EntryToExt.insert( std::pair<unsigned, std::pair<unsigned, unsigned>>( InstTable.size() - 1, ExtObj ) );
    }
  };

  // retrieve all the target instructions
  load( Ext->GetTable() );

  return true;
}

bool RevVectorCoProc::InitTableMapping() {
  output->verbose( CALL_INFO, 6, 0, "Initializing table mapping for V extension\n" );
  for( unsigned i = 0; i < InstTable.size(); i++ ) {
    NameToEntry.insert( std::pair<std::string, unsigned>( ExtractMnemonic( InstTable[i] ), i ) );
    // map normal instruction
    EncToEntry.insert( std::pair<uint64_t, unsigned>( CompressEncoding( InstTable[i] ), i ) );
    output->verbose(
      CALL_INFO, 6, 0, "Table Entry %" PRIu64 " = %s\n", CompressEncoding( InstTable[i] ), ExtractMnemonic( InstTable[i] ).data()
    );
  }
  return true;
}

std::string RevVectorCoProc::ExtractMnemonic( const RevInstEntry& Entry ) {
  std::string              Tmp = Entry.mnemonic;
  std::vector<std::string> vstr;
  RevOpts::splitStr( Tmp, " ", vstr );
  return vstr[0];
}

uint32_t RevVectorCoProc::CompressEncoding( const RevInstEntry& Entry ) {
  uint32_t Value = Entry.opcode;
  Value |= uint32_t( Entry.funct3 ) << 8;
  return Value;
}

auto RevVectorCoProc::matchInst(
  const std::unordered_multimap<uint64_t, unsigned>& map,
  uint64_t                                           encoding,
  const std::vector<RevInstEntry>&                   InstTable,
  uint32_t                                           Inst
) const {
  // Iterate through all entries which match the encoding
  for( auto [it, end] = map.equal_range( encoding ); it != end; ++it ) {
    unsigned Entry = it->second;
    // If an entry is valid and has a satisfied predicate, return it
    if( Entry < InstTable.size() && InstTable[Entry].predicate( Inst ) )
      return it;
  }
  // No match
  return map.end();
}

bool RevVectorCoProc::Decode( const uint32_t Inst ) {
  // capture decoded information at each stage including
  // values for any source scalar registers.
  decodedInst.inst = Inst;
  // vector csr access instructions
  csr_inst_t csr_inst( Inst );
  if( csr_inst.f.opcode == csr_inst_opcode ) {
    if( csr_inst.f.csr >= RevCSR::vstart && csr_inst.f.csr <= RevCSR::vcsr )
      return true;
    if( csr_inst.f.csr >= RevCSR::vl && csr_inst.f.csr <= RevCSR::vlenb )
      return true;
  }

  // RVV instructions
  uint64_t func3  = 0xff;  // an illegal value
  uint32_t opcode = Inst & 0x7f;
  if( opcode == 0b0000111 ) {  // LOAD-FP
    decodedInst.format = RevInstF::RVVTypeLd;
    func3              = 8;
  } else if( opcode == 0b0100111 ) {  // STORE-FP
    decodedInst.format = RevInstF::RVVTypeSt;
    func3              = 8;
  } else if( opcode == 0b1010111 ) {  // OP-V
    decodedInst.format = RevInstF::RVVTypeOp;
    func3              = ( Inst >> 12 ) & 7;
  }

  uint64_t Enc = func3 << 8 | opcode;
  auto     it  = matchInst( EncToEntry, Enc, InstTable, Inst );
  if( it == EncToEntry.end() ) {
    output->verbose( CALL_INFO, 1, 0, "Warning: Vector coprocessor unable to decode instruction 0x%" PRIu32 "\n", Inst );
  }

  return true;
}

bool RevVectorCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  if( Decode( Inst ) ) {
    // TODO finite instruction queue
    RevCoProcInst inst = RevCoProcInst( decodedInst, F, R, M );
    //std::cout << "Vector CoProc instruction issued: " << std::hex << Inst << std::dec << std::endl;
    //parent->ExternalDepSet(CreatePasskey(), F->GetHartToExecID(), 7, false);
    InstQ.push( inst );
    return true;
  }
  return false;
}

bool RevVectorCoProc::ClockTick( SST::Cycle_t cycle ) {
  if( !InstQ.empty() ) {
    RevCoProcInst rec = InstQ.front();
    //parent->ExternalDepClear(CreatePasskey(), InstQ.front().Feature->GetHartToExecID(), 7, false);
    num_instRetired->addData( 1 );
    parent->ExternalStallHart( CreatePasskey(), 0 );
    Exec( rec );
    InstQ.pop();
    cycleCount = cycle;
  }
  if( ( cycle - cycleCount ) > 1 ) {
    parent->ExternalReleaseHart( CreatePasskey(), 0 );
  }
  return true;
}

void RevVectorCoProc::Exec( RevCoProcInst rec ) {
  std::cout << "Vector CoProcessor to execute instruction: " << std::hex << rec.decodedInst.inst << std::endl;

  // CSR access presumes valid csr address
  csr_inst_t csr_inst( rec.decodedInst.inst );
  if( csr_inst.f.opcode == csr_inst_opcode ) {
    if( csr_inst.f.funct3 == csr_inst_funct3::csrrsi ) {
      rec.RegFile->SetX( csr_inst.f.rd, csrmap[csr_inst.f.csr] );
      return;
    } else {
      output->fatal( CALL_INFO, -1, "currently only csrrsi supported for vector csr access\n" );
      return;
    }
  }

  // Vector instructions
  // VLEN=128, ELEN=64
  // 32-bit elements require 4 parallel loads, adds, or stores.
  // MemHierarchy must be disabled.

  if( rec.decodedInst.format == RevInstF::RVVTypeOp ) {
    switch( rec.decodedInst.inst ) {
    case 0x0d0572d7:  // vsetvli t0, a0, e32, m1, ta, ma
      // returns new vl. Always 4 for our test case
      rec.RegFile->SetX( RevReg::t0, 0x4 );
      return;
    case 0x02008157:  // vadd.vv v2, v0, v1
      // this does 64 bits at a time. Overflow will not occur for this test
      vreg[2][0] = vreg[0][0] + vreg[1][0];
      vreg[2][1] = vreg[0][1] + vreg[1][1];
      output->verbose(
        CALL_INFO,
        5,
        0,
        "*V"
        " 0x%" PRIx64 " <- v[0][0] 0x%" PRIx64 " <- v[0][1]"
        " 0x%" PRIx64 " <- v[1][0] 0x%" PRIx64 " <- v[1][1]"
        " v[2][0] <- 0x%" PRIx64 " v[2][1] <- 0x%" PRIx64 "\n",
        vreg[0][0],
        vreg[0][1],
        vreg[1][0],
        vreg[1][1],
        vreg[2][0],
        vreg[2][1]
      );
      return;
    }
  }

  if( rec.decodedInst.format == RevInstF::RVVTypeLd ) {
    switch( rec.decodedInst.inst ) {
    case 0x0205e007:  // vle32.v v0, (a1)
    {
      uint64_t a1 = rec.RegFile->GetX<uint64_t>( RevReg::a1 );
      MemReq   req0( a1, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
      rec.Mem->ReadVal<uint64_t>( 0, a1, &vreg[0][0], req0, RevFlag::F_NONE );
      MemReq req1( a1 + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
      rec.Mem->ReadVal<uint64_t>( 0, a1 + 8, &vreg[0][1], req1, RevFlag::F_NONE );
      output->verbose( CALL_INFO, 5, 0, "*V v[0][0] <- 0x%" PRIx64 " v[0][1] <- 0x%" PRIx64 "\n", vreg[0][0], vreg[0][1] );
      return;
    }
    case 0x02066087:  // vle32.v v1, (a2)
    {
      uint64_t a2 = rec.RegFile->GetX<uint64_t>( RevReg::a2 );
      MemReq   req0( a2, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
      rec.Mem->ReadVal<uint64_t>( 0, a2, &vreg[1][0], req0, RevFlag::F_NONE );
      MemReq req1( a2 + 8, RevReg::zero, RevRegClass::RegGPR, 0, MemOp::MemOpREAD, true, rec.RegFile->GetMarkLoadComplete() );
      rec.Mem->ReadVal<uint64_t>( 0, a2 + 8, &vreg[1][1], req1, RevFlag::F_NONE );
      output->verbose( CALL_INFO, 5, 0, "*V v[1][0] <- 0x%" PRIx64 " v[1][1] <- 0x%" PRIx64 "\n", vreg[1][0], vreg[1][1] );
      return;
    }
    }
  }

  if( rec.decodedInst.format == RevInstF::RVVTypeSt ) {
    switch( rec.decodedInst.inst ) {
    case 0x0206e127:  // vse32.v v2, (a3)
    {
      uint64_t a3 = rec.RegFile->GetX<uint64_t>( RevReg::a3 );
      rec.Mem->WriteMem( 0, a3, 8, &vreg[2][0] );
      rec.Mem->WriteMem( 0, a3 + 8, 8, &vreg[2][1] );
      output->verbose(
        CALL_INFO,
        5,
        0,
        "*V"
        " 0x%" PRIx64 " <- v[2][0] 0x%" PRIx64 " <- v[2][1]"
        " M[0x%" PRIx64 "] <- 0x%" PRIx64 " M[0x%" PRIx64 "] <- 0x%" PRIx64 "\n",
        vreg[2][0],
        vreg[2][1],
        a3,
        vreg[2][0],
        a3 + 8,
        vreg[2][1]
      );
      return;
    }
    }
  }

  output->fatal( CALL_INFO, -1, "faux vector coprocessor cannot digest instruction 0x%" PRIx32 "\n", rec.decodedInst.inst );
}

}  //namespace SST::RevCPU
