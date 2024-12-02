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
#include "RevCommon.h"
#include "SST.h"
#include <iomanip>
#include <iostream>
#include <utility>
#include <vector>

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

  reg_vtype_t( uint64_t v ) : v( v ){};

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

class RevCore;

// Vtype field names
enum class RevVType : uint8_t { vlmul, vsew, vta, vma, vill };

class RevVRegFile {
public:
  // Constructor which takes a RevCore to indicate its hart's parent core
  RevVRegFile( RevCore* parent, SST::Output* output, uint16_t vlen, uint16_t elen );
  ~RevVRegFile();

  bool IsRV64() const { return feature->IsRV64(); }

  /// Extract a field from VType, returning a strict type depending on field
  template<RevVType field>
  auto GetVType( uint64_t vtype ) const {
    if constexpr( field == RevVType::vlmul ) {
      return static_cast<lmul_t>( vtype >> 0 & 0b111 );  // [2:0]
    } else if constexpr( field == RevVType::vsew ) {
      return static_cast<sew_t>( vtype >> 3 & 0b111 );  // [5:3]
    } else if constexpr( field == RevVType::vta ) {
      return static_cast<bool>( vtype >> 6 & 0b001 );  // [6]
    } else if constexpr( field == RevVType::vma ) {
      return static_cast<bool>( vtype >> 7 & 0b001 );  // [7]
    } else if constexpr( field == RevVType::vill ) {
      return static_cast<bool>( vtype >> ( IsRV64() ? 63 : 31 ) & 1 );  // [XLEN-1]
    } else {
      static_assert( make_dependent<field>( false ), "Error: Unrecognized RevVType field" );
    }
  }

  /// With no arguments, get return the field for the current CSR's VType
  template<RevVType field>
  auto GetVType() const {
    return GetVType<field>( GetCSR( RevCSR::vtype ) );
  }

  /// Set a field in a VType
  template<RevVType field, typename VT, typename T>
  void SetVType( VT& vtype, T val ) const {
    if constexpr( field == RevVType::vlmul ) {
      vtype = ~( ~vtype | 0b111 << 0 ) | ( static_cast<VT>( val ) & 0b111 ) << 0;  // [2:0]
    } else if constexpr( field == RevVType::vsew ) {
      vtype = ~( ~vtype | 0b111 << 3 ) | ( static_cast<VT>( val ) & 0b111 ) << 3;  // [5:3]
    } else if constexpr( field == RevVType::vta ) {
      vtype = ~( ~vtype | 0b001 << 6 ) | ( static_cast<VT>( val ) & 0b001 ) << 6;  // [6]
    } else if constexpr( field == RevVType::vma ) {
      vtype = ~( ~vtype | 0b001 << 7 ) | ( static_cast<VT>( val ) & 0b001 ) << 7;  // [7]
    } else if constexpr( field == RevVType::vill ) {
      int vill = IsRV64() ? 63 : 31;
      assert( vill < sizeof( VT ) * 8 );  // make sure we're writing to large enough vtype
      vtype = ~( ~vtype | -VT{ 1 } << vill ) | ( static_cast<VT>( val ) & 0b001 ) << vill;  // [XLEN-1]
    } else {
      static_assert( make_dependent<T>( false ), "Error: Unrecognized RevVType field" );
    }
  }

  /// With one argument, set a field in the current CSR's VType
  template<RevVType field, typename T>
  void SetVType( T val ) {
    uint64_t vtype = GetCSR( RevCSR::vtype );
    SetVType<field>( vtype, val );
    SetCSR( RevCSR::vtype, vtype );
  }

  void Configure( uint64_t vtype, uint16_t vlmax );

  uint16_t ElemsPerReg() const { return elemsPerReg; }  ///< RevVRegFile: VLEN/SEW

