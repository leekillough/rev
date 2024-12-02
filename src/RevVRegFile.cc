//
// _RevVRegFile_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevCore.h"
#include "RevVRegFile.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace SST::RevCPU {

RevVRegFile::RevVRegFile( RevCore* parent, SST::Output* output, uint16_t vlen, uint16_t elen )
  : parent( parent ), feature( parent->GetRevFeature() ), output( output ), vlen( vlen ), elen( elen ) {
  assert( vlen <= 128 );  // Temporary constraint
  assert( vlen > elen );  // Not supporting VLEN==ELEN (max(ELEN)==64)

#if 1
  std::cout << "*V VLEN=" << std::dec << vlen << " ELEN=" << elen << std::endl;
#endif

  // Set CSR getters/setters in the parent core, capturing pointer to "this" vector register file
  SetCSRGetters( [this]( uint16_t csr ) { return GetCSR( csr ); } );
  SetCSRSetters( [this]( uint16_t csr, uint64_t val ) { return SetCSR( csr, val ); } );

  // Initialize non-zero CSRs
  SetVType<RevVType::vill>( true );  // vill = 1

  uint16_t vlenb = vlen >> 3;
  SetCSR( RevCSR::vlenb, vlenb );

  // Initialize vector register file
  vreg.resize( 32, std::vector<uint8_t>( vlenb ) );
}

RevVRegFile::~RevVRegFile() {
  // Clear the CSR Getters/Setters
  SetCSRGetters( nullptr );
  SetCSRSetters( nullptr );
}

void RevVRegFile::Configure( uint64_t vtype, uint16_t vlmax ) {
  this->vlmax = vlmax;
  // Elements per register
  sewbits     = 1 << ( static_cast<uint8_t>( GetVType<RevVType::vsew>( vtype ) ) + 3 );
  sewmask     = ~( ~uint64_t{} << sewbits );
  assert( sewbits <= elen );
  elemsPerReg = vlen / sewbits;
  assert( elemsPerReg > 0 );
}

bool RevVRegFile::v0mask( bool vm, size_t vecElem ) const {
  if( vm )
    return true;
  // Grab 64-bit chunk containing element mask bit.
  assert( vecElem < vlen );
  unsigned chunk = vecElem / 64;
  unsigned shift = vecElem % 64;
  uint64_t mask;
  memcpy( &mask, &vreg[0][chunk << 3], sizeof( mask ) );
  return mask >> shift & 1;
}

void RevVRegFile::SetMaskReg( uint64_t vd, uint64_t mask[], uint64_t mfield[] ) {
  assert( vlen <= 128 );
  // TODO templatizing would allow 2 operations for vlen=128 instead of 16
  uint8_t*    pmask  = reinterpret_cast<uint8_t*>( mask );
  uint8_t*    pfield = reinterpret_cast<uint8_t*>( mfield );
  std::string s;
  for( unsigned i = 0; i < ( vlen >> 3 ); i++ ) {
    uint8_t byte = ( vreg[vd][i] & ( ~pfield[i] ) ) | ( pmask[i] & pfield[i] );
    vreg[vd][i]  = byte;
    std::stringstream bs;
    bs << std::hex << std::setfill( '0' ) << std::setw( 2 ) << (uint16_t) byte;
    s = bs.str() + s;
  }
  std::cout << "v" << std::dec << vd << ".mask <- 0x" << s << std::endl;
};

uint64_t RevVRegFile::vfirst( uint64_t vs ) const {
  unsigned nbits  = vlmax * sewbits;
  uint16_t nbytes = nbits >> 3;
  uint64_t vlenb  = GetCSR( RevCSR::vlenb );
  uint64_t v      = vs;
  for( uint16_t b = 0; b < nbytes; b++ ) {
    uint16_t d = vreg[v][b % vlenb];
    if( d != 0 )
      return ( b << 3 ) + __builtin_ctz( d );
    if( ( b + 1 ) % vlenb == 0 )
      v++;
  }
  return ~uint64_t{ 0 };
}

/// Get a vector CSR register
uint64_t RevVRegFile::GetCSR( uint16_t csr ) const {
  // clang-format off
  switch( csr ) {
    case RevCSR::vxsat:   return CSR[RevCSR::vcsr] >> 0 & 0b001;
    case RevCSR::vxrm:    return CSR[RevCSR::vcsr] >> 1 & 0b011;
    case RevCSR::vcsr:    return CSR[RevCSR::vcsr] >> 0 & 0b111;
    case RevCSR::vtype:
    case RevCSR::vstart:
    case RevCSR::vl:
    case RevCSR::vlenb:   return CSR[csr];
    default:
      output->fatal( CALL_INFO, -1, "Error: Invalid vector CSR register 0x%04" PRIx16 "\n", csr );
  }
  // clang-format on
  return 0;
}

/// Set a vector CSR register
// This function can set vector CSR registers which are read-only in scalar CSR instructions
bool RevVRegFile::SetCSR( uint16_t csr, uint64_t val ) {
  // clang-format off
  switch( csr ) {
    case RevCSR::vxsat:  CSR[RevCSR::vcsr] = ( CSR[RevCSR::vcsr] & ~uint64_t{0b001} ) | ( val & 0b001 ); break;
    case RevCSR::vxrm:   CSR[RevCSR::vcsr] = ( CSR[RevCSR::vcsr] & ~uint64_t{0b110} ) | ( val & 0b110 ); break;
    case RevCSR::vcsr:   CSR[RevCSR::vcsr] = ( CSR[RevCSR::vcsr] & ~uint64_t{0b111} ) | ( val & 0b111 ); break;
    case RevCSR::vtype:
      uint64_t mask1, mask2;
      if(IsRV64()){
        mask1 = uint64_t{0xff} | +uint64_t{1} << 63;
        mask2 = ~mask1;
      } else {
        mask1 = uint64_t{0xff} | -uint64_t{1} << 31;
        mask2 = uint64_t{0xff} | +uint64_t{1} << 31;
      }
      CSR[RevCSR::vtype] = (CSR[RevCSR::vtype] & ~mask1) | (val & mask2);
      break;
    case RevCSR::vstart:
    case RevCSR::vl:
    case RevCSR::vlenb:
      CSR[csr] = val;
      break;
    default:
      output->fatal( CALL_INFO, -1, "Error: Invalid vector CSR register 0x%04" PRIx16 "\n", csr );
  }
  // clang-format on
  return true;
}

}  //namespace SST::RevCPU
