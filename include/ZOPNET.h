//
// _ZOPNet_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

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
#define Z_NUM_HEADER_FLITS 2

#define Z_SHIFT_HARTID     0
#define Z_SHIFT_ZCID       9
#define Z_SHIFT_PCID       13
#define Z_SHIFT_PRECINCT   17
#define Z_SHIFT_OPC        32
#define Z_SHIFT_CREDIT     40
#define Z_SHIFT_MSGID      45
#define Z_SHIFT_FLITLEN    51
#define Z_SHIFT_BLOCK      59
#define Z_SHIFT_TYPE       60
#define Z_SHIFT_APPID      32

#define Z_MASK_HARTID      0b111111111
#define Z_MASK_ZCID        0b1111
#define Z_MASK_PCID        0b1111
#define Z_MASK_PRECINCT    0b1111111111111
#define Z_MASK_OPC         0b11111111
#define Z_MASK_CREDIT      0b11111
#define Z_MASK_MSGID       0b111111
#define Z_MASK_FLITLEN     0b11111111
#define Z_MASK_BLOCK       0b1
#define Z_MASK_TYPE        0b1111
#define Z_MASK_APPID       0xFFFFFFFF

#define Z_FLIT_OPC         0
#define Z_FLIT_CREDIT      0
#define Z_FLIT_MSGID       0
#define Z_FLIT_FLITLEN     0
#define Z_FLIT_BLOCK       0
#define Z_FLIT_TYPE        0
#define Z_FLIT_APPID       1
#define Z_FLIT_DEST        0
#define Z_FLIT_SRC         1

#define Z_FLIT_ACS         2
#define Z_FLIT_ADDR        3
#define Z_FLIT_DATA        4
#define Z_FLIT_DATA_RESP   3
#define Z_FLIT_SENSE       2

#define Z_MZOP_PIPE_HART   0
#define Z_HZOP_PIPE_HART   1
#define Z_RZOP_PIPE_HART   2

#define Z_MZOP_DMA_MAX     2040

#define Z_ACS_WRITE        0xFFFFFFFF00000000ULL
#define Z_ACS_READ         0xFFFFFFFFFFFFFFFFULL

// --------------------------------------------
// zopMsgT : ZOP Type
// --------------------------------------------
enum class zopMsgT : uint8_t {
  Z_MZOP   = 0b0000,  /// FORZA MZOP
  Z_HZOPAC = 0b0001,  /// FORZA HZOP ATOMICS/CUSTOM
  Z_HZOPV  = 0b0010,  /// FORZA HZOP VECTOR
  Z_RZOP   = 0b0011,  /// FORZA RZOP
  Z_MSG    = 0b0100,  /// FORZA MESSAGING
  Z_TMIG   = 0b0101,  /// FORZA THREAD MIGRATION
  Z_TMGT   = 0b0110,  /// FORZA THREAD MANAGEMENT
  Z_SYSC   = 0b0111,  /// FORZA SYSCALL
  Z_RESP   = 0b1000,  /// FORZA RESPONSE
  // -- 0b1001 - 0b1101 UNASSIGNED
  Z_FENCE  = 0b1110,  /// FORZE FENCE
  Z_EXCP   = 0b1111,  /// FORZA EXCEPTION
};

// --------------------------------------------
// zopOpc : ZOP Opcode
// --------------------------------------------
enum class zopOpc : uint8_t {
  // -- MZOPs --
  Z_MZOP_LB           = 0b00000000,  /// zopOpc: MZOP Load unsigned byte
  Z_MZOP_LH           = 0b00000001,  /// zopOpc: MZOP Load unsigned half
  Z_MZOP_LW           = 0b00000010,  /// zopOpc: MZOP Load unsigned word
  Z_MZOP_LD           = 0b00000011,  /// zopOpc: MZOP Load doubleword
  Z_MZOP_LSB          = 0b00000100,  /// zopOpc: MZOP Load signed byte
  Z_MZOP_LSH          = 0b00000101,  /// zopOpc: MZOP Load signed half
  Z_MZOP_LSW          = 0b00000110,  /// zopOpc: MZOP Load signed word
  Z_MZOP_LDMA         = 0b00000111,  /// zopOpc: MZOP Load DMA
  Z_MZOP_SB           = 0b00001000,  /// zopOpc: MZOP Store unsigned byte
  Z_MZOP_SH           = 0b00001001,  /// zopOpc: MZOP Store unsigned half
  Z_MZOP_SW           = 0b00001010,  /// zopOpc: MZOP Store unsigned word
  Z_MZOP_SD           = 0b00001011,  /// zopOpc: MZOP Store doubleword
  Z_MZOP_SSB          = 0b00001100,  /// zopOpc: MZOP Store signed byte
  Z_MZOP_SSH          = 0b00001101,  /// zopOpc: MZOP Store signed half
  Z_MZOP_SSW          = 0b00001110,  /// zopOpc: MZOP Store signed word
  Z_MZOP_SDMA         = 0b00001111,  /// zopOpc: MSOP Store DMA

