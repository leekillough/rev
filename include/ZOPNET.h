//
// _ZOPNet_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

/**
 * TOOD: Look at changing up the zop event to handle msg sends a little differently since
 * those are sent to an 11b logical PE; in the zen, we're breaking that up to a 9b hart id
 * and 2b ZCID; zopnet::send() handles steering those to the zqm (zqm holds mapping of
 * logical_pe to phys zap/hart)
 */

#ifndef _SST_ZOPNET_H_
#define _SST_ZOPNET_H_

// -- Standard Headers
#include <map>
#include <queue>
#include <unistd.h>
#include <vector>

// -- SST Headers
#include "SST.h"

/// -- Rev Headers
#include "../common/include/RevCommon.h"

namespace SST::Forza {

// --------------------------------------------
// Preprocessor defs
// --------------------------------------------
#define Z_NUM_HEADER_FLITS 3

// Src/Dest shifts
#define Z_SHIFT_HARTID     0
#define Z_SHIFT_ZCID       11
#define Z_SHIFT_PCID       15
#define Z_SHIFT_PRECINCT   19
// Rest of flit 0
#define Z_SHIFT_OPC        32
#define Z_SHIFT_MBXID      40
#define Z_SHIFT_SEQNUM     43
#define Z_SHIFT_FLITLEN    47
#define Z_SHIFT_TYPE       59
// Rest of flit 1
#define Z_SHIFT_APPID      32
#define Z_SHIFT_RINGLVL    36
#define Z_SHIFT_MSGID      38
// Addr flit
#define Z_SHIFT_ADDR       0

// Src/Dest masks
#define Z_MASK_HARTID      0b11111111111
#define Z_MASK_ZCID        0b1111
#define Z_MASK_PCID        0b1111
#define Z_MASK_PRECINCT    0b1111111111111
// Rest of flit 0
#define Z_MASK_OPC         0b11111111
#define Z_MASK_MBXID       0b111
#define Z_MASK_SEQNUM      0b1111
#define Z_MASK_FLITLEN     0b1111
#define Z_MASK_TYPE        0b11111
// Rest of flit 1
#define Z_MASK_APPID       0b1111
#define Z_MASK_RINGLVL     0b11
#define Z_MASK_MSGID       0b1111111111111
// Addr flit
#define Z_MASK_ADDR        0x7FFFFFFFFF  //39 bit field

#define Z_FLIT_OPC         0
#define Z_FLIT_MBXID       0
#define Z_FLIT_SEQNUM      0
#define Z_FLIT_FLITLEN     0
#define Z_FLIT_TYPE        0
#define Z_FLIT_APPID       1
#define Z_FLIT_RINGLVL     1
#define Z_FLIT_MSGID       1
#define Z_FLIT_DEST        0
#define Z_FLIT_SRC         1

#define Z_FLIT_ADDR        3  //2
#define Z_FLIT_DATA        4  //3
#define Z_FLIT_DATA_RESP   3
#define Z_FLIT_SENSE       2  //3

#define Z_MZOP_PIPE_HART   0
#define Z_HZOP_PIPE_HART   1
#define Z_RZOP_PIPE_HART   2

#define Z_MZOP_DMA_MAX     2040

#define Z_MAX_MSG_IDS      8192

// --------------------------------------------
// zopMsgT : ZOP Type
// --------------------------------------------
enum class zopMsgT : uint8_t {
  // Commented items are currently unused/unsupported
  Z_MZOP    = 0b00000,  /// FORZA MZOP
  Z_HZOPAC  = 0b00001,  /// FORZA HZOP ATOMICS/CUSTOM
  //Z_HZOPV  = 0b00010,  /// FORZA HZOP VECTOR
  //Z_RZOP   = 0b00011,  /// FORZA RZOP
  Z_MSG     = 0b00100,  /// FORZA MESSAGING
  Z_TMIG    = 0b00101,  /// FORZA THREAD MIGRATION
  //Z_TMGT   = 0b00110,  /// FORZA THREAD MANAGEMENT
  //Z_SYSC   = 0b00111,  /// FORZA SYSCALL
  Z_RESP    = 0b01000,  /// FORZA RESPONSE
  // -- 0b01001 - 0b01101 UNASSIGNED
  Z_FENCE   = 0b01110,  /// FORZE FENCE
  Z_EXCP    = 0b01111,  /// FORZA EXCEPTION
  // -- 0b10000 - 0b1111 UNASSIGNED
  Z_INVALID = 0b11111,  /// FORZA INVALID
};

// --------------------------------------------
// zopOpc : ZOP Opcode
// --------------------------------------------
enum class zopOpc : uint8_t {
  // -- MZOPs --
  Z_MZOP_LB            = 0b00000000,  /// zopOpc: MZOP Load unsigned byte
  Z_MZOP_LH            = 0b00000001,  /// zopOpc: MZOP Load unsigned half
  Z_MZOP_LW            = 0b00000010,  /// zopOpc: MZOP Load unsigned word
  Z_MZOP_LD            = 0b00000011,  /// zopOpc: MZOP Load doubleword
  Z_MZOP_LSB           = 0b00000100,  /// zopOpc: MZOP Load signed byte
  Z_MZOP_LSH           = 0b00000101,  /// zopOpc: MZOP Load signed half
  Z_MZOP_LSW           = 0b00000110,  /// zopOpc: MZOP Load signed word
  Z_MZOP_LDMA          = 0b00000111,  /// zopOpc: MZOP Load DMA
  Z_MZOP_SB            = 0b00001000,  /// zopOpc: MZOP Store unsigned byte
  Z_MZOP_SH            = 0b00001001,  /// zopOpc: MZOP Store unsigned half
  Z_MZOP_SW            = 0b00001010,  /// zopOpc: MZOP Store unsigned word
  Z_MZOP_SD            = 0b00001011,  /// zopOpc: MZOP Store doubleword
  Z_MZOP_SSB           = 0b00001100,  /// zopOpc: MZOP Store signed byte
  Z_MZOP_SSH           = 0b00001101,  /// zopOpc: MZOP Store signed half
  Z_MZOP_SSW           = 0b00001110,  /// zopOpc: MZOP Store signed word
  Z_MZOP_SDMA          = 0b00001111,  /// zopOpc: MSOP Store DMA

  // Next three are not currently implemented
  //Z_MZOP_MCOPY_RD     = 0b11100111,  /// zopOpc: MZOP MCOPY Read
  //Z_MZOP_MCOPY_WR     = 0b11101111,  /// zopOpc: MZOP MCOPY Write
  //Z_MZOP_FLUSH        = 0b11111111,  /// zopOpc: FLUSH (internal to RZA for flushing cache line)

  // -- HZOP ATOMIC/CUSTOM --
  // -- Encoding Styles: Z_HAC_width_type_OP
  // -- widths of 8, 16, 32, 64 (bits [1:0])
  // -- For the types below, bits [3:2]
  // -- 'B' = BASE-Type (aka U - update; update mem, return ack)
  // -- 'M' = M-Type (aka NN - both Rd and mem get result)
  // -- 'S' = S-Type (aka ON - mem unchanged, Rd gets result)
  // -- 'MS' = MS-Type (aka NO - mem gets result, Rd gets orig memory)
  // Function defined in bits [7:4] - function codes 0xB and 0xF unused
  Z_HAC_8_BASE_ADD     = 0x00,  /// zopOpc: HZOP-AC 8bit BASE AMO ADD
  Z_HAC_8_BASE_AND     = 0x10,  /// zopOpc: HZOP-AC 8bit BASE AMO AND
  Z_HAC_8_BASE_OR      = 0x20,  /// zopOpc: HZOP-AC 8bit BASE AMO OR
  Z_HAC_8_BASE_XOR     = 0x30,  /// zopOpc: HZOP-AC 8bit BASE AMO XOR
  Z_HAC_8_BASE_SMAX    = 0x40,  /// zopOpc: HZOP-AC 8bit BASE AMO Signed Max
  Z_HAC_8_BASE_MAX     = 0x50,  /// zopOpc: HZOP-AC 8bit BASE AMO Max
  Z_HAC_8_BASE_SMIN    = 0x60,  /// zopOpc: HZOP-AC 8bit BASE AMO Signed Min
  Z_HAC_8_BASE_MIN     = 0x70,  /// zopOpc: HZOP-AC 8bit BASE AMO Min
  Z_HAC_8_BASE_SWAP    = 0x80,  /// zopOpc: HZOP-AC 8bit BASE AMO SWAP
  Z_HAC_8_BASE_CAS     = 0x90,  /// zopOpc: HZOP-AC 8bit BASE AMO CAS
  Z_HAC_8_BASE_FADD    = 0xA0,  /// zopOpc: HZOP-AC 8bit BASE AMO FADD
  Z_HAC_8_BASE_FSUB    = 0xC0,  /// zopOpc: HZOP-AC 8bit BASE AMO FSUB
  Z_HAC_8_BASE_FRSUB   = 0xD0,  /// zopOpc: HZOP-AC 8bit BASE AMO FRSUB
  Z_HAC_8_BASE_THRESH  = 0xE0,  /// zopOpc: HZOP-AC 8bit BASE AMO THRESHOLD

