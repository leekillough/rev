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

#include "RevCPU.h"
#include "RevMemSegment.h"
#include "SST.h"
#include "../common/include/RevCommon.h"
#include <bitset>

// DEFAULT SCRATCHPAD INFO
#define _CHUNK_SIZE_ 512
#define _SCRATCHPAD_SIZE_ (_CHUNK_SIZE_*1024)
#define _SCRATCHPAD_BASE_ 0x0300000000000000


namespace SST::RevCPU{

class RevScratchpad : public RevCustomMemSegment {
public:
  RevScratchpad(const std::string& Name, RevCPU* CPU, SST::Params& Params, SST::Output *Output)
  : RevCustomMemSegment(Name, "scratchpad", CPU, Params, Output){

    // Set up all attributes for this segment
    ParseAdditionalParams();

    TopAddr = BaseAddr + Size - 1;

    std::cout << "===> Scratchpad:" <<std::endl;
    std::cout << "Size: " << Size << std::endl;
    std::cout << "ChunkSize: " << ChunkSize << std::endl;
    std::cout << "BaseAddr: 0x" << std::hex << BaseAddr << std::endl;
    std::cout << "TopAddr: 0x" << std::hex << TopAddr << std::endl;
    std::cout << "ZapNum: " << ZapNum << std::endl;

    // Allocate the BackingStore
    BackingMem = new char [Size]{};

    // Set size of FreeList & set each chunk to free (ie. true)
    FreeList.resize(Size/ChunkSize, true);
  }

  // Override parsing of params
  virtual void ParseAdditionalParams() override {
    // In the event that we have multiple instances of the same type of custom memory segment
    // we may want to name it to keep it unique... If not, just default to the name being the
    // type string (ie. scratchpad)
    const std::string& segName = Params.find<std::string>("name", "scratchpad");

    // See if we have a custom size, if not, default to the global
    size_t segSize = Params.find<uint64_t>("size", _SCRATCHPAD_SIZE_);

    // See if we have a custom size, if not, default to the global
    size_t chunkSize = Params.find<uint64_t>("chunkSize", _CHUNK_SIZE_);

    // See if we're manually specifying the ZapNum... if not default to the CPU ID
    uint64_t zapNum = Params.find<uint64_t>("zapNum", CPU->getId());

    // See if we're manually specifying the baseAddr, if not, default to the address spec
    uint64_t segBaseAddr = Params.find<size_t>("baseAddr", _SCRATCHPAD_BASE_ + (zapNum * segSize));

    // Make sure we found them
    if( !segSize ){
      // We couldn't find the size/baseAddr for segments, throw error
      output->fatal(CALL_INFO, -1,
                    "Custom memory segment '%s' was found with an invalid size "
                    "or none at all: [%zu Bytes]. \n\nPlease specify the segment size "
                    "as a scoped parameter prefixed by the name of the custom segment "
                    "you previously specified (ex. \"%s.size\")\n\n",
                    Name.c_str(), segSize, Name.c_str());
    }
    // Make sure we found them
    if( !segBaseAddr ){
      // We couldn't find the size/baseAddr for segments, throw error
      output->fatal(CALL_INFO, -1,
                    "Custom memory segment '%s' was found starting at an invalid "
                    "base address or none at all: [0x%" PRIx64 "]. \n\nPlease specify "
                    "the base address of the segment as a scoped parameter "
                    "prefixed by the name of the corresponding segment (ex. \"%s.baseAddr\")\n\n",
                    Name.c_str(), segBaseAddr, Name.c_str());
      }

    ZapNum = zapNum;
    Name = segName;
    // TODO: Figure out what the right way to set these is
    Size = segSize;
    ChunkSize = chunkSize;
    BaseAddr = segBaseAddr;
    setBaseAddr(segBaseAddr);
    setSize(segSize);
  };

  // Implement the pure virtual functions from CustomMemSegment
  virtual void ReadMem(unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req, RevFlag flags) override {
    // Figure out what chunk we're in
    size_t BaseChunkNum = (Addr - BaseAddr) / ChunkSize;
    size_t TopChunkNum = (Addr + Len - BaseAddr) / ChunkSize;

    output->verbose(CALL_INFO, 8, 0, "Scratchpad Read: Addr = 0x%" PRIx64 ", Len = %zu, BaseChunkNum = %zu, TopChunkNum = %zu\n", Addr, Len, BaseChunkNum, TopChunkNum);

    // Figure out the bounds of this read request... Need to make sure that all chunks involved in the read are allocated
    bool isValidRead = std::all_of(FreeList.begin() + BaseChunkNum,
                                   FreeList.begin() + TopChunkNum + 1,
                                   [](bool isFree) { return !isFree; });
    if( !isValidRead ){
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
      if( FreeList[i] ){
        output->fatal(CALL_INFO, 11, "Error: Hart %" PRIu32 " is attempting to write to unallocated memory"
                                    " in the scratchpad (Addr = 0x %" PRIx64 ", Len = %zu)\n", Hart, Addr, Len);
      }
    }

    // write the memory to the BackingStore
    std::memcpy(&BackingMem[Addr - BaseAddr], Data, Len);
  }

  // Override the Init function
  // virtual void Initialize() override { }

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
        FreeList[i] = false;
      }
    }
    return AllocedAt;
  }

  virtual void Dealloc(const unsigned Hart, const uint64_t Addr, const size_t Size) override {
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
      FreeList[i] = true;
    }
    return;
  }

  // Used to find N contiguous chunks that are free
  size_t FindContiguousChunks(const size_t numChunks){
    size_t count = 0;
    for (size_t i = 0; i < FreeList.size(); ++i) {
      if (FreeList[i]) {
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
  uint64_t BaseAddr;          ///< RevScratchpad: Base address of the scratchpad
  size_t Size;                ///< RevScratchpad: Size of the scratchpad
  uint64_t ChunkSize;         ///< RevScratchpad: Base address of the scratchpad
  uint64_t TopAddr;           ///< RevScratchpad: Top address of the scratchpad
  unsigned ZapNum;            ///< RevScratchpad: ZapNum of this scratchpad
  std::vector<bool> FreeList; ///< RevScratchpad: Used for managing what chunks in the scratchpad are free
  char *BackingMem = nullptr; ///< RevScratchpad: Scratchpad memory container for FORZA
};

};

#endif