  Z_MZOP_SCLB         = 0b11100000,  /// zopOpc: MZOP Load scratch unsigned byte
  Z_MZOP_SCLH         = 0b11100001,  /// zopOpc: MZOP Load scratch unsigned half
  Z_MZOP_SCLW         = 0b11100010,  /// zopOpc: MZOP Load scratch unsigned word
  Z_MZOP_SCLD         = 0b11100011,  /// zopOpc: MZOP Load scratch doubleword
  Z_MZOP_SCLSB        = 0b11100100,  /// zopOpc: MZOP Load scratch signed byte
  Z_MZOP_SCLSH        = 0b11100101,  /// zopOpc: MZOP Load scratch signed half
  Z_MZOP_SCLSW        = 0b11100110,  /// zopOpc: MZOP Load scratch signed word
  //Z_MZOP_SCLSD  = 0b11100111,   /// zopOpc: MZOP Load scratch double (dupe)
  Z_MZOP_SCSB         = 0b11101000,  /// zopOpc: MZOP Store scratch unsigned byte
  Z_MZOP_SCSH         = 0b11101001,  /// zopOpc: MZOP Store scratch unsigned half
  Z_MZOP_SCSW         = 0b11101010,  /// zopOpc: MZOP Store scratch unsigned word
  Z_MZOP_SCSD         = 0b11101011,  /// zopOpc: MZOP Store scratch doubleword
  Z_MZOP_SCSSB        = 0b11101100,  /// zopOpc: MZOP Store scratch signed byte
  Z_MZOP_SCSSH        = 0b11101101,  /// zopOpc: MZOP Store scratch signed half
  Z_MZOP_SCSSW        = 0b11101110,  /// zopOpc: MZOP Store scratch signed word
  //Z_MZOP_SCSSD  = 0b11101111,   /// zopOpc: MZOP Store scratch doubleword (dupe)

  // -- HZOP ATOMIC/CUSTOM --
  // -- Encoding Styles: Z_HAC_width_type_OP
  // -- '4' = 32 bit width operation
  // -- '8' = 64 bit width operation
  // -- 'B' = BASE-Type
  // -- 'M' = M-Type
  // -- 'S' = S-Type
  // -- 'MS' = MS-Type
  Z_HAC_32_BASE_ADD   = 0b00000000,  /// zopOpc: HZOP-AC 32bit BASE AMO ADD
  Z_HAC_32_BASE_AND   = 0b00001000,  /// zopOpc: HZOP-AC 32bit BASE AMO AND
  Z_HAC_32_BASE_OR    = 0b00010000,  /// zopOpc: HZOP-AC 32bit BASE AMO OR
  Z_HAC_32_BASE_XOR   = 0b00011000,  /// zopOpc: HZOP-AC 32bit BASE AMO XOR
  Z_HAC_32_BASE_SMAX  = 0b00100000,  /// zopOpc: HZOP-AC 32bit BASE AMO Signed Max
  Z_HAC_32_BASE_MAX   = 0b00101000,  /// zopOpc: HZOP-AC 32bit BASE AMO Max
  Z_HAC_32_BASE_SMIN  = 0b00110000,  /// zopOpc: HZOP-AC 32bit BASE AMO Signed Min
  Z_HAC_32_BASE_MIN   = 0b00111000,  /// zopOpc: HZOP-AC 32bit BASE AMO Min
  Z_HAC_32_BASE_SWAP  = 0b01000000,  /// zopOpc: HZOP-AC 32bit BASE AMO SWAP
  Z_HAC_32_BASE_CAS   = 0b01001000,  /// zopOpc: HZOP-AC 32bit BASE AMO CAS
  Z_HAC_32_BASE_FADD  = 0b01010000,  /// zopOpc: HZOP-AC 32bit BASE AMO FADD
  Z_HAC_32_BASE_FSUB  = 0b01100000,  /// zopOpc: HZOP-AC 32bit BASE AMO FSUB
  Z_HAC_32_BASE_FRSUB = 0b01101000,  /// zopOpc: HZOP-AC 32bit BASE AMO FRSUB