  Z_HAC_16_BASE_ADD    = 0x01,  /// zopOpc: HZOP-AC 16bit BASE AMO ADD
  Z_HAC_16_BASE_AND    = 0x11,  /// zopOpc: HZOP-AC 16bit BASE AMO AND
  Z_HAC_16_BASE_OR     = 0x21,  /// zopOpc: HZOP-AC 16bit BASE AMO OR
  Z_HAC_16_BASE_XOR    = 0x31,  /// zopOpc: HZOP-AC 16bit BASE AMO XOR
  Z_HAC_16_BASE_SMAX   = 0x41,  /// zopOpc: HZOP-AC 16bit BASE AMO Signed Max
  Z_HAC_16_BASE_MAX    = 0x51,  /// zopOpc: HZOP-AC 16bit BASE AMO Max
  Z_HAC_16_BASE_SMIN   = 0x61,  /// zopOpc: HZOP-AC 16bit BASE AMO Signed Min
  Z_HAC_16_BASE_MIN    = 0x71,  /// zopOpc: HZOP-AC 16bit BASE AMO Min
  Z_HAC_16_BASE_SWAP   = 0x81,  /// zopOpc: HZOP-AC 16bit BASE AMO SWAP
  Z_HAC_16_BASE_CAS    = 0x91,  /// zopOpc: HZOP-AC 16bit BASE AMO CAS
  Z_HAC_16_BASE_FADD   = 0xA1,  /// zopOpc: HZOP-AC 16bit BASE AMO FADD
  Z_HAC_16_BASE_FSUB   = 0xC1,  /// zopOpc: HZOP-AC 16bit BASE AMO FSUB
  Z_HAC_16_BASE_FRSUB  = 0xD1,  /// zopOpc: HZOP-AC 16bit BASE AMO FRSUB
  Z_HAC_16_BASE_THRESH = 0xE1,  /// zopOpc: HZOP-AC 16bit BASE AMO THRESHOLD

  Z_HAC_32_BASE_ADD    = 0x02,  /// zopOpc: HZOP-AC 32bit BASE AMO ADD
  Z_HAC_32_BASE_AND    = 0x12,  /// zopOpc: HZOP-AC 32bit BASE AMO AND
  Z_HAC_32_BASE_OR     = 0x22,  /// zopOpc: HZOP-AC 32bit BASE AMO OR
  Z_HAC_32_BASE_XOR    = 0x32,  /// zopOpc: HZOP-AC 32bit BASE AMO XOR
  Z_HAC_32_BASE_SMAX   = 0x42,  /// zopOpc: HZOP-AC 32bit BASE AMO Signed Max
  Z_HAC_32_BASE_MAX    = 0x52,  /// zopOpc: HZOP-AC 32bit BASE AMO Max
  Z_HAC_32_BASE_SMIN   = 0x62,  /// zopOpc: HZOP-AC 32bit BASE AMO Signed Min
  Z_HAC_32_BASE_MIN    = 0x72,  /// zopOpc: HZOP-AC 32bit BASE AMO Min
  Z_HAC_32_BASE_SWAP   = 0x82,  /// zopOpc: HZOP-AC 32bit BASE AMO SWAP
  Z_HAC_32_BASE_CAS    = 0x92,  /// zopOpc: HZOP-AC 32bit BASE AMO CAS
  Z_HAC_32_BASE_FADD   = 0xA2,  /// zopOpc: HZOP-AC 32bit BASE AMO FADD
  Z_HAC_32_BASE_FSUB   = 0xC2,  /// zopOpc: HZOP-AC 32bit BASE AMO FSUB
  Z_HAC_32_BASE_FRSUB  = 0xD2,  /// zopOpc: HZOP-AC 32bit BASE AMO FRSUB
  Z_HAC_32_BASE_THRESH = 0xE2,  /// zopOpc: HZOP-AC 32bit BASE AMO THRESHOLD

  Z_HAC_64_BASE_ADD    = 0x03,  /// zopOpc: HZOP-AC 64bit BASE AMO ADD
  Z_HAC_64_BASE_AND    = 0x13,  /// zopOpc: HZOP-AC 64bit BASE AMO AND
  Z_HAC_64_BASE_OR     = 0x23,  /// zopOpc: HZOP-AC 64bit BASE AMO OR
  Z_HAC_64_BASE_XOR    = 0x33,  /// zopOpc: HZOP-AC 64bit BASE AMO XOR
  Z_HAC_64_BASE_SMAX   = 0x43,  /// zopOpc: HZOP-AC 64bit BASE AMO Signed Max
  Z_HAC_64_BASE_MAX    = 0x53,  /// zopOpc: HZOP-AC 64bit BASE AMO Max
  Z_HAC_64_BASE_SMIN   = 0x63,  /// zopOpc: HZOP-AC 64bit BASE AMO Signed Min
  Z_HAC_64_BASE_MIN    = 0x73,  /// zopOpc: HZOP-AC 64bit BASE AMO Min
  Z_HAC_64_BASE_SWAP   = 0x83,  /// zopOpc: HZOP-AC 64bit BASE AMO SWAP
  Z_HAC_64_BASE_CAS    = 0x93,  /// zopOpc: HZOP-AC 64bit BASE AMO CAS
  Z_HAC_64_BASE_FADD   = 0xA3,  /// zopOpc: HZOP-AC 64bit BASE AMO FADD
  Z_HAC_64_BASE_FSUB   = 0xC3,  /// zopOpc: HZOP-AC 64bit BASE AMO FSUB
  Z_HAC_64_BASE_FRSUB  = 0xD3,  /// zopOpc: HZOP-AC 64bit BASE AMO FRSUB
  Z_HAC_64_BASE_THRESH = 0xE3,  /// zopOpc: HZOP-AC 64bit BASE AMO THRESHOLD

  Z_HAC_8_M_ADD        = 0x04,  /// zopOpc: HZOP-AC 8bit M AMO ADD
  Z_HAC_8_M_AND        = 0x14,  /// zopOpc: HZOP-AC 8bit M AMO AND
  Z_HAC_8_M_OR         = 0x24,  /// zopOpc: HZOP-AC 8bit M AMO OR
  Z_HAC_8_M_XOR        = 0x34,  /// zopOpc: HZOP-AC 8bit M AMO XOR
  Z_HAC_8_M_SMAX       = 0x44,  /// zopOpc: HZOP-AC 8bit M AMO Signed Max
  Z_HAC_8_M_MAX        = 0x54,  /// zopOpc: HZOP-AC 8bit M AMO Max
  Z_HAC_8_M_SMIN       = 0x64,  /// zopOpc: HZOP-AC 8bit M AMO Signed Min
  Z_HAC_8_M_MIN        = 0x74,  /// zopOpc: HZOP-AC 8bit M AMO Min
  Z_HAC_8_M_SWAP       = 0x84,  /// zopOpc: HZOP-AC 8bit M AMO SWAP
  Z_HAC_8_M_CAS        = 0x94,  /// zopOpc: HZOP-AC 8bit M AMO CAS
  Z_HAC_8_M_FADD       = 0xA4,  /// zopOpc: HZOP-AC 8bit M AMO FADD
  Z_HAC_8_M_FSUB       = 0xC4,  /// zopOpc: HZOP-AC 8bit M AMO FSUB
  Z_HAC_8_M_FRSUB      = 0xD4,  /// zopOpc: HZOP-AC 8bit M AMO FRSUB
  Z_HAC_8_M_THRESH     = 0xE4,  /// zopOpc: HZOP-AC 8bit M AMO THRESHOLD

  Z_HAC_16_M_ADD       = 0x05,  /// zopOpc: HZOP-AC 16bit M AMO ADD
  Z_HAC_16_M_AND       = 0x15,  /// zopOpc: HZOP-AC 16bit M AMO AND
  Z_HAC_16_M_OR        = 0x25,  /// zopOpc: HZOP-AC 16bit M AMO OR
  Z_HAC_16_M_XOR       = 0x35,  /// zopOpc: HZOP-AC 16bit M AMO XOR
  Z_HAC_16_M_SMAX      = 0x45,  /// zopOpc: HZOP-AC 16bit M AMO Signed Max
  Z_HAC_16_M_MAX       = 0x55,  /// zopOpc: HZOP-AC 16bit M AMO Max
  Z_HAC_16_M_SMIN      = 0x65,  /// zopOpc: HZOP-AC 16bit M AMO Signed Min
  Z_HAC_16_M_MIN       = 0x75,  /// zopOpc: HZOP-AC 16bit M AMO Min
  Z_HAC_16_M_SWAP      = 0x85,  /// zopOpc: HZOP-AC 16bit M AMO SWAP
  Z_HAC_16_M_CAS       = 0x95,  /// zopOpc: HZOP-AC 16bit M AMO CAS
  Z_HAC_16_M_FADD      = 0xA5,  /// zopOpc: HZOP-AC 16bit M AMO FADD
  Z_HAC_16_M_FSUB      = 0xC5,  /// zopOpc: HZOP-AC 16bit M AMO FSUB
  Z_HAC_16_M_FRSUB     = 0xD5,  /// zopOpc: HZOP-AC 16bit M AMO FRSUB
  Z_HAC_16_M_THRESH    = 0xE5,  /// zopOpc: HZOP-AC 16bit M AMO THRESHOLD

