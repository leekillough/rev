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

  vregfile = new RevVRegFile( 128, 64 );
}

RevVectorCoProc::~RevVectorCoProc() {
  if( vregfile )
    delete vregfile;
}

void RevVectorCoProc::registerStats() {
  num_instRetired = registerStatistic<uint64_t>( "InstRetired" );
}

bool RevVectorCoProc::Reset() {
  InstQ = {};
  return true;
}

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

bool RevVectorCoProc::EnableExt( RVVec* Ext ) {
  if( !Ext )
    output->fatal( CALL_INFO, -1, "Error: failed to initialize RISC-V extensions\n" );

  output->verbose( CALL_INFO, 6, 0, "Enabling extension=%s\n", Ext->GetName().data() );

  // add the extension to our vector of enabled objects
  Extensions.push_back( std::unique_ptr<RVVec>( Ext ) );

  // setup the mapping of InstTable to Ext objects
  auto load = [&]( const std::vector<RevVecInstEntry>& Table ) {
    InstTable.reserve( InstTable.size() + Table.size() );
    for( unsigned i = 0; i < Table.size(); i++ ) {
      InstTable.push_back( Table[i] );
      auto ExtObj = std::pair<unsigned, unsigned>( Extensions.size() - 1, i );
      EntryToExt.insert( std::pair<unsigned, std::pair<unsigned, unsigned>>( InstTable.size() - 1, ExtObj ) );
    }
  };

  // retrieve all the target instructions
  load( Ext->GetVecTable() );

  return true;
}

bool RevVectorCoProc::InitTableMapping() {
  output->verbose( CALL_INFO, 6, 0, "Initializing table mapping for V extension\n" );
  for( unsigned i = 0; i < InstTable.size(); i++ ) {
    NameToEntry.insert( std::pair<std::string, unsigned>( ExtractMnemonic( InstTable[i] ), i ) );
    // map normal instruction
    EncToEntry.insert( std::pair<uint64_t, unsigned>( CompressEncoding( InstTable[i] ), i ) );
    output->verbose(
      CALL_INFO, 6, 0, "Table Entry %" PRIu32 " = %s\n", CompressEncoding( InstTable[i] ), ExtractMnemonic( InstTable[i] ).data()
    );
  }
  return true;
}

std::string RevVectorCoProc::ExtractMnemonic( const RevVecInstEntry& Entry ) {
  std::string              Tmp = Entry.mnemonic;
  std::vector<std::string> vstr;
  RevOpts::splitStr( Tmp, " ", vstr );
  return vstr[0];
}

uint32_t RevVectorCoProc::CompressEncoding( const RevVecInstEntry& Entry ) {
  uint32_t Value = Entry.opcode;
  Value |= uint32_t( Entry.funct3 ) << 8;
  return Value;
}

auto RevVectorCoProc::matchInst(
  const std::unordered_multimap<uint64_t, unsigned>& map,
  uint64_t                                           encoding,
  const std::vector<RevVecInstEntry>&                InstTable,
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

  // vector csr access instructions
  csr_inst_t csr_inst( Inst );
  if( csr_inst.f.opcode == csr_inst_opcode ) {
    if( csr_inst.f.csr >= RevCSR::vstart && csr_inst.f.csr <= RevCSR::vcsr )
      return true;
    if( csr_inst.f.csr >= RevCSR::vl && csr_inst.f.csr <= RevCSR::vlenb )
      return true;
  }
  // RVV instructions
  //VecInst = { Inst, vregfile };  // pre-decode
  VecInst = { Inst };  // pre-decode
  auto it = matchInst( EncToEntry, VecInst.Enc, InstTable, Inst );
  if( it == EncToEntry.end() ) {
    output->verbose( CALL_INFO, 1, 0, "Warning: Vector coprocessor unable to decode instruction 0x%" PRIu32 "\n", Inst );
  }
  VecInst.revInstEntry = &( InstTable[it->second] );
  return true;
}