  Z_HAC_64_BASE_ADD   = 0b00000001,  /// zopOpc: HZOP-AC 64bit BASE AMO ADD
  Z_HAC_64_BASE_AND   = 0b00001001,  /// zopOpc: HZOP-AC 64bit BASE AMO AND
  Z_HAC_64_BASE_OR    = 0b00010001,  /// zopOpc: HZOP-AC 64bit BASE AMO OR
  Z_HAC_64_BASE_XOR   = 0b00011001,  /// zopOpc: HZOP-AC 64bit BASE AMO XOR
  Z_HAC_64_BASE_SMAX  = 0b00100001,  /// zopOpc: HZOP-AC 64bit BASE AMO Signed Max
  Z_HAC_64_BASE_MAX   = 0b00101001,  /// zopOpc: HZOP-AC 64bit BASE AMO Max
  Z_HAC_64_BASE_SMIN  = 0b00110001,  /// zopOpc: HZOP-AC 64bit BASE AMO Signed Min
  Z_HAC_64_BASE_MIN   = 0b00111001,  /// zopOpc: HZOP-AC 64bit BASE AMO Min
  Z_HAC_64_BASE_SWAP  = 0b01000001,  /// zopOpc: HZOP-AC 64bit BASE AMO SWAP
  Z_HAC_64_BASE_CAS   = 0b01001001,  /// zopOpc: HZOP-AC 64bit BASE AMO CAS
  Z_HAC_64_BASE_FADD  = 0b01010001,  /// zopOpc: HZOP-AC 64bit BASE AMO FADD
  Z_HAC_64_BASE_FSUB  = 0b01100001,  /// zopOpc: HZOP-AC 64bit BASE AMO FSUB
  Z_HAC_64_BASE_FRSUB = 0b01101001,  /// zopOpc: HZOP-AC 64bit BASE AMO FRSUB

  Z_HAC_32_M_ADD      = 0b00000010,  /// zopOpc: HZOP-AC 32bit M AMO ADD
  Z_HAC_32_M_AND      = 0b00001010,  /// zopOpc: HZOP-AC 32bit M AMO AND
  Z_HAC_32_M_OR       = 0b00010010,  /// zopOpc: HZOP-AC 32bit M AMO OR
  Z_HAC_32_M_XOR      = 0b00011010,  /// zopOpc: HZOP-AC 32bit M AMO XOR
  Z_HAC_32_M_SMAX     = 0b00100010,  /// zopOpc: HZOP-AC 32bit M AMO Signed Max
  Z_HAC_32_M_MAX      = 0b00101010,  /// zopOpc: HZOP-AC 32bit M AMO Max
  Z_HAC_32_M_SMIN     = 0b00110010,  /// zopOpc: HZOP-AC 32bit M AMO Signed Min
  Z_HAC_32_M_MIN      = 0b00111010,  /// zopOpc: HZOP-AC 32bit M AMO Min
  Z_HAC_32_M_SWAP     = 0b01000010,  /// zopOpc: HZOP-AC 32bit M AMO SWAP
  Z_HAC_32_M_CAS      = 0b01001010,  /// zopOpc: HZOP-AC 32bit M AMO CAS
  Z_HAC_32_M_FADD     = 0b01010010,  /// zopOpc: HZOP-AC 32bit M AMO FADD
  Z_HAC_32_M_FSUB     = 0b01100010,  /// zopOpc: HZOP-AC 32bit M AMO FSUB
  Z_HAC_32_M_FRSUB    = 0b01101010,  /// zopOpc: HZOP-AC 32bit M AMO FRSUB

  Z_HAC_64_M_ADD      = 0b00000011,  /// zopOpc: HZOP-AC 64bit M AMO ADD
  Z_HAC_64_M_AND      = 0b00001011,  /// zopOpc: HZOP-AC 64bit M AMO AND
  Z_HAC_64_M_OR       = 0b00010011,  /// zopOpc: HZOP-AC 64bit M AMO OR
  Z_HAC_64_M_XOR      = 0b00011011,  /// zopOpc: HZOP-AC 64bit M AMO XOR
  Z_HAC_64_M_SMAX     = 0b00100011,  /// zopOpc: HZOP-AC 64bit M AMO Signed Max
  Z_HAC_64_M_MAX      = 0b00101011,  /// zopOpc: HZOP-AC 64bit M AMO Max
  Z_HAC_64_M_SMIN     = 0b00110011,  /// zopOpc: HZOP-AC 64bit M AMO Signed Min
  Z_HAC_64_M_MIN      = 0b00111011,  /// zopOpc: HZOP-AC 64bit M AMO Min
  Z_HAC_64_M_SWAP     = 0b01000011,  /// zopOpc: HZOP-AC 64bit M AMO SWAP
  Z_HAC_64_M_CAS      = 0b01001011,  /// zopOpc: HZOP-AC 64bit M AMO CAS
  Z_HAC_64_M_FADD     = 0b01010011,  /// zopOpc: HZOP-AC 64bit M AMO FADD
  Z_HAC_64_M_FSUB     = 0b01100011,  /// zopOpc: HZOP-AC 64bit M AMO FSUB
  Z_HAC_64_M_FRSUB    = 0b01101011,  /// zopOpc: HZOP-AC 64bit M AMO FRSUB

