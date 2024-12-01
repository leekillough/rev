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
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SST::RevCPU {

// ---------------------------------------------------------------
// RevVectorCoProc
// ---------------------------------------------------------------
RevVectorCoProc::RevVectorCoProc( ComponentId_t id, Params& params, RevCore* parent )
  : RevCoProc( id, params, parent ), num_instRetired( 0 ) {

  vlen                  = params.find<uint16_t>( "vlen", 128 );
  elen                  = params.find<uint16_t>( "elen", 64 );

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

  vregfile = std::make_unique<RevVRegFile>( parent, output, vlen, elen );
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

  EnableExt( new( std::nothrow ) RVVec( nullptr, nullptr, output ) );
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
    uint64_t enc = CompressEncoding( InstTable[i] );
    EncToEntry.insert( std::pair<uint64_t, unsigned>( enc, i ) );
    output->verbose( CALL_INFO, 6, 0, "Table Entry 0x%" PRIx64 " = %s\n", enc, ExtractMnemonic( InstTable[i] ).data() );
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
  // Must be consistent with RevVecInst::RevVecInst( uint32_t inst )
  uint32_t Value = Entry.opcode;           // 7-bits
  Value |= uint32_t( Entry.funct3 ) << 8;  // 3 bits
  if( ( Entry.format == RVVTypeLd ) || ( Entry.format == RVVTypeSt ) ) {
    //Value |= (uint32_t) ( Entry.umop ) << 14;  // 5 bits
    Value |= (uint32_t) ( Entry.mop ) << 16;  // 2 bits
  }
  Value |= (uint32_t) ( Entry.funct6 ) << 18;  // 6 bits

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
    // else {
    //   std::cout << "### Entry=" << Entry << " pred=" << InstTable[Entry].predicate(Inst) << std::endl;
    // }
  }
  // No match
  return map.end();
}

bool RevVectorCoProc::Decode( const uint32_t Inst ) {

  VecInst = { Inst };  // pre-decode

  // RVV instructions
  auto it = matchInst( EncToEntry, VecInst.Enc, InstTable, Inst );
  if( it == EncToEntry.end() ) {
    output->verbose( CALL_INFO, 1, 0, "Warning: Vector coprocessor unable to decode instruction 0x%" PRIx32 "\n", Inst );
    return false;
  }
  VecInst.revInstEntry = &( InstTable[it->second] );
  return true;
}

bool RevVectorCoProc::IssueInst( const RevFeature* F, RevRegFile* R, RevMem* M, uint32_t Inst ) {
  // TODO finite instruction queue
  // TODO It is not clear from here if Decode() is setting anything like VecInst. Better to return VecInst from Decode() or pass by local VecInst variable by reference to Decode() instead of setting a class variable hidden somewhere else.
  if( Decode( Inst ) ) {
    // Decode the full instruction
    if( VecInst.revInstEntry ) {
      switch( VecInst.revInstEntry->format ) {
      case RVVTypeOpv: VecInst.DecodeRVVTypeOp(); break;
      case RVVTypeLd: VecInst.DecodeRVVTypeLd(); break;
      case RVVTypeSt: VecInst.DecodeRVVTypeSt(); break;
      default: output->fatal( CALL_INFO, -1, "Error: failed to decode instruction 0x%" PRIx32 ".", Inst );
      }
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
  // execute vector instruction
  assert( VecInst.revInstEntry );
  VecInst.revInstEntry->func( rec.Feature, rec.RegFile, vregfile.get(), rec.Mem, VecInst );
  return;
}

//RevVecInst::RevVecInst( uint32_t inst, RevVRegFile* vrf ) : Inst( inst ), vregfile( vrf ) {
RevVecInst::RevVecInst( uint32_t inst ) : Inst( inst ) {
  // Must be consistent with RevVectorCoProc::CompressEncoding
  opcode = Inst & 0x7f;
  if( opcode == 0b0000111 ) {  // LOAD-FP
    format = RevInstF::RVVTypeLd;
  } else if( opcode == 0b0100111 ) {  // STORE-FP
    format = RevInstF::RVVTypeSt;
  } else if( opcode == 0b1010111 ) {  // OP-V
    format = RevInstF::RVVTypeOpv;
  }
  // Funct3 selects width for vl*/vs* and op-v for vector arith/set*vl*
  funct3 = ( Inst >> 12 ) & 7;
  Enc    = funct3 << 8 | opcode;
  if( ( format == RevInstF::RVVTypeLd ) || ( format == RevInstF::RVVTypeSt ) ) {
    //umop = ( Inst >> 20 ) & 0b11111;
    mop = ( Inst >> 26 ) & 0b11;
    //Enc |= ( umop << 14 ) | ( mop << 16 );
    Enc |= ( mop << 16 );
  } else if( ( format == RevInstF::RVVTypeOpv ) && ( funct3 != RVVec::OPV::CFG ) ) {
    // funct6 selects operation for integer arithmetic
    uint64_t funct6 = ( Inst >> 26 ) & 0b111111;
    Enc |= ( funct6 << 18 );
  }
  // vector mask bit
  vm = ( Inst >> 25 ) & 1;
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
  } else if( revInstEntry->rs2Class != RevRegClass::RegUNKNOWN ) {
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