bool RevVectorCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  // TODO finite instruction queue
  if( Decode( Inst ) ) {
    // Decode the full instruction.
    assert( VecInst.revInstEntry );
    switch( VecInst.revInstEntry->format ) {
    case RVVTypeOp: VecInst.DecodeRVVTypeOp(); break;
    case RVVTypeLd: VecInst.DecodeRVVTypeLd(); break;
    case RVVTypeSt: VecInst.DecodeRVVTypeSt(); break;
    default: output->fatal( CALL_INFO, -1, "Error: failed to decode instruction 0x%" PRIx32 ".", Inst );
    }
    // Grab any scalars from core register file. In reality, the core must provided these after its decode pipeline.
    //VecInst.ReadScalars( R );
    // Now push the whole enchilada into the vector instruction queue.
    RevCoProcInst inst = RevCoProcInst( VecInst, F, R, M );
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

  // CSR access presumes valid csr address
  csr_inst_t csr_inst( rec.VecInst.Inst );
  if( csr_inst.f.opcode == csr_inst_opcode ) {
    if( csr_inst.f.funct3 == csr_inst_funct3::csrrsi ) {
      rec.RegFile->SetX( csr_inst.f.rd, csrmap[csr_inst.f.csr] );
      return;
    } else {
      output->fatal( CALL_INFO, -1, "currently only csrrsi supported for vector csr access\n" );
      return;
    }
  }
  // execute vector instruction
  VecInst.revInstEntry->func( rec.Feature, rec.RegFile, vregfile, rec.Mem, VecInst );
  return;
}

//RevVecInst::RevVecInst( uint32_t inst, RevVRegFile* vrf ) : Inst( inst ), vregfile( vrf ) {
RevVecInst::RevVecInst( uint32_t inst ) : Inst( inst ) {
  opcode = Inst & 0x7f;
  if( opcode == 0b0000111 ) {  // LOAD-FP
    format = RevInstF::RVVTypeLd;
    funct3 = 8;
  } else if( opcode == 0b0100111 ) {  // STORE-FP
    format = RevInstF::RVVTypeSt;
    funct3 = 8;
  } else if( opcode == 0b1010111 ) {  // OP-V
    format = RevInstF::RVVTypeOp;
    funct3 = ( Inst >> 12 ) & 7;
  }
  Enc = funct3 << 8 | opcode;
}

void RevVecInst::DecodeBase() {
  // registers

  if( revInstEntry->rdClass == RevRegClass::RegVEC ) {
    vd = DECODE_VD( Inst );
  } else if( revInstEntry->rdClass != RevRegClass::RegUNKNOWN ) {
    rd = DECODE_RD( Inst );
  }

  if( revInstEntry->rs1Class == RevRegClass::RegVEC ) {
    vs1 = DECODE_VS1( Inst );
  } else if( revInstEntry->rs1Class != RevRegClass::RegUNKNOWN ) {
    rs1      = DECODE_RS1( Inst );
    read_rs1 = true;
  }

  if( revInstEntry->rs2Class == RevRegClass::RegVEC ) {
    vs2 = DECODE_VS2( Inst );
  } else if( revInstEntry->rs2Class == RevRegClass::RegUNKNOWN ) {
    rs2      = DECODE_RS2( Inst );
    read_rs2 = true;
  }

  if( revInstEntry->rs3Class == RevRegClass::RegVEC ) {
    vs3 = DECODE_VS3( Inst );
  }
}

void RevVecInst::DecodeRVVTypeOp() {
  DecodeBase();
}

void RevVecInst::DecodeRVVTypeLd() {
  DecodeBase();
}

void RevVecInst::DecodeRVVTypeSt() {
  DecodeBase();
}

// void RevVecInst::ReadScalars( RevRegFile* R ) {
//   if( read_rs1 )
//     src1 = R->GetX<uint64_t>( rs1 );
//   if( read_rs2 )
//     src1 = R->GetX<uint64_t>( rs2 );
// }

}  //namespace SST::RevCPU