  Z_HAC_32_S_ADD      = 0b00000100,  /// zopOpc: HZOP-AC 32bit S AMO ADD
  Z_HAC_32_S_AND      = 0b00001100,  /// zopOpc: HZOP-AC 32bit S AMO AND
  Z_HAC_32_S_OR       = 0b00010100,  /// zopOpc: HZOP-AC 32bit S AMO OR
  Z_HAC_32_S_XOR      = 0b00011100,  /// zopOpc: HZOP-AC 32bit S AMO XOR
  Z_HAC_32_S_SMAX     = 0b00100100,  /// zopOpc: HZOP-AC 32bit S AMO Signed Max
  Z_HAC_32_S_MAX      = 0b00101100,  /// zopOpc: HZOP-AC 32bit S AMO Max
  Z_HAC_32_S_SMIN     = 0b00110100,  /// zopOpc: HZOP-AC 32bit S AMO Signed Min
  Z_HAC_32_S_MIN      = 0b00111100,  /// zopOpc: HZOP-AC 32bit S AMO Min
  Z_HAC_32_S_SWAP     = 0b01000100,  /// zopOpc: HZOP-AC 32bit S AMO SWAP
  Z_HAC_32_S_CAS      = 0b01001100,  /// zopOpc: HZOP-AC 32bit S AMO CAS
  Z_HAC_32_S_FADD     = 0b01010100,  /// zopOpc: HZOP-AC 32bit S AMO FADD
  Z_HAC_32_S_FSUB     = 0b01100100,  /// zopOpc: HZOP-AC 32bit S AMO FSUB
  Z_HAC_32_S_FRSUB    = 0b01101100,  /// zopOpc: HZOP-AC 32bit S AMO FRSUB

  Z_HAC_64_S_ADD      = 0b00000101,  /// zopOpc: HZOP-AC 64bit S AMO ADD
  Z_HAC_64_S_AND      = 0b00001101,  /// zopOpc: HZOP-AC 64bit S AMO AND
  Z_HAC_64_S_OR       = 0b00010101,  /// zopOpc: HZOP-AC 64bit S AMO OR
  Z_HAC_64_S_XOR      = 0b00011101,  /// zopOpc: HZOP-AC 64bit S AMO XOR
  Z_HAC_64_S_SMAX     = 0b00100101,  /// zopOpc: HZOP-AC 64bit S AMO Signed Max
  Z_HAC_64_S_MAX      = 0b00101101,  /// zopOpc: HZOP-AC 64bit S AMO Max
  Z_HAC_64_S_SMIN     = 0b00110101,  /// zopOpc: HZOP-AC 64bit S AMO Signed Min
  Z_HAC_64_S_MIN      = 0b00111101,  /// zopOpc: HZOP-AC 64bit S AMO Min
  Z_HAC_64_S_SWAP     = 0b01000101,  /// zopOpc: HZOP-AC 64bit S AMO SWAP
  Z_HAC_64_S_CAS      = 0b01001101,  /// zopOpc: HZOP-AC 64bit S AMO CAS
  Z_HAC_64_S_FADD     = 0b01010101,  /// zopOpc: HZOP-AC 64bit S AMO FADD
  Z_HAC_64_S_FSUB     = 0b01100101,  /// zopOpc: HZOP-AC 64bit S AMO FSUB
  Z_HAC_64_S_FRSUB    = 0b01101101,  /// zopOpc: HZOP-AC 64bit S AMO FRSUB

  Z_HAC_32_MS_ADD     = 0b00000110,  /// zopOpc: HZOP-AC 32bit MS AMO ADD
  Z_HAC_32_MS_AND     = 0b00001110,  /// zopOpc: HZOP-AC 32bit MS AMO AND
  Z_HAC_32_MS_OR      = 0b00010110,  /// zopOpc: HZOP-AC 32bit MS AMO OR
  Z_HAC_32_MS_XOR     = 0b00011110,  /// zopOpc: HZOP-AC 32bit MS AMO XOR
  Z_HAC_32_MS_SMAX    = 0b00100110,  /// zopOpc: HZOP-AC 32bit MS AMO Signed Max
  Z_HAC_32_MS_MAX     = 0b00101110,  /// zopOpc: HZOP-AC 32bit MS AMO Max
  Z_HAC_32_MS_SMIN    = 0b00110110,  /// zopOpc: HZOP-AC 32bit MS AMO Signed Min
  Z_HAC_32_MS_MIN     = 0b00111110,  /// zopOpc: HZOP-AC 32bit MS AMO Min
  Z_HAC_32_MS_SWAP    = 0b01000110,  /// zopOpc: HZOP-AC 32bit MS AMO SWAP
  Z_HAC_32_MS_CAS     = 0b01001110,  /// zopOpc: HZOP-AC 32bit MS AMO CAS
  Z_HAC_32_MS_FADD    = 0b01010110,  /// zopOpc: HZOP-AC 32bit MS AMO FADD
  Z_HAC_32_MS_FSUB    = 0b01100110,  /// zopOpc: HZOP-AC 32bit MS AMO FSUB
  Z_HAC_32_MS_FRSUB   = 0b01101110,  /// zopOpc: HZOP-AC 32bit MS AMO FRSUB

