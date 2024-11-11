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

RevVRegFile::RevVRegFile( uint16_t vlen, uint16_t elen ) : VLEN( vlen ), ELEN( elen ) {}

void RevVRegFile::Configure( reg_vtype_t vt ) {
  maskLo  = 0xffffffffffffffffULL;
  bytesLo = 8;
  maskHi  = 0xffffffffffffffffULL;
  bytesHi = 8;
  if( vt.f.vlmul >= 5 ) {
    maskHi  = 0;
    bytesHi = 0;
    if( vt.f.vlmul == 6 ) {
      maskLo  = 0x00000000ffffffffULL;
      bytesLo = 4;
    } else if( vt.f.vlmul == 5 ) {
      maskLo  = 0x000000000000ffffULL;
      bytesLo = 2;
    }
  }
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
}

void RevVRegFile::SetVLo( uint64_t vd, uint64_t d ) {
  vreg[vd][0] = d & maskLo;
};

void RevVRegFile::SetVHi( uint64_t vd, uint64_t d ) {
  vreg[vd][1] = d & maskHi;
}

uint64_t RevVRegFile::GetVLo( uint64_t vs ) {
  return vreg[vs][0] & maskLo;
}

uint64_t RevVRegFile::GetVHi( uint64_t vs ) {
  return vreg[vs][1] & maskHi;
}

uint8_t RevVRegFile::BytesHi() {
  return bytesHi;
};

uint8_t RevVRegFile::BytesLo() {
  return bytesLo;
}

uint8_t RevVRegFile::Iters() {
  return iters;
};

}  //namespace SST::RevCPU
