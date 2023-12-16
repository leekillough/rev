//
// _RevScratchpad_h_
//
// Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVSCRATCHPAD_H_
#define _SST_REVCPU_REVSCRATCHPAD_H_


#include "RevMemSegment.h"
#include "SST.h"
#include "../common/include/RevCommon.h"
#include <bitset>

// FORZA: SCRATCHPAD
#define _CHUNK_SIZE_ 512
#define _SCRATCHPAD_SIZE_ (_CHUNK_SIZE_*1024)
#define _SCRATCHPAD_BASE_ 0x0300000000000000

namespace SST::RevCPU{

class RevScratchpad : public RevCustomMemSegment {
public:
  RevScratchpad(RevCPU* CPU, uint64_t baseAddr, size_t Size, SST::Output *Output)
  : RevCustomMemSegment(CPU, baseAddr, Size, "scratchpad", Output),
    BaseAddr(_SCRATCHPAD_BASE_ + ZapNum * Size), Size(Size), ChunkSize(ChunkSize), TopAddr(BaseAddr + Size - 1), ZapNum(ZapNum) {
    FreeList.set();
  }

  // Implement the pure virtual functions from CustomMemSegment
  virtual void ReadMem(unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req, RevFlag flags) override {
      // Figure out what chunk we're in
  size_t BaseChunkNum = (Addr - BaseAddr) / ChunkSize;
  size_t TopChunkNum = (Addr + Len - BaseAddr) / ChunkSize;

  output->verbose(CALL_INFO, 8, 0, "Scratchpad Read: Addr = 0x%" PRIx64 ", Len = %zu, BaseChunkNum = %zu, TopChunkNum = %zu\n", Addr, Len, BaseChunkNum, TopChunkNum);

  // Figure out the bounds of this read request... Need to make sure that all chunks involved in the read are allocated
  // FIXME: Also test the chunks in between
  if( FreeList.test(BaseChunkNum) || FreeList.test(TopChunkNum) ){
    output->fatal(CALL_INFO, 11, "Error: Hart %" PRIu32 " is attempting to read from unallocated memory"
                                 " in the scratchpad (Addr = 0x %" PRIx64 ", Len = %zu)\n", Hart, Addr, Len);
  }

  // Check if the read will go beyond end of scratchpad space
  if( Addr + Len > TopAddr ){
    output->fatal(CALL_INFO, 11, "Error: Hart %" PRIu32 " is attempting to read beyond the highest address "
                                 "in the scratchpad (Addr = 0x %" PRIx64 ", Size = %zu)\n"
                                 "While this scratchpad starts at 0x%" PRIx64 " and ends at 0x%" PRIx64 "\n",
                                 Hart, Addr, Len, BaseAddr, TopAddr);
  }

  // Perform the read
  std::memcpy(Target, &BackingMem[Addr - BaseAddr], Len);

  // TODO: Add scratchpad stats --- memStats.bytesRead += Len;

  // clear the hazard
  req.MarkLoadComplete();
    return;
}


  virtual void WriteMem(unsigned Hart, uint64_t Addr, size_t Len, const void *Data, RevFlag flags) override {
    // Figure out what chunk(s) we're writing to
    size_t BaseChunkNum = (Addr - BaseAddr) / ChunkSize;
    size_t TopChunkNum = (Addr + Len - BaseAddr) / ChunkSize;

    output->verbose(CALL_INFO, 8, 0, "Scratchpad Write: Addr = 0x%" PRIx64 ", Len = %zu, BaseChunkNum = %zu, TopChunkNum = %zu\n", Addr, Len, BaseChunkNum, TopChunkNum);

    // Figure out the bounds of this read request... Need to make sure that all chunks involved in the read are allocated
    for( size_t i=BaseChunkNum; i<=TopChunkNum; i++ ){
      if( FreeList.test(i) ){
        output->fatal(CALL_INFO, 11, "Error: Hart %" PRIu32 " is attempting to write to unallocated memory"
                                    " in the scratchpad (Addr = 0x %" PRIx64 ", Len = %zu)\n", Hart, Addr, Len);
      }
    }

    // write the memory to the BackingStore
    std::memcpy(&BackingMem[Addr - BaseAddr], Data, Len);
  }

  // Override the Init function
  virtual void Initialize() override {
    // TODO: Implement initialization based on baseAddr, size

  }

  // Override the allocation logic
  virtual uint64_t Alloc(const unsigned Hart, const size_t SizeRequested) override {
    // Make sure we have enough space
    if( SizeRequested > Size ){
      output->verbose(CALL_INFO, 11, 0, "Error: Requested allocation size %zu is larger than the scratchpad size %zu\n", SizeRequested, Size);
      return _INVALID_ADDR_;
    }
    uint64_t AllocedAt;
    // figure out how many chunks we need (round up)
    size_t numChunks = (SizeRequested + ChunkSize - 1) / ChunkSize;

    // find N contiguous chunks
    size_t FirstChunk = FindContiguousChunks(numChunks);
    if( FirstChunk == FreeList.size() ){
      output->verbose(CALL_INFO, 1, 0, "Error: Unable to find %zu contiguous chunks in the scratchpad\n", numChunks);
      AllocedAt = _INVALID_ADDR_;
    } else {
      AllocedAt = BaseAddr + FirstChunk * ChunkSize;
      for (size_t i = FirstChunk; i < FirstChunk + numChunks; ++i) {
        FreeList.set(i, false);
      }
    }
    return AllocedAt;
  }

  void Free(uint64_t Addr, size_t Size){
    // Figure out what chunk we're starting at
    size_t BaseChunkNum = (Addr - BaseAddr) / ChunkSize;
    // Figure out how many chunks we need to free (round up)
    size_t numChunks = (Size + ChunkSize - 1) / ChunkSize;
    // Make sure were not trying to free beyond the end of the scratchpad
    if( BaseChunkNum + numChunks > FreeList.size() ){
      output->fatal(CALL_INFO, 6, "Error: Attempting to free memory starting at 0x%" PRIx64 " with size %zu\n"
                               "While this scratchpad starts at 0x%" PRIx64 " and ends at 0x%" PRIx64 "\n",
                               Addr, Size, BaseAddr, TopAddr);
    }
    // Free the chunks
    for (size_t i = BaseChunkNum; i < BaseChunkNum + numChunks; ++i){
      output->verbose(CALL_INFO, 4, 0, "Freeing chunk %zu which is at 0x%" PRIx64 "\n", i, BaseAddr + i * ChunkSize);
      FreeList.set(i);
    }
    return;
  }

  // Used to find N contiguous chunks that are free
  size_t FindContiguousChunks(size_t numChunks){
    size_t count = 0;
    for (size_t i = 0; i < FreeList.size(); ++i) {
        if (FreeList.test(i)) {
        ++count;
        if (count == numChunks) {
          // return the index of the first bit of the n contiguous bits
          return i - numChunks + 1;
        }
      } else {
        count = 0;
      }
    }
  return FreeList.size(); // special value indicating "not found"
}


private:
  // TODO: Figure out how inheritance works (ie. if we need baseAddr, size)
  uint64_t BaseAddr; ///< Base address of the scratchpad
  size_t Size; ///< Size of the scratchpad
  uint64_t ChunkSize; ///< Base address of the scratchpad
  uint64_t TopAddr;  ///< Top address of the scratchpad
// SST::Output *output; ///< Output stream
  unsigned ZapNum;
  std::bitset<_SCRATCHPAD_SIZE_/_CHUNK_SIZE_> FreeList;
  char *BackingMem = nullptr;                           ///< RevMem: Scratchpad memory container for FORZA
};

};

#endif