  bool v0mask( bool vm, size_t vecElem ) const;  ///< RevVRegFile: Retrieve vector mask bit from v0

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
  T GetElem( uint64_t vs, size_t e ) const {
    T      res   = 0;
    size_t bytes = sizeof( T );
    assert( e * bytes <= GetCSR( RevCSR::vlenb ) );
    memcpy( &res, &vreg[vs][e * bytes], bytes );
    return res;
  };

    // Vector loads and stores have an EEW encoded directly in the instruction. The corresponding EMUL is
    // calculated as EMUL = (EEW/SEW)*LMUL. If the EMUL would be out of range (EMUL>8 or EMUL<1/8),
    // the instruction encoding is reserved. The vector register groups must have legal register specifiers for the
    // selected EMUL, otherwise the instruction encoding is reserved.

#if 1

  // Original version
  template<typename EEW, typename SEW>
  std::pair<unsigned, bool> emul() {
    reg_vtype_t vt      = GetCSR( RevCSR::vtype );
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

#else

  // Logarithmic version, returning lg(emul), which is right shift (non-positive) or left shift (non-negative)
  // lg() relies on fast __builtin_clz() and can be computed at compile-time
  template<typename EEW, typename SEW>
  int lgemul() const {
    auto vlmul = static_cast<uint8_t>( GetVType<RevVType::vlmul>() );  // [2:0]
    int  lgem  = lg( sizeof( EEW ) ) - lg( sizeof( SEW ) ) + vlmul;

    // m1=0, m2=1, m4=2, m8=3, reserved=4, mf8=5, mf4=6, mf2=7
    if( vlmul >= 4 )
      lgem -= 8;

    assert( lgem >= -3 && lgem <= 3 );
    return lgem;
  }
#endif

  void SetMaskReg( uint64_t vd, uint64_t mask[], uint64_t mfield[] );  ///< RevVRegFile: Write entire mask register

  uint64_t vfirst( uint64_t vs ) const;  ///< RevVRegFile: return index of first 1 in bit field or -1 if all are 0.

  /// Get a vector CSR register
  uint64_t GetCSR( uint16_t csr ) const;

  /// Set a vector CSR register
  bool SetCSR( uint16_t csr, uint64_t val );

private:
  // clang-format off

  /// Set the vector CSR Getters to a function which can be nullptr to clear them
  // make_dependent delays evaluation of "parent" until the template is instantiated
  template<typename GETTER>
  void SetCSRGetters( GETTER getter ) {
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vxsat,  getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vxrm,   getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vcsr,   getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vstart, getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vl,     getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vtype,  getter );
    make_dependent<GETTER>( parent )->SetCSRGetter( RevCSR::vlenb,  getter );
  }

  /// Set the vector CSR Setters to a function which can be nullptr to clear them
  // This does not need to set Setters for CSR registers which are read-only in scalar CSR instructions
  template<typename SETTER>
  void SetCSRSetters( SETTER setter ) {
    make_dependent<SETTER>( parent )->SetCSRSetter( RevCSR::vxsat,  setter );
    make_dependent<SETTER>( parent )->SetCSRSetter( RevCSR::vxrm,   setter );
    make_dependent<SETTER>( parent )->SetCSRSetter( RevCSR::vcsr,   setter );
    make_dependent<SETTER>( parent )->SetCSRSetter( RevCSR::vstart, setter );
  }

  // clang-format on

public:
  RevCore* const          parent;
  const RevFeature* const feature;
  SST::Output* const      output;

private:
  uint16_t                          vlen{};
  uint16_t                          elen{};
  uint16_t                          vlmax{};
  uint16_t                          elemsPerReg{};
  unsigned                          sewbits{};
  uint64_t                          sewmask{};
  uint64_t                          CSR[RevCSR::CSR_LIMIT]{};
  std::vector<std::vector<uint8_t>> vreg{};
};  //class RevVRegFile

};  //namespace SST::RevCPU

#endif  //_SST_REVCPU_VREVREGFILE_H_
