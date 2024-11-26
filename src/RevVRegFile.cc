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
  assert( vlen_ <= 128 );   // Temporary constraint
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
  vlmax_   = vlmax;
  // Elements per register
  sewbytes = 1 << vt.f.vsew;
  sewbits  = sewbytes << 3;
  sewmask  = ~( ( -1ULL ) << sewbits );
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

bool RevVRegFile::v0mask( bool vm, unsigned vecElem ) {
  if( vm )
    return true;
  // Grab 64-bit chunk containing element mask bit.
  assert( vecElem < vlen_ );
  unsigned  chunk = vecElem >> 6;
  unsigned  shift = vecElem % 64;
  uint64_t* pmask = (uint64_t*) ( &( vreg[0][chunk << 3] ) );
  return ( ( *pmask ) >> shift ) & 1;
}

void RevVRegFile::SetMaskReg( uint64_t vd, uint64_t mask[], uint64_t mfield[] ) {
  assert(vlen_ <= 128);
  // TODO templatizing would allow 2 operations for vlen=128 instead of 16
  uint8_t* pmask = (uint8_t*) mask;
  uint8_t* pfield = (uint8_t*) mfield;
  std::string s;
  for (unsigned i=0;i<(vlen_>>3); i++) {
    uint8_t byte = (vreg[vd][i] & (~pfield[i])) | (pmask[i] & pfield[i]);
    vreg[vd][i] = byte;
    std::stringstream bs;
    bs << std::hex << std::setfill('0') << std::setw(2) << (uint16_t) byte;
    s = bs.str() + s;
  }
  std::cout << "v" << std::dec << vd << ".mask <- 0x" << s << std::endl;
};

uint64_t RevVRegFile::vfirst( uint64_t vs ) {
  unsigned nbits  = vlmax_ * sewbits;
  uint16_t nbytes = nbits >> 3;
  uint64_t lenb   = vcsrmap[RevCSR::vlenb];
  uint64_t v      = vs;
  for( uint16_t b = 0; b < nbytes; b++ ) {
    uint16_t d = vreg[v][b % lenb];
    if( d != 0 )
      return ( b << 3 ) + __builtin_ctz( d );
    if( ( ( b + 1 ) % lenb ) == 0 )
      v++;
  }
  return -1ULL;
}

} //namespace SST::RevCPU