  Z_HAC_32_M_ADD       = 0x06,  /// zopOpc: HZOP-AC 32bit M AMO ADD
  Z_HAC_32_M_AND       = 0x16,  /// zopOpc: HZOP-AC 32bit M AMO AND
  Z_HAC_32_M_OR        = 0x26,  /// zopOpc: HZOP-AC 32bit M AMO OR
  Z_HAC_32_M_XOR       = 0x36,  /// zopOpc: HZOP-AC 32bit M AMO XOR
  Z_HAC_32_M_SMAX      = 0x46,  /// zopOpc: HZOP-AC 32bit M AMO Signed Max
  Z_HAC_32_M_MAX       = 0x56,  /// zopOpc: HZOP-AC 32bit M AMO Max
  Z_HAC_32_M_SMIN      = 0x66,  /// zopOpc: HZOP-AC 32bit M AMO Signed Min
  Z_HAC_32_M_MIN       = 0x76,  /// zopOpc: HZOP-AC 32bit M AMO Min
  Z_HAC_32_M_SWAP      = 0x86,  /// zopOpc: HZOP-AC 32bit M AMO SWAP
  Z_HAC_32_M_CAS       = 0x96,  /// zopOpc: HZOP-AC 32bit M AMO CAS
  Z_HAC_32_M_FADD      = 0xA6,  /// zopOpc: HZOP-AC 32bit M AMO FADD
  Z_HAC_32_M_FSUB      = 0xC6,  /// zopOpc: HZOP-AC 32bit M AMO FSUB
  Z_HAC_32_M_FRSUB     = 0xD6,  /// zopOpc: HZOP-AC 32bit M AMO FRSUB
  Z_HAC_32_M_THRESH    = 0xE6,  /// zopOpc: HZOP-AC 32bit M AMO THRESHOLD

  Z_HAC_64_M_ADD       = 0x07,  /// zopOpc: HZOP-AC 64bit M AMO ADD
  Z_HAC_64_M_AND       = 0x17,  /// zopOpc: HZOP-AC 64bit M AMO AND
  Z_HAC_64_M_OR        = 0x27,  /// zopOpc: HZOP-AC 64bit M AMO OR
  Z_HAC_64_M_XOR       = 0x37,  /// zopOpc: HZOP-AC 64bit M AMO XOR
  Z_HAC_64_M_SMAX      = 0x47,  /// zopOpc: HZOP-AC 64bit M AMO Signed Max
  Z_HAC_64_M_MAX       = 0x57,  /// zopOpc: HZOP-AC 64bit M AMO Max
  Z_HAC_64_M_SMIN      = 0x67,  /// zopOpc: HZOP-AC 64bit M AMO Signed Min
  Z_HAC_64_M_MIN       = 0x77,  /// zopOpc: HZOP-AC 64bit M AMO Min
  Z_HAC_64_M_SWAP      = 0x87,  /// zopOpc: HZOP-AC 64bit M AMO SWAP
  Z_HAC_64_M_CAS       = 0x97,  /// zopOpc: HZOP-AC 64bit M AMO CAS
  Z_HAC_64_M_FADD      = 0xA7,  /// zopOpc: HZOP-AC 64bit M AMO FADD
  Z_HAC_64_M_FSUB      = 0xC7,  /// zopOpc: HZOP-AC 64bit M AMO FSUB
  Z_HAC_64_M_FRSUB     = 0xD7,  /// zopOpc: HZOP-AC 64bit M AMO FRSUB
  Z_HAC_64_M_THRESH    = 0xE7,  /// zopOpc: HZOP-AC 64bit M AMO THRESHOLD

  Z_HAC_8_S_ADD        = 0x08,  /// zopOpc: HZOP-AC 8bit S AMO ADD
  Z_HAC_8_S_AND        = 0x18,  /// zopOpc: HZOP-AC 8bit S AMO AND
  Z_HAC_8_S_OR         = 0x28,  /// zopOpc: HZOP-AC 8bit S AMO OR
  Z_HAC_8_S_XOR        = 0x38,  /// zopOpc: HZOP-AC 8bit S AMO XOR
  Z_HAC_8_S_SMAX       = 0x48,  /// zopOpc: HZOP-AC 8bit S AMO Signed Max
  Z_HAC_8_S_MAX        = 0x58,  /// zopOpc: HZOP-AC 8bit S AMO Max
  Z_HAC_8_S_SMIN       = 0x68,  /// zopOpc: HZOP-AC 8bit S AMO Signed Min
  Z_HAC_8_S_MIN        = 0x78,  /// zopOpc: HZOP-AC 8bit S AMO Min
  Z_HAC_8_S_SWAP       = 0x88,  /// zopOpc: HZOP-AC 8bit S AMO SWAP
  Z_HAC_8_S_CAS        = 0x98,  /// zopOpc: HZOP-AC 8bit S AMO CAS
  Z_HAC_8_S_FADD       = 0xA8,  /// zopOpc: HZOP-AC 8bit S AMO FADD
  Z_HAC_8_S_FSUB       = 0xC8,  /// zopOpc: HZOP-AC 8bit S AMO FSUB
  Z_HAC_8_S_FRSUB      = 0xD8,  /// zopOpc: HZOP-AC 8bit S AMO FRSUB
  Z_HAC_8_S_THRESH     = 0xE8,  /// zopOpc: HZOP-AC 8bit S AMO THRESHOLD

  Z_HAC_16_S_ADD       = 0x09,  /// zopOpc: HZOP-AC 16bit S AMO ADD
  Z_HAC_16_S_AND       = 0x19,  /// zopOpc: HZOP-AC 16bit S AMO AND
  Z_HAC_16_S_OR        = 0x29,  /// zopOpc: HZOP-AC 16bit S AMO OR
  Z_HAC_16_S_XOR       = 0x39,  /// zopOpc: HZOP-AC 16bit S AMO XOR
  Z_HAC_16_S_SMAX      = 0x49,  /// zopOpc: HZOP-AC 16bit S AMO Signed Max
  Z_HAC_16_S_MAX       = 0x59,  /// zopOpc: HZOP-AC 16bit S AMO Max
  Z_HAC_16_S_SMIN      = 0x69,  /// zopOpc: HZOP-AC 16bit S AMO Signed Min
  Z_HAC_16_S_MIN       = 0x79,  /// zopOpc: HZOP-AC 16bit S AMO Min
  Z_HAC_16_S_SWAP      = 0x89,  /// zopOpc: HZOP-AC 16bit S AMO SWAP
  Z_HAC_16_S_CAS       = 0x99,  /// zopOpc: HZOP-AC 16bit S AMO CAS
  Z_HAC_16_S_FADD      = 0xA9,  /// zopOpc: HZOP-AC 16bit S AMO FADD
  Z_HAC_16_S_FSUB      = 0xC9,  /// zopOpc: HZOP-AC 16bit S AMO FSUB
  Z_HAC_16_S_FRSUB     = 0xD9,  /// zopOpc: HZOP-AC 16bit S AMO FRSUB
  Z_HAC_16_S_THRESH    = 0xE9,  /// zopOpc: HZOP-AC 16bit S AMO THRESHOLD

  Z_HAC_32_S_ADD       = 0x0A,  /// zopOpc: HZOP-AC 32bit S AMO ADD
  Z_HAC_32_S_AND       = 0x1A,  /// zopOpc: HZOP-AC 32bit S AMO AND
  Z_HAC_32_S_OR        = 0x2A,  /// zopOpc: HZOP-AC 32bit S AMO OR
  Z_HAC_32_S_XOR       = 0x3A,  /// zopOpc: HZOP-AC 32bit S AMO XOR
  Z_HAC_32_S_SMAX      = 0x4A,  /// zopOpc: HZOP-AC 32bit S AMO Signed Max
  Z_HAC_32_S_MAX       = 0x5A,  /// zopOpc: HZOP-AC 32bit S AMO Max
  Z_HAC_32_S_SMIN      = 0x6A,  /// zopOpc: HZOP-AC 32bit S AMO Signed Min
  Z_HAC_32_S_MIN       = 0x7A,  /// zopOpc: HZOP-AC 32bit S AMO Min
  Z_HAC_32_S_SWAP      = 0x8A,  /// zopOpc: HZOP-AC 32bit S AMO SWAP
  Z_HAC_32_S_CAS       = 0x9A,  /// zopOpc: HZOP-AC 32bit S AMO CAS
  Z_HAC_32_S_FADD      = 0xAA,  /// zopOpc: HZOP-AC 32bit S AMO FADD
  Z_HAC_32_S_FSUB      = 0xCA,  /// zopOpc: HZOP-AC 32bit S AMO FSUB
  Z_HAC_32_S_FRSUB     = 0xDA,  /// zopOpc: HZOP-AC 32bit S AMO FRSUB
  Z_HAC_32_S_THRESH    = 0xEA,  /// zopOpc: HZOP-AC 32bit S AMO THRESHOLD

  Z_HAC_64_S_ADD       = 0x0B,  /// zopOpc: HZOP-AC 64bit S AMO ADD
  Z_HAC_64_S_AND       = 0x1B,  /// zopOpc: HZOP-AC 64bit S AMO AND
  Z_HAC_64_S_OR        = 0x2B,  /// zopOpc: HZOP-AC 64bit S AMO OR
  Z_HAC_64_S_XOR       = 0x3B,  /// zopOpc: HZOP-AC 64bit S AMO XOR
  Z_HAC_64_S_SMAX      = 0x4B,  /// zopOpc: HZOP-AC 64bit S AMO Signed Max
  Z_HAC_64_S_MAX       = 0x5B,  /// zopOpc: HZOP-AC 64bit S AMO Max
  Z_HAC_64_S_SMIN      = 0x6B,  /// zopOpc: HZOP-AC 64bit S AMO Signed Min
  Z_HAC_64_S_MIN       = 0x7B,  /// zopOpc: HZOP-AC 64bit S AMO Min
  Z_HAC_64_S_SWAP      = 0x8B,  /// zopOpc: HZOP-AC 64bit S AMO SWAP
  Z_HAC_64_S_CAS       = 0x9B,  /// zopOpc: HZOP-AC 64bit S AMO CAS
  Z_HAC_64_S_FADD      = 0xAB,  /// zopOpc: HZOP-AC 64bit S AMO FADD
  Z_HAC_64_S_FSUB      = 0xCB,  /// zopOpc: HZOP-AC 64bit S AMO FSUB
  Z_HAC_64_S_FRSUB     = 0xDB,  /// zopOpc: HZOP-AC 64bit S AMO FRSUB
  Z_HAC_64_S_THRESH    = 0xEB,  /// zopOpc: HZOP-AC 64bit S AMO THRESHOLD

