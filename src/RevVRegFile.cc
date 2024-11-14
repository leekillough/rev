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
#if 1
  std::cout << "*V VLEN=" << std::dec << VLEN << " ELEN=" << ELEN << std::endl;
#endif
  // Initialize non-zero CSRs
  uint64_t vlenb         = VLEN >> 3;
  vcsrmap[RevCSR::vtype] = 1ULL << 63;  // vii = 1
  vcsrmap[RevCSR::vlenb] = vlenb;
  // Initialize vector register file
  vreg.resize( 32, std::vector<uint8_t>( vlenb ) );
}

void RevVRegFile::Configure( reg_vtype_t vt ) {
  // Iterations over vector registers
  switch( vt.f.vlmul ) {
  case 0: itersOverLMUL = 1; break;
  case 1: itersOverLMUL = 2; break;
  case 2: itersOverLMUL = 4; break;
  case 3: itersOverLMUL = 8; break;
  //case 4:
  case 5:  //mf8
  case 6:  //mf4
  case 7:  //mf2
    itersOverLMUL = 1;
    break;
  default: assert( false ); break;
  }

  // Iterations over an element
  unsigned sewbytes = 1 << vt.f.vsew;
  unsigned sewbits  = sewbytes << 3;
  assert( sewbits <= ELEN );
  itersOverElement = ELEN / sewbits;

  // Elements per register
  elemsPerReg      = VLEN / sewbits;
  assert( elemsPerReg > 0 );
  elemValid.resize( elemsPerReg );

  // Fraactional LMUL support
  // vlen=64, elen=8 ( vlen/elen = 8 )
  //              e7 e6 e5 e4 e3 e2 e1 e0
  // m[1248]      1  1  1  1  1  1  1  1   elems=8
  // mf2 lmul=7   0  0  0  0  1  1  1  1   elems=4 eshift=4
  // mf4 lmul=6   0  0  0  0  0  0  1  1   elems=2 eshift=2
  // mf8 lmul=5   0  0  0  0  0  0  0  1   elems=1 eshift=1
  //

  for( unsigned e = 0; e < elemsPerReg; e++ ) {
    assert( vt.f.vlmul != 4 );  // reserved value
    if( vt.f.vlmul <= 3 ) {
      elemValid[e] = true;
    } else {
      // Fractional LMUL
      // ELEN * LMUL >= SEW
      unsigned eshift = 8 - vt.f.vlmul;  // mf2:1 mf4:2 mf8:3
      if( ( ELEN >> eshift ) < sewbits ) {
        vcsrmap[vtype] |= ( 1ULL < 63 );  // vtype.vii
        std::cout << "V* Illegal lmul " << vt.f.vlmul << " for elen=" << std::dec << ELEN << " and sew=e" << sewbits << std::endl;
        assert( false );
      }
      if( e < ( elemsPerReg >> eshift ) ) {
        elemValid[e] = true;
      } else {
        elemValid[e] = false;
        std::cout << "*V Suppressing element " << e << std::endl;
      }
    }
  }
}

uint64_t RevVRegFile::GetVCSR( uint16_t csr ) {
  assert( vcsrmap.count( csr ) == 1 );
  return vcsrmap[csr];
}

void RevVRegFile::SetVCSR( uint16_t csr, uint64_t d ) {
  assert( vcsrmap.count( csr ) == 1 );
  vcsrmap[csr] = d;
}

uint16_t RevVRegFile::BytesPerReg() {
  return vlenb;
}

uint16_t RevVRegFile::ElemsPerReg() {
  return elemsPerReg;
}

uint16_t RevVRegFile::ItersOverLMUL() {
  return itersOverLMUL;
}

uint16_t RevVRegFile::ItersOverElement() {
  return itersOverElement;
}

bool RevVRegFile::ElemValid( unsigned e ) {
  return elemValid[e];
}

}  //namespace SST::RevCPU
