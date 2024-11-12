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

// Vector CSR Types
union reg_vtype_t {
  uint64_t v = 0x0UL;

  struct {
    uint64_t vlmul : 3;   // [2:0]
    uint64_t vsew  : 3;   // [5:3]
    uint64_t vta   : 1;   // [6]
    uint64_t vma   : 1;   // [7]
    uint64_t zero  : 55;  // [62:8]
    uint64_t vii   : 1;   // [63]
  } f;

  reg_vtype_t( uint64_t _v ) : v( _v ) {};
};

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
    assert( false );  // TODO
    return true;
  }

public:
  void     Configure( reg_vtype_t vt );
  uint64_t GetVCSR( uint16_t csr );
  void     SetVCSR( uint16_t csr, uint64_t d );
  void     SetElem( uint64_t vd, unsigned e, uint64_t d );
  uint64_t GetElem( uint64_t vs, unsigned e );

  uint8_t  BytesPerReg();
  uint8_t  ElemsPerReg();
  uint8_t  Iters();
  uint8_t  ElemBytes( unsigned e );
  uint64_t ElemMask( unsigned e );

private:
  uint16_t                                  VLEN;
  uint16_t                                  ELEN;
  uint8_t                                   bytesPerReg;
  uint8_t                                   bytesPerElem;
  uint8_t                                   elemsPerReg;
  std::vector<std::vector<uint8_t>>         vreg         = {};
  std::vector<std::pair<uint8_t, uint64_t>> elemMaskInfo = {};
  uint8_t                                   iters        = 0;
  ///< RevVRegFile: CSRs residing in vector coprocessor
  std::map<uint16_t, uint64_t> vcsrmap{
    // URW
    {0x008,             0}, // vstart
    {0x009,             0}, // vxsat
    {0x00a,             0}, // vxrm
    {0x00f,             0}, // vxcsr
    // URO
    {0xc20,             0}, // vl
    {0xc21,    1ULL << 63}, // vtype
    {0xc22, 128ULL / 8ULL}, // vlenb
  };
};  //class RevVRegFile

};  //namespace SST::RevCPU

#endif  //_SST_REVCPU_VREVREGFILE_H_