  Z_HAC_8_MS_ADD       = 0x0C,  /// zopOpc: HZOP-AC 8bit MS AMO ADD
  Z_HAC_8_MS_AND       = 0x1C,  /// zopOpc: HZOP-AC 8bit MS AMO AND
  Z_HAC_8_MS_OR        = 0x2C,  /// zopOpc: HZOP-AC 8bit MS AMO OR
  Z_HAC_8_MS_XOR       = 0x3C,  /// zopOpc: HZOP-AC 8bit MS AMO XOR
  Z_HAC_8_MS_SMAX      = 0x4C,  /// zopOpc: HZOP-AC 8bit MS AMO Signed Max
  Z_HAC_8_MS_MAX       = 0x5C,  /// zopOpc: HZOP-AC 8bit MS AMO Max
  Z_HAC_8_MS_SMIN      = 0x6C,  /// zopOpc: HZOP-AC 8bit MS AMO Signed Min
  Z_HAC_8_MS_MIN       = 0x7C,  /// zopOpc: HZOP-AC 8bit MS AMO Min
  Z_HAC_8_MS_SWAP      = 0x8C,  /// zopOpc: HZOP-AC 8bit MS AMO SWAP
  Z_HAC_8_MS_CAS       = 0x9C,  /// zopOpc: HZOP-AC 8bit MS AMO CAS
  Z_HAC_8_MS_FADD      = 0xAC,  /// zopOpc: HZOP-AC 8bit MS AMO FADD
  Z_HAC_8_MS_FSUB      = 0xCC,  /// zopOpc: HZOP-AC 8bit MS AMO FSUB
  Z_HAC_8_MS_FRSUB     = 0xDC,  /// zopOpc: HZOP-AC 8bit MS AMO FRSUB
  Z_HAC_8_MS_THRESH    = 0xEC,  /// zopOpc: HZOP-AC 8bit MS AMO THRESHOLD

  Z_HAC_16_MS_ADD      = 0x0D,  /// zopOpc: HZOP-AC 16bit MS AMO ADD
  Z_HAC_16_MS_AND      = 0x1D,  /// zopOpc: HZOP-AC 16bit MS AMO AND
  Z_HAC_16_MS_OR       = 0x2D,  /// zopOpc: HZOP-AC 16bit MS AMO OR
  Z_HAC_16_MS_XOR      = 0x3D,  /// zopOpc: HZOP-AC 16bit MS AMO XOR
  Z_HAC_16_MS_SMAX     = 0x4D,  /// zopOpc: HZOP-AC 16bit MS AMO Signed Max
  Z_HAC_16_MS_MAX      = 0x5D,  /// zopOpc: HZOP-AC 16bit MS AMO Max
  Z_HAC_16_MS_SMIN     = 0x6D,  /// zopOpc: HZOP-AC 16bit MS AMO Signed Min
  Z_HAC_16_MS_MIN      = 0x7D,  /// zopOpc: HZOP-AC 16bit MS AMO Min
  Z_HAC_16_MS_SWAP     = 0x8D,  /// zopOpc: HZOP-AC 16bit MS AMO SWAP
  Z_HAC_16_MS_CAS      = 0x9D,  /// zopOpc: HZOP-AC 16bit MS AMO CAS
  Z_HAC_16_MS_FADD     = 0xAD,  /// zopOpc: HZOP-AC 16bit MS AMO FADD
  Z_HAC_16_MS_FSUB     = 0xCD,  /// zopOpc: HZOP-AC 16bit MS AMO FSUB
  Z_HAC_16_MS_FRSUB    = 0xDD,  /// zopOpc: HZOP-AC 16bit MS AMO FRSUB
  Z_HAC_16_MS_THRESH   = 0xED,  /// zopOpc: HZOP-AC 16bit MS AMO THRESHOLD

  Z_HAC_32_MS_ADD      = 0x0E,  /// zopOpc: HZOP-AC 32bit MS AMO ADD
  Z_HAC_32_MS_AND      = 0x1E,  /// zopOpc: HZOP-AC 32bit MS AMO AND
  Z_HAC_32_MS_OR       = 0x2E,  /// zopOpc: HZOP-AC 32bit MS AMO OR
  Z_HAC_32_MS_XOR      = 0x3E,  /// zopOpc: HZOP-AC 32bit MS AMO XOR
  Z_HAC_32_MS_SMAX     = 0x4E,  /// zopOpc: HZOP-AC 32bit MS AMO Signed Max
  Z_HAC_32_MS_MAX      = 0x5E,  /// zopOpc: HZOP-AC 32bit MS AMO Max
  Z_HAC_32_MS_SMIN     = 0x6E,  /// zopOpc: HZOP-AC 32bit MS AMO Signed Min
  Z_HAC_32_MS_MIN      = 0x7E,  /// zopOpc: HZOP-AC 32bit MS AMO Min
  Z_HAC_32_MS_SWAP     = 0x8E,  /// zopOpc: HZOP-AC 32bit MS AMO SWAP
  Z_HAC_32_MS_CAS      = 0x9E,  /// zopOpc: HZOP-AC 32bit MS AMO CAS
  Z_HAC_32_MS_FADD     = 0xAE,  /// zopOpc: HZOP-AC 32bit MS AMO FADD
  Z_HAC_32_MS_FSUB     = 0xCE,  /// zopOpc: HZOP-AC 32bit MS AMO FSUB
  Z_HAC_32_MS_FRSUB    = 0xDE,  /// zopOpc: HZOP-AC 32bit MS AMO FRSUB
  Z_HAC_32_MS_THRESH   = 0xEE,  /// zopOpc: HZOP-AC 32bit MS AMO THRESHOLD

  Z_HAC_64_MS_ADD      = 0x0F,  /// zopOpc: HZOP-AC 64bit MS AMO ADD
  Z_HAC_64_MS_AND      = 0x1F,  /// zopOpc: HZOP-AC 64bit MS AMO AND
  Z_HAC_64_MS_OR       = 0x2F,  /// zopOpc: HZOP-AC 64bit MS AMO OR
  Z_HAC_64_MS_XOR      = 0x3F,  /// zopOpc: HZOP-AC 64bit MS AMO XOR
  Z_HAC_64_MS_SMAX     = 0x4F,  /// zopOpc: HZOP-AC 64bit MS AMO Signed Max
  Z_HAC_64_MS_MAX      = 0x5F,  /// zopOpc: HZOP-AC 64bit MS AMO Max
  Z_HAC_64_MS_SMIN     = 0x6F,  /// zopOpc: HZOP-AC 64bit MS AMO Signed Min
  Z_HAC_64_MS_MIN      = 0x7F,  /// zopOpc: HZOP-AC 64bit MS AMO Min
  Z_HAC_64_MS_SWAP     = 0x8F,  /// zopOpc: HZOP-AC 64bit MS AMO SWAP
  Z_HAC_64_MS_CAS      = 0x9F,  /// zopOpc: HZOP-AC 64bit MS AMO CAS
  Z_HAC_64_MS_FADD     = 0xAF,  /// zopOpc: HZOP-AC 64bit MS AMO FADD
  Z_HAC_64_MS_FSUB     = 0xCF,  /// zopOpc: HZOP-AC 64bit MS AMO FSUB
  Z_HAC_64_MS_FRSUB    = 0xDF,  /// zopOpc: HZOP-AC 64bit MS AMO FRSUB
  Z_HAC_64_MS_THRESH   = 0xEF,  /// zopOpc: HZOP-AC 64bit MS AMO THRESHOLD

  // -- MESSAGING --
  Z_MSG_SENDP          = 0b00000000,  /// zopOpc: MESSAGING Send with payload
  //Z_MSG_SENDAS        = 0b00000001,  /// zopOpc: MESSAGING Send with address and size -- Unused; Delete?
  Z_MSG_ACK            = 0b11110000,  /// zopOpc: MESSAGING Send Ack
  Z_MSG_NACK           = 0b11110001,  /// zopOpc: MESSAGING Send Nack
  Z_MSG_EXCP           = 0b11110010,  /// zopOpc: MESSAGING Send exception
  Z_MSG_ZBAR           = 0b11111001,  /// zopOpc: MESSAGING Zone Barrier Request

  // -- THREAD MIGRATION --
  Z_TMIG_INTREGS       = 0b00000000,  /// zopOpc: THREAD MIGRATION migrate state + int regs
  Z_TMIG_FPREGS        = 0b00000001,  /// zopOpc: THREAD MIGRATION migrate state + int regs + fp regs
  Z_TMIG_SPAWN         = 0b00000010,  /// zopOpc: THREAD MIGRATION Spawned thread
  Z_TMIG_REQUEST       = 0b00000100,  /// zopOpc: THREAD MIGRATION ZAP requesting a thread

  // -- RZA RESPONSE --
  // TODO: Update these and/or exception codes (need to review how flags are returned)
  Z_RESP_LR            = 0b00000000,  /// zopOpc: RZA RESPONSE Load response
  Z_RESP_LEXCP         = 0b00000001,  /// zopOpc: RZA RESPONSE Load exception
  Z_RESP_SACK          = 0b00000010,  /// zopOpc: RZA RESPONSE Store ack
  Z_RESP_SEXCP         = 0b00000011,  /// zopOpc: RZA RESPONSE Store exception
  Z_RESP_HRESP         = 0b00000100,  /// zopOpc: RZA RESPONSE HZOP response
  Z_RESP_HEXCP         = 0b00000101,  /// zopOpc: RZA RESPONSE HZOP exception
  Z_RESP_RRESP         = 0b00000110,  /// zopOpc: RZA RESPONSE RZOP response
  Z_RESP_REXCP         = 0b00000111,  /// zopOpc: RZA RESPONSE RZOP exception

