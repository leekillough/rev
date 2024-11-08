//
// _RevVRegFile_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_VREVREGFILE_H_
#define _SST_REVCPU_VREVREGFILE_H_

#include "RevCSR.h"

namespace SST::RevCPU {

/// RISC-V Vector Register Mnemonics
// clang-format off
enum class VRevReg : uint16_t {
    v0  = 0,  v1  = 1,  v2  = 2,  v3  = 3,  v4  = 4,  v5  = 5,  v6  = 6,   v7 = 7,
    v8  = 8,  v9  = 9,  v10 = 10, v11 = 11, v12 = 12, v13 = 13, v14 = 14, v15 = 15,
    v16 = 16, v17 = 17, v18 = 18, v19 = 19, v20 = 20, v21 = 21, v22 = 22, v23 = 23,
    v24 = 24, v25 = 25, v26 = 26, v27 = 27, v28 = 28, v29 = 29, v30 = 30, v31 = 31
};
// clang-format on

class RevVRegFile : public RevCSR {
public:
  RevVRegFile( uint16_t vlen, uint16_t elen );

  RevCore* GetCore() const override {
    assert( false );
    return nullptr;
  };

  uint64_t GetPC() const override {
    assert( false );
    return 0;
  }

  bool IsRV64() const override {
    assert( false );
    return true;
  }

public:
  // Proto Vector RF VLEN=128, ELEN=64
  uint64_t vreg[32][2] = { { 0 } };

private:
  uint16_t VLEN;
  uint16_t ELEN;
};  //class RevVRegFile

};  //namespace SST::RevCPU

#endif  //_SST_REVCPU_VREVREGFILE_H_