  Z_HAC_64_MS_ADD     = 0b00000111,  /// zopOpc: HZOP-AC 64bit MS AMO ADD
  Z_HAC_64_MS_AND     = 0b00001111,  /// zopOpc: HZOP-AC 64bit MS AMO AND
  Z_HAC_64_MS_OR      = 0b00010111,  /// zopOpc: HZOP-AC 64bit MS AMO OR
  Z_HAC_64_MS_XOR     = 0b00011111,  /// zopOpc: HZOP-AC 64bit MS AMO XOR
  Z_HAC_64_MS_SMAX    = 0b00100111,  /// zopOpc: HZOP-AC 64bit MS AMO Signed Max
  Z_HAC_64_MS_MAX     = 0b00101111,  /// zopOpc: HZOP-AC 64bit MS AMO Max
  Z_HAC_64_MS_SMIN    = 0b00110111,  /// zopOpc: HZOP-AC 64bit MS AMO Signed Min
  Z_HAC_64_MS_MIN     = 0b00111111,  /// zopOpc: HZOP-AC 64bit MS AMO Min
  Z_HAC_64_MS_SWAP    = 0b01000111,  /// zopOpc: HZOP-AC 64bit MS AMO SWAP
  Z_HAC_64_MS_CAS     = 0b01001111,  /// zopOpc: HZOP-AC 64bit MS AMO CAS
  Z_HAC_64_MS_FADD    = 0b01010111,  /// zopOpc: HZOP-AC 64bit MS AMO FADD
  Z_HAC_64_MS_FSUB    = 0b01100111,  /// zopOpc: HZOP-AC 64bit MS AMO FSUB
  Z_HAC_64_MS_FRSUB   = 0b01101111,  /// zopOpc: HZOP-AC 64bit MS AMO FRSUB

  // -- MESSAGING --
  Z_MSG_SENDP         = 0b00000000,  /// zopOpc: MESSAGING Send with payload
  Z_MSG_SENDAS        = 0b00000001,  /// zopOpc: MESSAGING Send with address and size
  Z_MSG_CREDIT        = 0b11110000,  /// zopOpc: MESSAGING Credit replenishment
  Z_MSG_ZENSET        = 0b11110001,  /// zopOpc: MESSAGING ZEN Setup
  Z_MSG_ZQMSET        = 0b11110100,  /// zopOpc: MESSAGING ZQM Setup
  Z_MSG_ZQMHARTDONE   = 0b11110101,  /// zopOpc: MESSAGING ZQM Notify HART completion
  Z_MSG_ACK           = 0b11110010,  /// zopOpc: MESSAGING Send Ack
  Z_MSG_EXCP          = 0b11110011,  /// zopOpc: MESSAGING Send exception
  Z_MSG_ZBAR          = 0b11111001,  /// zopOpc: MESSAGING Zone Barrier Request

  // -- THREAD MIGRATION --
  Z_TMIG_SELECT       = 0b00000000,  /// zopOpc: THREAD MIGRATION ZQM choose HART
  Z_TMIG_FIXED        = 0b00000001,  /// zopOpc: THREAD MIGRATION Fixed HART choice

  // -- RZA RESPONSE --
  Z_RESP_LR           = 0b00000000,  /// zopOpc: RZA RESPONSE Load response
  Z_RESP_LEXCP        = 0b00000001,  /// zopOpc: RZA RESPONSE Load exception
  Z_RESP_SACK         = 0b00000010,  /// zopOpc: RZA RESPONSE Store ack
  Z_RESP_SEXCP        = 0b00000011,  /// zopOpc: RZA RESPONSE Store exception
  Z_RESP_HRESP        = 0b00000100,  /// zopOpc: RZA RESPONSE HZOP response
  Z_RESP_HEXCP        = 0b00000101,  /// zopOpc: RZA RESPONSE HZOP exception
  Z_RESP_RRESP        = 0b00000110,  /// zopOpc: RZA RESPONSE RZOP response
  Z_RESP_REXCP        = 0b00000111,  /// zopOpc: RZA RESPONSE RZOP exception

  // -- FENCE --
  Z_FENCE_HART        = 0b00000000,  /// zopOpc: HART Fence (only fences the calling HART)
  Z_FENCE_ZAP         = 0b00000001,  /// zopOpc: ZAP Fence (fences all HARTs on a ZAP)
  Z_FENCE_RZA         = 0b00000010,  /// zopOpc: RZA Fence (fences all requests on an RZA)

  // -- EXCEPTION --
  Z_EXCP_NONE         = 0b00000000,  /// zopOpc: Exception; no exception
  Z_EXCP_INVENDP      = 0b00000001,  /// zopOpc: Exception; Invalid endpoint
  Z_EXCP_INVO         = 0b00000010,  /// zopOpc: Exception; Invalid operation
  Z_EXCP_FINVO        = 0b10000000,  /// zopOpc: Exception; Invalid floating point operation
  Z_EXCP_FDIVZ        = 0b10000001,  /// zopOpc: Exception; Floating point division by zero
  Z_EXCP_FOVR         = 0b10000010,  /// zopOpc: Exception; Floating point overlow
  Z_EXCP_FUND         = 0b10000011,  /// zopOpc: Exception; Floating point underflow
  Z_EXCP_FINXT        = 0b10000100,  /// zopOpc: Exception; Floating point inexact