  // -- FENCE --
  Z_FENCE_HART         = 0b00000000,  /// zopOpc: HART Fence (only fences the calling HART)
  Z_FENCE_ZAP          = 0b00000001,  /// zopOpc: ZAP Fence (fences all HARTs on a ZAP)
  Z_FENCE_RZA          = 0b00000010,  /// zopOpc: RZA Fence (fences all requests on an RZA)

  // -- EXCEPTION --
  Z_EXCP_NONE          = 0b00000000,  /// zopOpc: Exception; no exception
  Z_EXCP_INVENDP       = 0b00000001,  /// zopOpc: Exception; Invalid endpoint
  Z_EXCP_INVO          = 0b00000010,  /// zopOpc: Exception; Invalid operation
  Z_EXCP_FINVO         = 0b10000000,  /// zopOpc: Exception; Invalid floating point operation
  Z_EXCP_FDIVZ         = 0b10000001,  /// zopOpc: Exception; Floating point division by zero
  Z_EXCP_FOVR          = 0b10000010,  /// zopOpc: Exception; Floating point overlow
  Z_EXCP_FUND          = 0b10000011,  /// zopOpc: Exception; Floating point underflow
  Z_EXCP_FINXT         = 0b10000100,  /// zopOpc: Exception; Floating point inexact

  Z_NULL_OPC           = 0b11111111,  /// zopOpc: null opcode

};

// --------------------------------------------
// zopCompID: ZOP Component ID
// Note: ZQM (and other components?) assumes the ZAPs
//    are always 0-(max-1)
// --------------------------------------------
enum class zopCompID : uint8_t {
  Z_ZAP0     = 0b00000000,  /// zopCompID: ZAP0
  Z_ZAP1     = 0b00000001,  /// zopCompID: ZAP1
  Z_ZAP2     = 0b00000010,  /// zopCompID: ZAP2
  Z_ZAP3     = 0b00000011,  /// zopCompID: ZAP3
  Z_RZA      = 0b00001000,  /// zopCompID: RZA
  // TODO: Eventually add multiple RZAs
  Z_ZQM      = 0b00001010,  /// zopCompID: ZQM // RTL value is 9
  Z_ZEN      = 0b00001100,  /// zopCompID: ZEN // RTL value is 8
  Z_PREC_ZIP = 0b00001110,  /// zopCompID: PRECINCT ZIP - update doc?
  Z_UNK_COMP = 0b11111111,  /// zopCompID:: Z_UNK_COMP - mostly needed for initialization
};

// --------------------------------------------
// zopPrecID : ZOP Precinct Component ID
// --------------------------------------------
enum class zopPrecID : uint8_t {
  Z_ZONE0 = 0b00000000,  /// zopPrecID: ZONE0
  Z_ZONE1 = 0b00000001,  /// zopPrecID: ZONE1
  Z_ZONE2 = 0b00000010,  /// zopPrecID: ZONE2
  Z_ZONE3 = 0b00000011,  /// zopPrecID: ZONE3
  Z_ZONE4 = 0b00000100,  /// zopPrecID: ZONE4
  Z_ZONE5 = 0b00000101,  /// zopPrecID: ZONE5
  Z_ZONE6 = 0b00000110,  /// zopPrecID: ZONE6
  Z_ZONE7 = 0b00000111,  /// zopPrecID: ZONE7
  Z_PMP   = 0b00001000,  /// zopPrecID: PMP
  Z_ZIP   = 0b00001001,  /// zopPrecID: ZIP aka PIP
  Z_UNK   = 0b11111111   /// zopPrecID: UNKNOWN
};

// --------------------------------------------
// zopMsgID
// --------------------------------------------
class zopMsgID {
public:
  // zopMsgID: constructor
  zopMsgID() : NumFree( Z_MAX_MSG_IDS ) {
    for( bool& i : Mask ) {
      i = false;
    }
  }

  // zopMsgID: destructor
  ~zopMsgID() = default;

  // zopMsgID: get the number of free message slots
  uint16_t getNumFree() const { return NumFree; }

  /// zopMsgID: clear the message id
  void clearMsgId( uint16_t I ) {
    Mask[I] = false;
    NumFree++;
  }

  // zopMsgID: retrieve a new message id
  uint16_t getMsgId() {
    for( uint16_t i = 0; i < Z_MAX_MSG_IDS; i++ ) {
      if( Mask[i] == false ) {
        NumFree--;
        Mask[i] = true;
        return i;
      }
    }
    return Z_MAX_MSG_IDS;  // this is an erroneous id
  }

  // Return an empty vector if not enough IDs available
  std::vector<uint16_t> getSetOfMsgIds( uint16_t num_to_get ) {
    std::vector<uint16_t> v;
    if( num_to_get <= NumFree ) {
      for( uint16_t i = 0; i < num_to_get; i++ )
        v.push_back( this->getMsgId() );
    }
    return v;
  }

private:
  uint16_t NumFree;
  bool     Mask[Z_MAX_MSG_IDS]{};
};

// --------------------------------------------
// zopEvent
// --------------------------------------------
class zopEvent : public SST::Event {
public:
  // TODO: constructor should create 3+ words
  // Note: all constructors create two full FLITs
  // This is comprised of 2 x 64bit words

  /// zopEvent: init broadcast constructor
  //  This constructor is ONLY utilized for initialization
  //  DO NOT use this constructor for normal packet construction
  explicit zopEvent( unsigned srcId, zopCompID Type, unsigned ZoneID, unsigned PrecinctID ) : Event() {
    Packet.push_back( (uint64_t) ( Type ) );
    Packet.push_back( (uint64_t) ( srcId ) );
    Packet.push_back( (uint64_t) ( ZoneID ) );
    Packet.push_back( (uint64_t) ( PrecinctID ) );
  }

  /// zopEvent: raw event constructor
  explicit zopEvent() : Event() {
    Packet.push_back( 0x00ull );
    Packet.push_back( 0x00ull );
  }

  explicit zopEvent( zopMsgT T, zopOpc O ) : Event() {
    Packet.push_back( 0x00ul );
    Packet.push_back( 0x00ul );
    Type = T;
    Opc  = O;
  }

  /// zopEvent: virtual function to clone an event
  virtual Event* clone( void ) override {
    zopEvent* ev = new zopEvent( *this );
    return ev;
  }

  /// zopEvent: retrieve the raw packet
  std::vector<uint64_t> const getPacket() { return Packet; }

  /// zopEvent: set the memory request handler
  void setMemReq( const SST::RevCPU::MemReq& r ) {
    req  = r;
    Read = true;
  }

  /// zopEvent: set the target address for the read
  void setTarget( uint64_t* T ) { Target = T; }

  /// zopEvent: set the packet payload.  NOTE: this is a destructive operation, but it does reset the size
  void setPacket( const std::vector<uint64_t> P ) { Packet = std::move( P ); }

  /// zopEvent: set the packet packet payload w/o the header. NOT a destructive operation
  void setPayload( const std::vector<uint64_t> P ) {
    for( auto i : P ) {
      Packet.push_back( i );
    }
  }

  /// zopEvent: clear the packet payload
  void clearPacket() { Packet.clear(); }

  /// zopEvent: set the packet type
  void setType( zopMsgT T ) { Type = T; }

  /// zopEvent: set the sequence (flit) number
  void setSeqNum( uint8_t SN ) { SeqNum = SN; }

  /// zopEvent: set the packet ID
  void setID( uint16_t I ) { ID = I; }

  /// zopEvent: set the credit
  void setMboxID( uint8_t MB ) { MbxID = MB; }  // formerly setCredit

  /// zopEvent: set the opcode
  void setOpc( zopOpc O ) { Opc = O; }

  /// zopEvent: set the application id
  void setAppID( uint8_t A ) { AppID = A & Z_MASK_APPID; }

  /// zopEvent: set the packet reserved
  void setRingLevel( uint8_t RL ) { RingLvl = RL; }  // CURRENTLY USED AS RETRY NUMBER for messages/(n)acks

  /// zopEvent: set the dest Addr
  void setAddr( uint64_t A ) { Addr = A; }

  /// zopEvent: set the destination hart
  void setDestHart( uint16_t H ) { DestHart = H; }

  /// zopEvent: set the destination ZCID
  template<typename T>
  void setDestZCID( T Z ) {
    DestZCID = static_cast<uint8_t>( Z );
  }

  /// zopEvent: set the destination PCID
  template<typename T>
  void setDestPCID( T P ) {
    DestPCID = static_cast<uint8_t>( P );
  }

  /// zopEvent: set the destination precinct
  void setDestPrec( uint16_t P ) { DestPrec = P; }

  /// zopEvent: set the src hart
  void setSrcHart( uint16_t H ) { SrcHart = H; }

  /// zopEvent: set the src ZCID
  template<typename T>
  void setSrcZCID( T Z ) {
    SrcZCID = static_cast<uint8_t>( Z );
  }

  /// zopEvent: set the src PCID
  template<typename T>
  void setSrcPCID( T P ) {
    SrcPCID = static_cast<uint8_t>( P );
  }

  /// zopEvent: set the src precinct
  void setSrcPrec( uint16_t P ) { SrcPrec = P; }

  /// zopEvent: set the fence encountered flag
  void setFence() { FenceEncountered = true; }

  /// zopEvent: sets the ClearMsgID flag: default=false;
  ///           true=manually triggers msgID clearing back to the caller
  void setClearMsgID( bool r ) { ClearMsgID = r; }

