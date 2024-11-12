//
// _RevVRegFile_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevVRegFile.h"

namespace SST::RevCPU {

RevVRegFile::RevVRegFile( uint16_t vlen, uint16_t elen ) : VLEN( vlen ), ELEN( elen ) {
  assert( VLEN <= 128 );  // Practical limit matching current V standard
  assert( vlen > ELEN );  // Not supporting VLEN==ELEN (max(ELEN)==64)
  bytesPerReg  = VLEN >> 3;
  bytesPerElem = ELEN >> 3;
  elemsPerReg  = VLEN / ELEN;
  vreg.resize( 32, std::vector<uint8_t>( bytesPerReg ) );
  elemMaskInfo.resize( elemsPerReg );
}

void RevVRegFile::Configure( reg_vtype_t vt ) {
  // Iterations over vector registers
  switch( vt.f.vlmul ) {
  case 0: iters = 1; break;
  case 1: iters = 2; break;
  case 2: iters = 4; break;
  case 3: iters = 8; break;
  //case 4:
  case 5:  //mf8
  case 6:  //mf4
  case 7:  //mf2
    iters = 1;
    break;
  default: assert( false ); break;
  }

  // bytes active for each element (for loads/stores)
  for( unsigned e = 0; e < elemsPerReg; e++ ) {
    assert( elemsPerReg == 2 );
    assert( bytesPerElem == 8 );
    elemMaskInfo[e] = std::make_pair( bytesPerElem, -1ULL );
    if( vt.f.vlmul >= 5 ) {
      if( e == 1 ) {
        elemMaskInfo[e] = std::pair( 0, 0ULL );
      } else {
        uint8_t  bytes = bytesPerElem >> ( 7 - vt.f.vlmul );
        uint64_t mask  = 0;
        for( int b = 0; b < bytes; b++ )
          mask |= ( 0xffULL << ( b << 3 ) );
        elemMaskInfo[e] = std::make_pair( bytes, mask );
      }
    }
  }
}

void RevVRegFile::SetElem( uint64_t vd, unsigned e, uint64_t d ) {
  uint64_t res   = d & ElemMask( e );
  size_t   bytes = sizeof( uint64_t );
  assert( ( e * bytes ) < bytesPerReg );
  memcpy( &( vreg[vd][e * bytes] ), &res, bytes );
}

uint64_t RevVRegFile::GetElem( uint64_t vs, unsigned e ) {
  uint64_t res   = 0;
  size_t   bytes = sizeof( uint64_t );
  assert( ( e * bytes ) < bytesPerReg );
  memcpy( &res, &( vreg[vs][e * bytes] ), bytes );
  return res & ElemMask( e );
}

uint64_t RevVRegFile::GetVCSR( uint16_t csr ) {
  assert( vcsrmap.count( csr ) == 1 );
  return vcsrmap[csr];
}

void RevVRegFile::SetVCSR( uint16_t csr, uint64_t d ) {
  assert( vcsrmap.count( csr ) == 1 );
  vcsrmap[csr] = d;
}

uint8_t RevVRegFile::BytesPerReg() {
  return bytesPerReg;
}

uint8_t RevVRegFile::ElemsPerReg() {
  return elemsPerReg;
}

uint8_t RevVRegFile::Iters() {
  return iters;
}

uint8_t RevVRegFile::ElemBytes( unsigned e ) {
  return elemMaskInfo[e].first;
}

uint64_t RevVRegFile::ElemMask( unsigned e ) {
  return elemMaskInfo[e].second;
}

}  //namespace SST::RevCPU