  Z_NULL_OPC          = 0b11111111,  /// zopOpc: null opcode

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
  Z_ZAP4     = 0b00000100,  /// zopCompID: ZAP4
  Z_ZAP5     = 0b00000101,  /// zopCompID: ZAP5
  Z_ZAP6     = 0b00000110,  /// zopCompID: ZAP6
  Z_ZAP7     = 0b00000111,  /// zopCompID: ZAP7
  Z_RZA      = 0b00001000,  /// zopCompID: RZA
  Z_ZQM      = 0b00001010,  /// zopCompID: ZQM
  Z_ZEN      = 0b00001100,  /// zopCompID: ZEN
  Z_PREC_ZIP = 0b00001111,  /// zopCompID: PRECINCT ZIP
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
  Z_ZIP   = 0b00001001,  /// zopPrecID: ZIP
};

// --------------------------------------------
// zopMsgID
// --------------------------------------------
class zopMsgID {
public:
  // zopMsgID: constructor
  zopMsgID() : NumFree( 64 ) {
    for( uint8_t i = 0; i < 64; i++ ) {
      Mask[i] = false;
    }
  }

  // zopMsgID: destructor
  ~zopMsgID() = default;

  // zopMsgID: get the number of free message slots
  unsigned getNumFree() { return NumFree; }

  /// zopMsgID: clear the message id
  void clearMsgId( uint8_t I ) {
    Mask[I] = false;
    NumFree++;
  }

  // zopMsgID: retrieve a new message id
  uint8_t getMsgId() {
    for( uint8_t i = 0; i < 64; i++ ) {
      if( Mask[i] == false ) {
        NumFree--;
        Mask[i] = true;
        return i;
      }
    }
    return 64;  // this is an erroneous id
  }

private:
  unsigned NumFree;
  bool     Mask[64];
};

// --------------------------------------------
// zopEvent
// --------------------------------------------
class zopEvent : public SST::Event {
public:
  // Note: all constructors create two full FLITs
  // This is comprised of 2 x 64bit words

  /// zopEvent: init broadcast constructor
  //  This constructor is ONLY utilized for initialization
  //  DO NOT use this constructor for normal packet construction
  explicit zopEvent( unsigned srcId, zopCompID Type, unsigned ZoneID, unsigned PrecinctID )
    : Event(), Read( false ), FenceEncountered( false ), ClearMsgID( false ), Target( nullptr ) {
    Packet.push_back( (uint64_t) ( Type ) );
    Packet.push_back( (uint64_t) ( srcId ) );
    Packet.push_back( (uint64_t) ( ZoneID ) );
    Packet.push_back( (uint64_t) ( PrecinctID ) );
  }

  /// zopEvent: raw event constructor
  explicit zopEvent() : Event(), Read( false ), FenceEncountered( false ), ClearMsgID( false ), Target( nullptr ) {
    Packet.push_back( 0x00ull );
    Packet.push_back( 0x00ull );
  }