  /// zopEvent: retrieve the data payload from the packet
  std::vector<uint64_t> getPayload() {
    std::vector<uint64_t> P;
    for( unsigned i = 2; i < Packet.size(); i++ ) {
      P.push_back( Packet[i] );
    }
    return P;
  }

  /// zopEvent: set the full source info
  void setFullSrc( uint16_t Hart, zopCompID zoneComp, zopPrecID precComp, uint16_t Prec ) {
    SrcHart = Hart;
    SrcZCID = RevCPU::safe_static_cast<uint8_t>( zoneComp );
    SrcPrec = RevCPU::safe_static_cast<uint8_t>( precComp );
    SrcPrec = Prec;
  }

  void setFullDest( uint16_t Hart, zopCompID zoneComp, zopPrecID precComp, uint16_t Prec ) {
    DestHart = Hart;
    DestZCID = RevCPU::safe_static_cast<uint8_t>( zoneComp );
    DestPrec = RevCPU::safe_static_cast<uint8_t>( precComp );
    DestPrec = Prec;
  }

  /// zopEvent: get the length of the full packet (header + payload)
  uint8_t getPacketLength() { return ( Length + Z_NUM_HEADER_FLITS ); }

  /// zopEvent: get the memory request handler
  const SST::RevCPU::MemReq& getMemReq() { return req; }

  /// zopEvent: determines if the target request is a read or AMO request
  bool isRead() { return Read; }

  /// zopEvent: get the target for the read request
  uint64_t* getTarget() { return Target; }

  /// zopEvent: get the destination Hart
  uint16_t getDestHart() { return DestHart; }

  /// zopEvent: get the destination ZCID
  uint8_t getDestZCID() { return DestZCID; }

  /// zopEvent: get the destination PCID
  uint8_t getDestPCID() { return DestPCID; }

  /// zopEvent: get the destination precinct
  uint16_t getDestPrec() { return DestPrec; }

  /// zopEvent: get the source Hart
  uint16_t getSrcHart() { return SrcHart; }

  /// zopEvent: get the source ZCID
  uint8_t getSrcZCID() { return SrcZCID; }

  /// zopEvent: get the source PCID
  uint8_t getSrcPCID() { return SrcPCID; }

  /// zopEvent: get the source precinct
  uint16_t getSrcPrec() { return SrcPrec; }

  /// zopEvent: get the packet type
  zopMsgT getType() { return Type; }

  /// zopEvent: get the payload length - does NOT include the two header words
  uint8_t getLength() { return Length; }

  /// zopEvent: get which flit this is in the transaction
  uint8_t getSeqNum() { return SeqNum; }

  /// zopEvent: get the packet ID
  uint16_t getID() { return ID; }

  /// zopEvent: get the credit
  uint8_t getMbxID() { return MbxID; }  // formerly getCredit

  /// zopEvent: get the opcode
  zopOpc getOpc() { return Opc; }

  /// zopEvent: get the application id
  uint8_t getAppID() { return AppID; }

  /// zopEvent: get the packet reserved field
  uint32_t getRingLevel() { return RingLvl; }  // formerly getPktRes

  /// zopEvent: get the dest address
  uint64_t getAddr() { return Addr; }

  /// zopEvent: determine whether the fence has been encountered
  bool getFence() { return FenceEncountered; }

  /// zopEvent: retrieve the FLIT at the target location
  bool getFLIT( unsigned flit, uint64_t* F ) {
    if( flit > ( Packet.size() - 1 ) ) {
      return false;
    } else if( F == nullptr ) {
      return false;
    }

    *F = Packet[flit];

    return true;
  }

  /// zopEvent: decode this event and set the appropriate internal structures
  void decodeEvent() {
    DestHart = (uint16_t) ( ( Packet[Z_FLIT_DEST] >> Z_SHIFT_HARTID ) & Z_MASK_HARTID );
    DestZCID = (uint8_t) ( ( Packet[Z_FLIT_DEST] >> Z_SHIFT_ZCID ) & Z_MASK_ZCID );
    DestPCID = (uint8_t) ( ( Packet[Z_FLIT_DEST] >> Z_SHIFT_PCID ) & Z_MASK_PCID );
    DestPrec = (uint16_t) ( ( Packet[Z_FLIT_DEST] >> Z_SHIFT_PRECINCT ) & Z_MASK_PRECINCT );

    Opc      = (zopOpc) ( ( Packet[Z_FLIT_OPC] >> Z_SHIFT_OPC ) & Z_MASK_OPC );
    MbxID    = (uint8_t) ( ( Packet[Z_FLIT_MBXID] >> Z_SHIFT_MBXID ) & Z_MASK_MBXID );
    ID       = (uint16_t) ( ( Packet[Z_FLIT_MSGID] >> Z_SHIFT_MSGID ) & Z_MASK_MSGID );
    Length   = (uint8_t) ( ( Packet[Z_FLIT_FLITLEN] >> Z_SHIFT_FLITLEN ) & Z_MASK_FLITLEN );
    SeqNum   = (uint8_t) ( ( Packet[Z_FLIT_SEQNUM] >> Z_SHIFT_SEQNUM ) & Z_MASK_SEQNUM );
    Type     = (zopMsgT) ( ( Packet[Z_FLIT_TYPE] >> Z_SHIFT_TYPE ) & Z_MASK_TYPE );
    AppID    = (uint8_t) ( ( Packet[Z_FLIT_APPID] >> Z_SHIFT_APPID ) & Z_MASK_APPID );
    RingLvl  = (uint8_t) ( ( Packet[Z_FLIT_RINGLVL] >> Z_SHIFT_RINGLVL ) & Z_MASK_RINGLVL );

    SrcHart  = (uint16_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_HARTID ) & Z_MASK_HARTID );
    SrcZCID  = (uint8_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_ZCID ) & Z_MASK_ZCID );
    SrcPCID  = (uint8_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_PCID ) & Z_MASK_PCID );
    SrcPrec  = (uint16_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_PRECINCT ) & Z_MASK_PRECINCT );

    Addr     = ( ( Packet[Z_FLIT_ADDR] >> Z_SHIFT_ADDR ) & Z_MASK_ADDR );
  }

  /// zopEvent: encode this event and set the appropriate internal packet structures
  void encodeEvent() {
    Length = Packet.size() - Z_NUM_HEADER_FLITS;
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestHart & Z_MASK_HARTID ) << Z_SHIFT_HARTID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestZCID & Z_MASK_ZCID ) << Z_SHIFT_ZCID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestPCID & Z_MASK_PCID ) << Z_SHIFT_PCID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestPrec & Z_MASK_PRECINCT ) << Z_SHIFT_PRECINCT );

    Packet[Z_FLIT_OPC] |= ( ( RevCPU::safe_static_cast<uint64_t>( Opc ) & Z_MASK_OPC ) << Z_SHIFT_OPC );
    Packet[Z_FLIT_MBXID] |= ( (uint64_t) ( MbxID & Z_MASK_MBXID ) << Z_SHIFT_MBXID );
    Packet[Z_FLIT_MSGID] |= ( (uint64_t) ( ID & Z_MASK_MSGID ) << Z_SHIFT_MSGID );
    Packet[Z_FLIT_FLITLEN] |= ( (uint64_t) ( Length & Z_MASK_FLITLEN ) << Z_SHIFT_FLITLEN );
    Packet[Z_FLIT_SEQNUM] |= ( (uint64_t) ( SeqNum & Z_MASK_SEQNUM ) << Z_SHIFT_SEQNUM );
    Packet[Z_FLIT_TYPE] |= ( ( RevCPU::safe_static_cast<uint64_t>( Type ) & Z_MASK_TYPE ) << Z_SHIFT_TYPE );
    Packet[Z_FLIT_APPID] |= ( (uint64_t) ( AppID & Z_MASK_APPID ) << Z_SHIFT_APPID );
    Packet[Z_FLIT_RINGLVL] |= ( (uint64_t) ( RingLvl & Z_MASK_RINGLVL ) << Z_SHIFT_RINGLVL );

    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcHart & Z_MASK_HARTID ) << Z_SHIFT_HARTID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcZCID & Z_MASK_ZCID ) << Z_SHIFT_ZCID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcPCID & Z_MASK_PCID ) << Z_SHIFT_PCID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcPrec & Z_MASK_PRECINCT ) << Z_SHIFT_PRECINCT );

    Packet[Z_FLIT_ADDR] |= ( ( Addr & Z_MASK_ADDR ) << Z_SHIFT_ADDR );
  }

  std::string getSrcString() {
    std::stringstream opc_ss;
    opc_ss << "0x" << std::hex << RevCPU::safe_static_cast<uint8_t>( Opc );
    std::string str = "Src[hart:zcid:pcid:type:opc]=[";
    str += std::to_string( SrcHart ) + ":";
    str += ZCIDToStr( SrcZCID ) + ":";
    str += PCIDToStr( SrcPCID ) + ":";
    str += msgTToStr( Type ) + ":";
    str += opc_ss.str() + "]";
    return str;
  }

  std::string getDestString() {
    std::string str = "Dest[hart:zcid:pcid:type]=[";
    str += std::to_string( DestHart ) + ":";
    str += ZCIDToStr( DestZCID ) + ":";
    str += PCIDToStr( DestPCID ) + ":";
    str += msgTToStr( Type ) + "]";
    return str;
  }

  /// zopEvent: convert endpoint to string name
  std::string ZCIDToStr( zopCompID T ) {
    switch( T ) {
    case zopCompID::Z_ZAP0: return "ZAP0";
    case zopCompID::Z_ZAP1: return "ZAP1";
    case zopCompID::Z_ZAP2: return "ZAP2";
    case zopCompID::Z_ZAP3: return "ZAP3";
    case zopCompID::Z_RZA: return "RZA";
    case zopCompID::Z_ZEN: return "ZEN";
    case zopCompID::Z_ZQM: return "ZQM";
    case zopCompID::Z_PREC_ZIP: return "PREC_ZIP";
    default: return "UNKNOWN";
    }
  }

  std::string ZCIDToStr( uint8_t T ) {
    auto zcid = static_cast<zopCompID>( T );
    return ZCIDToStr( zcid );
  }

  /// zopAPI: convert the precinct ID to zopPrecID
  zopPrecID getPCID( unsigned Z ) {
    switch( Z ) {
    case 0: return zopPrecID::Z_ZONE0;
    case 1: return zopPrecID::Z_ZONE1;
    case 2: return zopPrecID::Z_ZONE2;
    case 3: return zopPrecID::Z_ZONE3;
    case 4: return zopPrecID::Z_ZONE4;
    case 5: return zopPrecID::Z_ZONE5;
    case 6: return zopPrecID::Z_ZONE6;
    case 7: return zopPrecID::Z_ZONE7;
    case 8: return zopPrecID::Z_PMP;
    case 9: return zopPrecID::Z_ZIP;
    default: return zopPrecID::Z_UNK;
    }
  }

  std::string PCIDToStr( zopPrecID P ) {
    switch( P ) {
    case zopPrecID::Z_ZONE0: return "Zone0";
    case zopPrecID::Z_ZONE1: return "Zone1";
    case zopPrecID::Z_ZONE2: return "Zone2";
    case zopPrecID::Z_ZONE3: return "Zone3";
    case zopPrecID::Z_ZONE4: return "Zone4";
    case zopPrecID::Z_ZONE5: return "Zone5";
    case zopPrecID::Z_ZONE6: return "Zone6";
    case zopPrecID::Z_ZONE7: return "Zone7";
    case zopPrecID::Z_PMP: return "PMP";
    case zopPrecID::Z_ZIP: return "ZIP";
    default: return "UNKNOWN";
    }
  }

  std::string PCIDToStr( unsigned P ) { return PCIDToStr( static_cast<zopPrecID>( P ) ); }

  /// zopEvent : convert message type to string name
  std::string msgTToStr( zopMsgT T ) {
    switch( T ) {
    case zopMsgT::Z_MZOP: return "MZOP";
    case zopMsgT::Z_HZOPAC: return "HZOPAC";
    //case zopMsgT::Z_HZOPV: return "HZOPV";
    //case zopMsgT::Z_RZOP: return "RZOP";
    case zopMsgT::Z_MSG: return "MSG";
    case zopMsgT::Z_TMIG: return "TMIG";
    //case zopMsgT::Z_TMGT: return "TMGT";
    //case zopMsgT::Z_SYSC: return "SYSC";
    case zopMsgT::Z_RESP: return "RESP";
    case zopMsgT::Z_FENCE: return "FENCE";
    case zopMsgT::Z_EXCP: return "EXCP";
    default: return "UNKNOWN";
    }
  }

