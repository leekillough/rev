//
// _Zfgatling_h_
//
// Copyright (C) 2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_ZFGATLING_H_
#define _SST_REVCPU_ZFGATLING_H_

#include "../RevExt.h"
#include "../RevInstHelpers.h"

namespace SST::RevCPU {

class Zfgatling : public RevExt {

  // clang-format off
  std::vector<RevInstEntry> ZfgatlingTable = {
  };
  // clang-format on

public:
  Zfgatling( const RevFeature* Feature, RevMem* RevMem, SST::Output* Output ) : RevExt( "Zfgatling", Feature, RevMem, Output ) {
    SetTable( std::move( ZfgatlingTable ) );
  }

};  // end class Zfgatling

}  // namespace SST::RevCPU

#endif