  explicit zopEvent( zopMsgT T, zopOpc O )
    : Event(), Read( false ), FenceEncountered( false ), ClearMsgID( false ), Target( nullptr ) {
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
  void setPacket( const std::vector<uint64_t> P ) {
    Packet.clear();
    for( auto i : P ) {
      Packet.push_back( i );
    }
  }

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

  /// zopEvent: set the NB flag
  void setNB( uint8_t N ) { NB = N; }

  /// zopEvent: set the packet ID
  void setID( uint8_t I ) { ID = I; }

  /// zopEvent: set the credit
  void setCredit( uint8_t C ) { Credit = C; }

  /// zopEvent: set the opcode
  void setOpc( zopOpc O ) { Opc = O; }

  /// zopEvent: set the application id
  void setAppID( uint32_t A ) { AppID = A; }

  /// zopEvent: set the destination hart
  void setDestHart( uint16_t H ) { DestHart = H; }

  /// zopEvent: set the destination ZCID
  void setDestZCID( uint8_t Z ) { DestZCID = Z; }

  void setDestZCID( zopCompID Z ) { DestZCID = static_cast<uint8_t>( Z ); }

  /// zopEvent: set the destination PCID
  void setDestPCID( uint8_t P ) { DestPCID = P; }

  /// zopEvent: set the destination precinct
  void setDestPrec( uint16_t P ) { DestPrec = P; }

  /// zopEvent: set the src hart
  void setSrcHart( uint16_t H ) { SrcHart = H; }

  /// zopEvent: set the src ZCID
  void setSrcZCID( uint8_t Z ) { SrcZCID = Z; }

  void setSrcZCID( zopCompID Z ) { SrcZCID = static_cast<uint8_t>( Z ); }

  /// zopEvent: set the src PCID
  void setSrcPCID( uint8_t P ) { SrcPCID = P; }

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

  /// zopEvent: get the NB flag
  uint8_t getNB() { return NB; }

  /// zopEvent: get the packet length
  uint8_t getLength() { return Length; }

  /// zopEvent: get the packet ID
  uint8_t getID() { return ID; }

  /// zopEvent: get the credit
  uint8_t getCredit() { return Credit; }

  /// zopEvent: get the opcode
  zopOpc getOpc() { return Opc; }

  /// zopEvent: get the application id
  uint32_t getAppID() { return AppID; }

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
    Credit   = (uint8_t) ( ( Packet[Z_FLIT_CREDIT] >> Z_SHIFT_CREDIT ) & Z_MASK_CREDIT );
    ID       = (uint8_t) ( ( Packet[Z_FLIT_MSGID] >> Z_SHIFT_MSGID ) & Z_MASK_MSGID );
    Length   = (uint8_t) ( ( Packet[Z_FLIT_FLITLEN] >> Z_SHIFT_FLITLEN ) & Z_MASK_FLITLEN );
    NB       = (uint8_t) ( ( Packet[Z_FLIT_BLOCK] >> Z_SHIFT_BLOCK ) & Z_MASK_BLOCK );
    Type     = (zopMsgT) ( ( Packet[Z_FLIT_TYPE] >> Z_SHIFT_TYPE ) & Z_MASK_TYPE );

    SrcHart  = (uint16_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_HARTID ) & Z_MASK_HARTID );
    SrcZCID  = (uint8_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_ZCID ) & Z_MASK_ZCID );
    SrcPCID  = (uint8_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_PCID ) & Z_MASK_PCID );
    SrcPrec  = (uint16_t) ( ( Packet[Z_FLIT_SRC] >> Z_SHIFT_PRECINCT ) & Z_MASK_PRECINCT );

    AppID    = (uint32_t) ( ( Packet[Z_FLIT_APPID] >> Z_SHIFT_APPID ) & Z_MASK_APPID );
  }

  /// zopEvent: encode this event and set the appropriate internal packet structures
  void encodeEvent() {
    Length = Packet.size() - Z_NUM_HEADER_FLITS;
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestHart & Z_MASK_HARTID ) << Z_SHIFT_HARTID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestZCID & Z_MASK_ZCID ) << Z_SHIFT_ZCID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestPCID & Z_MASK_PCID ) << Z_SHIFT_PCID );
    Packet[Z_FLIT_DEST] |= ( (uint64_t) ( DestPrec & Z_MASK_PRECINCT ) << Z_SHIFT_PRECINCT );

    Packet[Z_FLIT_OPC] |= ( (uint64_t) ( (uint64_t) (Opc) &Z_MASK_OPC ) << Z_SHIFT_OPC );
    Packet[Z_FLIT_CREDIT] |= ( (uint64_t) ( Credit & Z_MASK_CREDIT ) << Z_SHIFT_CREDIT );
    Packet[Z_FLIT_MSGID] |= ( (uint64_t) ( ID & Z_MASK_MSGID ) << Z_SHIFT_MSGID );
    Packet[Z_FLIT_FLITLEN] |= ( (uint64_t) ( Length & Z_MASK_FLITLEN ) << Z_SHIFT_FLITLEN );
    Packet[Z_FLIT_BLOCK] |= ( (uint64_t) ( NB & Z_MASK_BLOCK ) << Z_SHIFT_BLOCK );
    Packet[Z_FLIT_TYPE] |= ( (uint64_t) ( (uint64_t) (Type) &Z_MASK_TYPE ) << Z_SHIFT_TYPE );

    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcHart & Z_MASK_HARTID ) << Z_SHIFT_HARTID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcZCID & Z_MASK_ZCID ) << Z_SHIFT_ZCID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcPCID & Z_MASK_PCID ) << Z_SHIFT_PCID );
    Packet[Z_FLIT_SRC] |= ( (uint64_t) ( SrcPrec & Z_MASK_PRECINCT ) << Z_SHIFT_PRECINCT );

    Packet[Z_FLIT_APPID] |= ( (uint64_t) ( AppID & Z_MASK_APPID ) << Z_SHIFT_APPID );
  }