private:
  std::vector<uint64_t> Packet;  ///< zopEvent: data payload: serialized payload

  // -- private, non-serialized data members
  uint16_t DestHart{};             ///< zopEvent: destination hart id
  uint8_t  DestZCID{ UINT8_MAX };  ///< zopEvent: destination ZCID
  uint8_t  DestPCID{ UINT8_MAX };  ///< zopEvent: destination PCID
  uint16_t DestPrec{};             ///< zopEvent: destination Precinct
  uint16_t SrcHart{};              ///< zopEvent: src hart id
  uint8_t  SrcZCID{ UINT8_MAX };   ///< zopEvent: src ZCID
  uint8_t  SrcPCID{ UINT8_MAX };   ///< zopEvent: src PCID
  uint16_t SrcPrec{};              ///< zopEvent: src Precinct

  zopMsgT  Type{ zopMsgT::Z_INVALID };  ///< zopEvent: message type
  uint8_t  Length{};                    ///< zopEvent: packet length (in flits)
  uint8_t  SeqNum{};                    ///< zopEvent: sequence ID (flit number)
  uint16_t ID{};                        ///< zopEvent: message ID
  uint8_t  MbxID{};                     ///< zopEvent: mailbox ID (message type)
  zopOpc   Opc{ zopOpc::Z_NULL_OPC };   ///< zopEvent: opcode
  uint8_t  AppID{};                     ///< zopEvent: application source
  uint8_t  RingLvl{};                   ///< zopEvent: ring level
  uint64_t Addr{};                      ///< zopEvent: target address

  bool           Read{};              ///< zopEvent: sets this request as a read request
  bool           FenceEncountered{};  ///< zopEvent: whether this ZOP's fence has been seen
  bool           ClearMsgID{};        ///< zopEvent: determines whether to manually trigger msgID clears
  uint64_t*      Target{};            ///< zopEvent: target for the read request
  RevCPU::MemReq req;                 ///< zopEvent: read response handler

public:
  // zopEvent: event serializer
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    // we only serialize the raw packet
    Event::serialize_order( ser );
    ser & Packet;
  }

  // zopEvent: implements the nic serialization
  ImplementSerializable( SST::Forza::zopEvent );

};  // class zopEvent

// --------------------------------------------
// zopAPI
// --------------------------------------------
class zopAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::Forza::zopAPI )

  /// zopAPI: constructor
  zopAPI( ComponentId_t id, Params& params ) : SubComponent( id ) {}

  /// zopAPI: destructor
  virtual ~zopAPI()                                         = default;

  /// zopAPI: registers the the event handler with the core
  virtual void setMsgHandler( Event::HandlerBase* handler ) = 0;

  /// zopAPI: initializes the network
  virtual void init( unsigned int phase )                   = 0;

  /// zopAPI : setup the network
  virtual void setup() {}

  /// zopAPI : send a message on the network
  virtual void send( zopEvent* ev, zopCompID dest )                                    = 0;

  /// zopAPI : send a message on the network to a specific zone+precinct
  virtual void send( zopEvent* ev, zopCompID dest, zopPrecID zone, unsigned precinct ) = 0;

  /// zopAPI : send a zone barrier request
  virtual void send_zone_barrier( unsigned hart, unsigned endpoints )                  = 0;

  /// zopAPI: query the nic to see if the barrier is complete
  virtual bool isBarrierComplete( unsigned Hart )                                      = 0;

  /// zopAPI: query the NIC to see if it already exists in a barrier state
  virtual bool hasBarrier( unsigned Hart )                                             = 0;

  /// zopAPI : retrieve the number of potential endpoints
  virtual unsigned getNumDestinations()                                                = 0;

  /// zopAPI: return the NIC's network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress()                           = 0;

  /// zopAPI: set the type of the endpoint
  virtual void setEndpointType( zopCompID type )                                       = 0;

  /// zopAPI: get the type of the endpoint
  virtual zopCompID getEndpointType()                                                  = 0;

  /// zopAPI: set the number of harts
  virtual void setNumHarts( unsigned Hart )                                            = 0;

  /// zopAPI: set the precinct ID
  virtual void setPrecinctID( unsigned Precinct )                                      = 0;

  /// zopAPI: set the zone ID
  virtual void setZoneID( unsigned Zone )                                              = 0;

  /// zopAPI: get the precinct ID
  virtual unsigned getPrecinctID()                                                     = 0;

  /// zopAPI: get the zone ID
  virtual unsigned getZoneID()                                                         = 0;

  /// zopAPI: get the number of ZAPs
  virtual unsigned getNumZaps()                                                        = 0;

  /// zopAPI: get the number of zones in this precinct
  virtual unsigned getNumZones()                                                       = 0;

  /// zopAPI: get the number of precincts
  virtual unsigned getNumPrecincts()                                                   = 0;

  /// zopAPI: clear the message Id hazard
  virtual void clearMsgID( unsigned Hart, uint8_t Id )                                 = 0;

  /// zopAPI: convert the precinct ID to zopPrecID
  zopPrecID getPCID( unsigned Z ) {
    switch( Z ) {
    case 0: return zopPrecID::Z_ZONE0;
    case 1: return zopPrecID::Z_ZONE1;
    case 2: return zopPrecID::Z_ZONE2;
    case 3: return zopPrecID::Z_ZONE3;
    case 4: return zopPrecID::Z_ZONE4;
    case 5: return zopPrecID::Z_ZONE5;
    case 6: return zopPrecID::Z_ZONE6;
    case 7: return zopPrecID::Z_ZONE7;
    default: return zopPrecID::Z_PMP;
    }
  }

  /// zopAPI: convert the zone ID to zopCompID
  zopCompID getZCID( unsigned Z, bool isRZA ) {
    if( isRZA )
      return zopCompID::Z_RZA;

    switch( Z ) {
    case 0: return zopCompID::Z_ZAP0;
    case 1: return zopCompID::Z_ZAP1;
    case 2: return zopCompID::Z_ZAP2;
    case 3: return zopCompID::Z_ZAP3;
    default: return zopCompID::Z_ZEN;
    }
  }

  /// zopAPI: convert endpoint to string name
  std::string endPToStr( zopCompID T ) {
    switch( T ) {
    case zopCompID::Z_ZAP0: return "ZAP0";
    case zopCompID::Z_ZAP1: return "ZAP1";
    case zopCompID::Z_ZAP2: return "ZAP2";
    case zopCompID::Z_ZAP3: return "ZAP3";
    case zopCompID::Z_RZA: return "RZA";
    case zopCompID::Z_ZEN: return "ZEN";
    case zopCompID::Z_ZQM: return "ZQM";
    case zopCompID::Z_PREC_ZIP: return "PREC_ZIP";
    default: return "UNK";
    }
  }

  /// zopAPI : convert message type to string name
  std::string const msgTToStr( zopMsgT T ) {
    switch( T ) {
    case zopMsgT::Z_MZOP: return "MZOP";
    case zopMsgT::Z_HZOPAC: return "HZOPAC";
    //case zopMsgT::Z_HZOPV: return "HZOPV";
    //case zopMsgT::Z_RZOP: return "RZOP";
    case zopMsgT::Z_MSG: return "MSG";
    case zopMsgT::Z_TMIG: return "TMIG";
    //case zopMsgT::Z_TMGT: return "TMGT";
    //case zopMsgT::Z_SYSC: return "SYSC";
    case zopMsgT::Z_RESP: return "RESP";
    case zopMsgT::Z_EXCP: return "EXCP";
    default: return "UNK";
    }
  }
};

