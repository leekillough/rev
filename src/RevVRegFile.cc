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

  // Elements per register
  unsigned sewbytes = 1 << vt.f.vsew;
  unsigned sewbits  = sewbytes << 3;
  assert( sewbits <= ELEN );
  elemsPerReg = VLEN / sewbits;
  assert( elemsPerReg > 0 );
}

uint64_t RevVRegFile::GetVCSR( uint16_t csr ) {
  assert( vcsrmap.count( csr ) == 1 );
  return vcsrmap[csr];
}

void RevVRegFile::SetVCSR( uint16_t csr, uint64_t d ) {
  assert( vcsrmap.count( csr ) == 1 );
  vcsrmap[csr] = d;
}

uint16_t RevVRegFile::ElemsPerReg() {
  return elemsPerReg;
}

uint16_t RevVRegFile::ItersOverLMUL() {
  return itersOverLMUL;
}

}  //namespace SST::RevCPU