private:
  std::vector<uint64_t> Packet;  ///< zopEvent: data payload: serialized payload

  // -- private, non-serialized data members
  uint16_t DestHart;  ///< zopEvent: destination hart id
  uint8_t  DestZCID;  ///< zopEvent: destination ZCID
  uint8_t  DestPCID;  ///< zopEvent: destination PCID
  uint16_t DestPrec;  ///< zopEvent: destination Precinct
  uint16_t SrcHart;   ///< zopEvent: src hart id
  uint8_t  SrcZCID;   ///< zopEvent: src ZCID
  uint8_t  SrcPCID;   ///< zopEvent: src PCID
  uint16_t SrcPrec;   ///< zopEvent: src Precinct

  zopMsgT  Type;    ///< zopEvent: message type
  uint8_t  NB;      ///< zopEvent: blocking/non-blocking
  uint8_t  Length;  ///< zopEvent: packet length (in flits)
  uint8_t  ID;      ///< zopEvent: message ID
  uint8_t  Credit;  ///< zopEvent: credit piggyback
  zopOpc   Opc;     ///< zopEvent: opcode
  uint32_t AppID;   ///< zopEvent: application source

  bool                Read;              ///< zopEvent: sets this request as a read request
  bool                FenceEncountered;  ///< zopEvent: whether this ZOP's fence has been seen
  bool                ClearMsgID;        ///< zopEvent: determines whether to manually trigger msgID clears
  uint64_t*           Target;            ///< zopEvent: target for the read request
  SST::RevCPU::MemReq req;               ///< zopEvent: read response handler

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
  SST::Forza::zopPrecID getPCID( unsigned Z ) {
    switch( Z ) {
    case 0: return SST::Forza::zopPrecID::Z_ZONE0; break;
    case 1: return SST::Forza::zopPrecID::Z_ZONE1; break;
    case 2: return SST::Forza::zopPrecID::Z_ZONE2; break;
    case 3: return SST::Forza::zopPrecID::Z_ZONE3; break;
    case 4: return SST::Forza::zopPrecID::Z_ZONE4; break;
    case 5: return SST::Forza::zopPrecID::Z_ZONE5; break;
    case 6: return SST::Forza::zopPrecID::Z_ZONE6; break;
    case 7: return SST::Forza::zopPrecID::Z_ZONE7; break;
    default: return SST::Forza::zopPrecID::Z_PMP; break;
    }
    return SST::Forza::zopPrecID::Z_ZIP;
  }

  /// zopAPI: convert the zone ID to zopCompID
  SST::Forza::zopCompID getZCID( unsigned Z, bool isRZA ) {
    if( isRZA )
      return SST::Forza::zopCompID::Z_RZA;

    switch( Z ) {
    case 0: return SST::Forza::zopCompID::Z_ZAP0; break;
    case 1: return SST::Forza::zopCompID::Z_ZAP1; break;
    case 2: return SST::Forza::zopCompID::Z_ZAP2; break;
    case 3: return SST::Forza::zopCompID::Z_ZAP3; break;
    case 4: return SST::Forza::zopCompID::Z_ZAP4; break;
    case 5: return SST::Forza::zopCompID::Z_ZAP5; break;
    case 6: return SST::Forza::zopCompID::Z_ZAP6; break;
    case 7: return SST::Forza::zopCompID::Z_ZAP7; break;
    default: return SST::Forza::zopCompID::Z_ZEN; break;
    }

    return SST::Forza::zopCompID::Z_ZEN;
  }

  /// zopAPI: convert endpoint to string name
  std::string const endPToStr( zopCompID T ) {
    switch( T ) {
    case zopCompID::Z_ZAP0: return "ZAP0"; break;
    case zopCompID::Z_ZAP1: return "ZAP1"; break;
    case zopCompID::Z_ZAP2: return "ZAP2"; break;
    case zopCompID::Z_ZAP3: return "ZAP3"; break;
    case zopCompID::Z_ZAP4: return "ZAP4"; break;
    case zopCompID::Z_ZAP5: return "ZAP5"; break;
    case zopCompID::Z_ZAP6: return "ZAP6"; break;
    case zopCompID::Z_ZAP7: return "ZAP7"; break;
    case zopCompID::Z_RZA: return "RZA"; break;
    case zopCompID::Z_ZEN: return "ZEN"; break;
    case zopCompID::Z_ZQM: return "ZQM"; break;
    case zopCompID::Z_PREC_ZIP: return "PREC_ZIP"; break;
    default: return "UNK"; break;
    }
  }

  /// zopAPI : convert message type to string name
  std::string const msgTToStr( zopMsgT T ) {
    switch( T ) {
    case zopMsgT::Z_MZOP: return "MZOP"; break;
    case zopMsgT::Z_HZOPAC: return "HZOPAC"; break;
    case zopMsgT::Z_HZOPV: return "HZOPV"; break;
    case zopMsgT::Z_RZOP: return "RZOP"; break;
    case zopMsgT::Z_MSG: return "MSG"; break;
    case zopMsgT::Z_TMIG: return "TMIG"; break;
    case zopMsgT::Z_TMGT: return "TMGT"; break;
    case zopMsgT::Z_SYSC: return "SYSC"; break;
    case zopMsgT::Z_RESP: return "RESP"; break;
    case zopMsgT::Z_EXCP: return "EXCP"; break;
    default: return "UNK"; break;
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
