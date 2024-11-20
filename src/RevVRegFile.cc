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

RevVRegFile::RevVRegFile( SST::Output* o, uint16_t vlen, uint16_t elen ) : output_( o ), vlen_( vlen ), elen_( elen ) {
  assert( vlen_ <= 128 );  // Temporary constraint
  assert( vlen_ > elen_ );  // Not supporting VLEN==ELEN (max(ELEN)==64)
#if 1
  std::cout << "*V VLEN=" << std::dec << vlen_ << " ELEN=" << elen_ << std::endl;
#endif
  // Initialize non-zero CSRs
  uint64_t vlenb         = vlen_ >> 3;
  vcsrmap[RevCSR::vtype] = 1ULL << 63;  // vii = 1
  vcsrmap[RevCSR::vlenb] = vlenb;
  // Initialize vector register file
  vreg.resize( 32, std::vector<uint8_t>( vlenb ) );
}

void RevVRegFile::Configure( reg_vtype_t vt, uint16_t vlmax ) {
  vlmax_ = vlmax;
  // Elements per register
  sewbytes = 1 << vt.f.vsew;
  sewbits  = sewbytes << 3;
  sewmask  = ~((-1ULL) << sewbits);
  assert( sewbits <= elen_ );
  elemsPerReg_ = vlen_ / sewbits;
  assert( elemsPerReg_ > 0 );
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
  return elemsPerReg_;
}

void RevVRegFile::SetMaskReg( uint64_t vd, uint64_t d ) {
  // TODO this needs to be better generalized and tested.
  // only works for VLEN>=64 and VLMAX<=64
  unsigned lenb = GetVCSR(vlenb);
  memset( &( vreg[vd][0] ), 0, lenb );
  unsigned bytes = lenb<sizeof(d) ? lenb : sizeof(d);
  memcpy( &( vreg[vd][0] ), &d, bytes );
#if 1
  uint64_t* p = (uint64_t*)(&vreg[vd][0]);
  std::cout << "*V SetMaskReg v" << std::dec << vd << " <- 0x" << std::hex << *p << std::endl;
#endif
};

uint64_t RevVRegFile::vfirst( uint64_t vs ) {
  unsigned nbits = vlmax_ * sewbits; 
  uint16_t nbytes = nbits >> 3;
  uint64_t lenb = vcsrmap[RevCSR::vlenb];
  uint64_t v = vs;
  for( uint16_t b = 0; b < nbytes; b++ ) {
    uint16_t d = vreg[v][b % lenb];
    if( d != 0 )
      return ( b << 3 ) + __builtin_ctz( d );
    if ( ((b+1) % lenb) == 0 )
      v++;
  }
  return -1ULL;
}

}  //namespace SST::RevCPU