// --------------------------------------------
// zopNIC
// --------------------------------------------
class zopNIC : public zopAPI {
public:
  // register ELI with the SST core
  SST_ELI_REGISTER_SUBCOMPONENT(
    zopNIC, "forza", "zopNIC", SST_ELI_ELEMENT_VERSION( 1, 0, 0 ), "FORZA ZOP NIC", SST::Forza::zopAPI
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "clock", "Clock frequency of the NIC", "1GHz" },
    { "req_per_cycle", "Max requests to dispatch per cycle", "1" },
    { "port", "Port to use, if loaded as an anonymous subcomponent", "network" },
    { "verbose", "Verbosity for output (0 = nothing)", "0" },
    { "enableTestHarness", "Enables the message notification for the ZOPGen test harness", "0" },
    { "numZones", "Number of zones per precinct", "8" },
    { "numPrecincts", "Number of precincts in the system", "1" }
  )

  SST_ELI_DOCUMENT_PORTS( { "network", "Port to network", { "simpleNetworkExample.nicEvent" } } )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS( { "iface", "SimpleNetwork interface to a network", "SST::Interfaces::SimpleNetwork" } )

  SST_ELI_DOCUMENT_STATISTICS(
    { "BytesSent", "Number of bytes sent", "bytes", 1 },
    { "MZOPSent", "Number of MZOPs sent", "count", 1 },
    { "HZOPACSent", "Number of HZOPACs sent", "count", 1 },
    { "HZOPVSent", "Number of HZOPVs sent", "count", 1 },
    { "RZOPSent", "Number of RZOPs sent", "count", 1 },
    { "MSGSent", "Number of MSGs sent", "count", 1 },
    { "TMIGSent", "Number of TMIGs sent", "count", 1 },
    { "TMGTSent", "Number of TMGTs sent", "count", 1 },
    { "SYSCSent", "Number of Syscalls sent", "count", 1 },
    { "RESPSent", "Number of RESPs sent", "count", 1 },
    { "FENCESent", "Number of Fences sent", "count", 1 },
    { "EXCPSent", "Number of Exceptions sent", "count", 1 },
  )

  enum zopStats : uint32_t {
    BytesSent  = 0,
    MZOPSent   = 1,
    HZOPACSent = 2,
    HZOPVSent  = 3,
    RZOPSent   = 4,
    MSGSent    = 5,
    TMIGSent   = 6,
    TMGTSent   = 7,
    SYSCSent   = 8,
    RESPSent   = 9,
    FENCESent  = 10,
    EXCPSent   = 11,
  };

  /// zopNIC: constructor
  zopNIC( ComponentId_t id, Params& params );

  /// zopNIC: destructor
  virtual ~zopNIC();

  /// zopNIC: callback to parent on received messages
  virtual void setMsgHandler( Event::HandlerBase* handler );

  /// zopNIC: initialization function
  virtual void init( unsigned int phase );

  /// zopNIC: setup function
  virtual void setup();

  /// zopNIC: send an event
  virtual void send( zopEvent* ev, zopCompID dest );

  /// zopNIC: send an event
  virtual void send( zopEvent* ev, zopCompID dest, zopPrecID zone, unsigned precinct );

  /// zopNIC: send a zone barrier request
  virtual void send_zone_barrier( unsigned hart, unsigned endpoints );

  /// zopNIC: query the nic to see if the barrier is complete
  virtual bool isBarrierComplete( unsigned Hart );

  /// zopNIC: query the NIC to see if it already exists in a barrier state
  virtual bool hasBarrier( unsigned Hart );

  /// zopNIC: get the number of destinations
  virtual unsigned getNumDestinations();

  /// zopNIC: get the end network address
  virtual SST::Interfaces::SimpleNetwork::nid_t getAddress();

  /// zopNIC: set the endpoint type
  virtual void setEndpointType( zopCompID type ) { Type = type; }

  /// zopNic: get the endpoint type
  virtual zopCompID getEndpointType() { return Type; }

  /// zopNIC: handle load responses
  void handleLoadResponse(
    zopEvent* ev, uint64_t* Target, SST::Forza::zopOpc Opc, SST::RevCPU::MemReq Req, uint8_t ID, uint16_t SrcHart
  );

  /// zopNIC: callback function for the SimpleNetwork interface
  bool msgNotify( int virtualNetwork );

  /// zopNIC: initialize the number of Harts
  virtual void setNumHarts( unsigned Hart );

  /// zopNIC: set the precinct ID
  virtual void setPrecinctID( unsigned P ) { Precinct = P; }

  /// zopNIC: set the zone ID
  virtual void setZoneID( unsigned Z ) { Zone = Z; }

  /// zopNIC: get the precinct ID
  virtual unsigned getPrecinctID() { return Precinct; }

  /// zopNIC: get the zone ID
  virtual unsigned getZoneID() { return Zone; }

  /// zopNIC: get the number of ZAPs
  virtual unsigned getNumZaps();

  /// zopNIC: get the number of zones in this precinct
  virtual unsigned getNumZones() { return numZones; }

  /// zopNIC: get the number of precincts
  virtual unsigned getNumPrecincts() { return numPrecincts; }

  /// zopNIC: clear the message Id hazard
  virtual void clearMsgID( unsigned Hart, uint8_t Id ) { msgId[Hart].clearMsgId( Id ); }

  /// zopNIC: clock tick function
  virtual bool clockTick( Cycle_t cycle );

private:
  /// zopNIC: registers all the statistics with SST
  void registerStats();

  /// zopNIC: adds a statistic value to the desired entry
  void recordStat( zopNIC::zopStats Stat, uint64_t Data );

  /// zopNIC: retrieve the zopStat entry from the target zopEvent
  zopNIC::zopStats getStatFromPacket( zopEvent* ev );

  /// zopNIC: test the fence condition
  bool handleFence( zopEvent* ev );

  /// zopNIC: handle the barrier packet
  bool handleBarrier( zopEvent* ev );

  SST::Output                     output;             ///< zopNIC: SST output object
  SST::Interfaces::SimpleNetwork* iFace;              ///< zopNIC: SST network interface
  SST::Event::HandlerBase*        msgHandler;         ///< zopNIC: SST message handler
  bool                            initBroadcastSent;  ///< zopNIC: has the broadcast msg been sent
  unsigned                        numDest;            ///< zopNIC: number of destination endpoints
  unsigned                        ReqPerCycle;        ///< zopNIC: max requests to send per cycle
  unsigned                        numHarts;           ///< zopNIC: number of attached Harts
  unsigned                        Precinct;           ///< zopNIC: precinct ID
  unsigned                        Zone;               ///< zopNIC: zone ID
  zopCompID                       Type;               ///< zopNIC: endpoint type
  unsigned                        numZones;           ///< zopNIC: number of zones per precinct
  unsigned                        numPrecincts;       ///< zopNIC: number of precincts
  bool                            enableTestHarness;  ///< zopNIC: enable the test harness

  SST::Forza::zopMsgID* msgId;      ///< zopNIC: per hart message ID objects
  unsigned*             HARTFence;  ///< zopNIC: per hart fence counters

  unsigned*              barrierSense;      ///< zopNIC: barrier sense flags for A & B
  std::vector<unsigned*> zoneBarrier;       ///< zopNIC: Zone Barrier counters for A & B
  std::vector<unsigned*> barrierEndpoints;  ///< zopNIC: Number of endpoints to wait for A & B barriers

  std::vector<std::pair<zopEvent*, zopCompID>>          preInitQ;  ///< zopNIC: holds buffered requests before the network boots
  std::vector<SST::Interfaces::SimpleNetwork::Request*> sendQ;     ///< zopNIC: buffered send queue

#define _HM_ENDP_T 0
#define _HM_ZID    1
#define _HM_PID    2
  std::map<SST::Interfaces::SimpleNetwork::nid_t, std::tuple<zopCompID, zopPrecID, unsigned>>
    hostMap;  ///< zopNIC: network ID to endpoint type mapping

#define _ZNIC_OUT_HART   0
#define _ZNIC_OUT_ID     1
#define _ZNIC_OUT_READ   2
#define _ZNIC_OUT_TARGET 3
#define _ZNIC_OUT_OPC    4
#define _ZNIC_OUT_REQ    5
  std::vector<std::tuple<
    uint16_t,
    uint8_t,
    bool,
    uint64_t*,
    SST::Forza::zopOpc,
    SST::RevCPU::MemReq>>
    outstanding;  ///< zopNIC: tracks outstanding requests

  std::vector<Statistic<uint64_t>*> stats;  ///< zopNIC: statistics vector
};  // zopNIC

}  // namespace SST::Forza

#endif  // _SST_ZOPNET_H_

// EOF
