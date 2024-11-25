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

  // This causes an exception - why?
  // friend std::ostream& operator<<( std::ostream& os, const reg_vtype_t& vt ) {
  //   os << "vlmul=" << vt.f.vlmul << " vsew=" << vt.f.vsew << " vta=" << vt.f.vta << " vma=" << vt.f.vma << " vii=" << vt.f.vii;
  // };
};

// SEW
enum sew_t { e8 = 0, e16 = 1, e32 = 2, e64 = 3 };

// VLMUL
enum lmul_t { m1 = 0, m2 = 1, m4 = 2, m8 = 3, mf8 = 5, mf4 = 6, mf2 = 7 };

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
  RevVRegFile( SST::Output* output, uint16_t vlen, uint16_t elen );
  virtual ~RevVRegFile() {};

  SST::Output* output() { return output_; };

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

  uint16_t vlen() { return vlen_; }

  uint16_t elen() { return elen_; }

  uint16_t vlmax() { return vlmax_; }

  void     Configure( reg_vtype_t vt, uint16_t vlmax );
  uint64_t GetVCSR( uint16_t csr );
  void     SetVCSR( uint16_t csr, uint64_t d );
  uint16_t ElemsPerReg();  ///< RevVRegFile: VLEN/SEW

  bool v0mask( bool vm, unsigned vecElem );  ///< RevVRegFile: Retrieve vector mask bit from v0

  // base is destination register in instruction
  // vd is current destination register
  // vecElem is the vector element number relative to base register
  // el is the element within the current destination register
  // d is data
  // vm is vector mask bit
  template<typename T>
  void SetElem( uint64_t base, uint64_t vd, unsigned vecElem, unsigned el, T d ) {
    size_t bytes = sizeof( T );
    assert( ( el * bytes ) <= GetVCSR( vlenb ) );
    T res = d;
    memcpy( &( vreg[vd][el * bytes] ), &res, bytes );
    std::cout << "*V v" << std::dec << vd << "." << el << " <- 0x" << std::hex << (uint64_t) d << std::endl;
  };

  template<typename T>
  T GetElem( uint64_t vs, unsigned e ) {
    T      res   = 0;
    size_t bytes = sizeof( T );
    assert( ( e * bytes ) <= GetVCSR( vlenb ) );
    memcpy( &res, &( vreg[vs][e * bytes] ), bytes );
    return res;
  };

  // Vector loads and stores have an EEW encoded directly in the instruction. The corresponding EMUL is
  // calculated as EMUL = (EEW/SEW)*LMUL. If the EMUL would be out of range (EMUL>8 or EMUL<1/8),
  // the instruction encoding is reserved. The vector register groups must have legal register specifiers for the
  // selected EMUL, otherwise the instruction encoding is reserved.
  template<typename EEW, typename SEW>
  std::pair<unsigned, bool> emul() {
    reg_vtype_t vt      = vcsrmap[vtype];
    size_t      eew     = sizeof( EEW ) << 3;
    size_t      sew     = sizeof( SEW ) << 3;

    // m1=0, m2=1, m4=2, m8=3,  mf8=5, mf4=6, mf2=7
    int      m          = vt.f.vlmul < 4 ? eew << vt.f.vlmul : eew >> ( 8 - vt.f.vlmul );
    unsigned em         = m / sew;  // hopefully compiler makes this a shift
    bool     fractional = false;
    if( em == 0 ) {
      assert( m > 0 );
      em         = sew / m;  // TODO shift to replace divide?
      fractional = true;
      assert( em != 1 );
    }
    assert( em == 1 || em == 2 || em == 4 || em == 8 );
    return std::make_pair( em, fractional );
  }

  void SetMaskReg( uint64_t vd, uint64_t mask[], uint64_t mfield[] );  ///< RevVRegFile: Write entire mask register

  uint64_t vfirst( uint64_t vs );  ///< RevVRegFile: return index of first 1 in bit field or -1 if all are 0.

private:
  SST::Output* output_;
  uint16_t     vlen_;
  uint16_t     elen_;
  uint16_t     vlmax_;
  uint16_t     elemsPerReg_              = 0;
  unsigned     sewbytes                  = 0;
  unsigned     sewbits                   = 0;
  uint64_t     sewmask                   = 0;

  std::vector<std::vector<uint8_t>> vreg = {};

  ///< RevVRegFile: CSRs residing in vector coprocessor
  std::map<uint16_t, uint64_t> vcsrmap{
    // URW
    {0x008, 0}, // vstart
    {0x009, 0}, // vxsat
    {0x00a, 0}, // vxrm
    {0x00f, 0}, // vxcsr
    // URO
    {0xc20, 0}, // vl
    {0xc21, 0}, // vtype
    {0xc22, 0}, // vlenb
  };

};  //class RevVRegFile

};  //namespace SST::RevCPU

#endif  //_SST_REVCPU_VREVREGFILE_H_
