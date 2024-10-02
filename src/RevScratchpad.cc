//
// _RevScratchpad_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "../include/RevScratchpad.h"
#include <bitset>
#include <cstring>

namespace SST::RevCPU {

bool RevScratchpad::ReadMem(
  unsigned      Hart,
  uint64_t      Addr,
  size_t        Len,
  void*         Target,
  const MemReq& req
) {  //, Interfaces::StandardMem::Request::flags_t flags){
  // Figure out what chunk we're in
  size_t BaseChunkNum = ( Addr - BaseAddr ) / ChunkSize;
  size_t TopChunkNum  = ( Addr + Len - BaseAddr ) / ChunkSize;

  output->verbose(
    CALL_INFO,
    8,
    0,
    "Scratchpad Read: Addr = 0x%" PRIx64 ", Len = %zu, BaseChunkNum = %zu, TopChunkNum = %zu\n",
    Addr,
    Len,
    BaseChunkNum,
    TopChunkNum
  );

  // Figure out the bounds of this read request... Need to make sure that all chunks involved in the read are allocated
  // FIXME: Also test the chunks in between
  if( FreeList.test( BaseChunkNum ) || FreeList.test( TopChunkNum ) ) {
    output->fatal(
      CALL_INFO,
      11,
      "Error: Hart %" PRIu32 " is attempting to read from unallocated memory"
      " in the scratchpad (Addr = 0x %" PRIx64 ", Len = %zu)\n",
      Hart,
      Addr,
      Len
    );
  }

  // Check if the read will go beyond end of scratchpad space
  if( Addr + Len > TopAddr ) {
    output->fatal(
      CALL_INFO,
      11,
      "Error: Hart %" PRIu32 " is attempting to read beyond the highest address "
      "in the scratchpad (Addr = 0x %" PRIx64 ", Size = %zu)\n"
      "While this scratchpad starts at 0x%" PRIx64 " and ends at 0x%" PRIx64 "\n",
      Hart,
      Addr,
      Len,
      BaseAddr,
      TopAddr
    );
  }

  // Perform the read
  if( !VerScratch ) {
    std::memcpy( Target, &BackingMem[Addr - BaseAddr], Len );
    // clear the hazard
    if( req.MarkLoadCompleteFunc != nullptr ) {
      req.MarkLoadComplete();
    }
    return true;
  } else {
    return VerScratch->ReadMem( Hart, Addr, Len, Target, req );  //TODO: VerScratch->ReadMem might not need Hart
    // VerScratch will need to mark the req as complete when the operation is done, so it has to catch it
  }
  // TODO: Add scratchpad stats --- memStats.bytesRead += Len;
}

bool RevScratchpad::WriteMem(
  unsigned    Hart,
  uint64_t    Addr,
  size_t      Len,
  const void* Data
) {  // Interfaces::StandardMem::Request::flags_t flags){
  // Figure out what chunk(s) we're writing to
  size_t BaseChunkNum = ( Addr - BaseAddr ) / ChunkSize;
  size_t TopChunkNum  = ( Addr + Len - BaseAddr ) / ChunkSize;

  output->verbose(
    CALL_INFO,
    8,
    0,
    "Scratchpad Write: Addr = 0x%" PRIx64 ", Len = %zu, BaseChunkNum = %zu, TopChunkNum = %zu\n",
    Addr,
    Len,
    BaseChunkNum,
    TopChunkNum
  );

  // Figure out the bounds of this read request... Need to make sure that all chunks involved in the read are allocated
  for( size_t i = BaseChunkNum; i <= TopChunkNum; i++ ) {
    if( FreeList.test( i ) ) {
      output->fatal(
        CALL_INFO,
        11,
        "Error: Hart %" PRIu32 " is attempting to write to unallocated memory"
        " in the scratchpad (Addr = 0x %" PRIx64 ", Len = %zu)\n",
        Hart,
        Addr,
        Len
      );
    }
  }

  // write the memory to the BackingStore or Verilator-backed scratchpad
  if( !VerScratch ) {
    std::memcpy( &BackingMem[Addr - BaseAddr], Data, Len );
  } else {
    return VerScratch->WriteMem( Hart, Addr, Len, Data );  //TODO: VerScratch->WriteMem probably won't need Hart
  }

  // TODO: Add scratchpad stats --- memStats.bytesWritten += Len;
  return true;
}
}  // namespace SST::RevCPU
