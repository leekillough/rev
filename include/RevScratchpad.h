//
// _RevScratchpad_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVSCRATCHPAD_H_
#define _SST_REVCPU_REVSCRATCHPAD_H_

#include "../common/include/RevCommon.h"
#include "RevVerScratchpad.h"
#include "SST.h"
#include <bitset>

// FORZA: SCRATCHPAD
#define _CHUNK_SIZE_      512
#define _SCRATCHPAD_SIZE_ ( _CHUNK_SIZE_ * 1024 )
#define _SCRATCHPAD_BASE_ 0x0300000000000000

namespace SST::RevCPU {

class RevScratchpad {
public:
  RevScratchpad( const unsigned ZapNum, size_t Size, size_t ChunkSize, SST::Output* Output )
    : ZapNum( ZapNum ), Size( Size ), ChunkSize( ChunkSize ), output( Output ), VerScratch( nullptr ) {
    // Create the byte array
    // TODO: Verify this is correct
    BaseAddr += ZapNum * Size;
    BackingMem = new char[Size]{};
    FreeList.set();
    TopAddr = BaseAddr + Size - 1;
  }

  RevScratchpad( const unsigned ZapNum, size_t Size, size_t ChunkSize, SST::Output* Output, VerilatorScratchpadAPI* Scratch )
    : ZapNum( ZapNum ), Size( Size ), ChunkSize( ChunkSize ), output( Output ), VerScratch( Scratch ) {
    // Using verilator model scratchpad; do NOT initalize BackingMem
    // TODO: Verify this is correct
    BaseAddr += ZapNum * Size;
    //BackingMem = new char[Size]{};
    FreeList.set();
    TopAddr = BaseAddr + Size - 1;
  }

  uint64_t GetBaseAddr() const { return BaseAddr; }

  uint64_t GetTopAddr() const { return TopAddr; }

  size_t GetSize() const { return Size; }

  uint64_t Alloc( size_t SizeRequested ) {
    // Make sure we have enough space
    if( SizeRequested > Size ) {
      output->verbose(
        CALL_INFO,
        11,
        0,
        "Error: Requested allocation size %zu is larger than "
        "the scratchpad size %zu\n",
        SizeRequested,
        Size
      );
      return _INVALID_ADDR_;
    }
    uint64_t AllocedAt;
    // figure out how many chunks we need (round up)
    size_t numChunks  = ( SizeRequested + ChunkSize - 1 ) / ChunkSize;

    // find N contiguous chunks
    size_t FirstChunk = FindContiguousChunks( numChunks );
    if( FirstChunk == FreeList.size() ) {
      output->verbose( CALL_INFO, 1, 0, "Error: Unable to find %zu contiguous chunks in the scratchpad\n", numChunks );
      AllocedAt = _INVALID_ADDR_;
    } else {
      AllocedAt = BaseAddr + FirstChunk * ChunkSize;
      for( size_t i = FirstChunk; i < FirstChunk + numChunks; ++i ) {
        FreeList.set( i, false );
      }
    }
    return AllocedAt;
  }

  void Free( uint64_t Addr, size_t Size ) {
    // Figure out what chunk we're starting at
    size_t BaseChunkNum = ( Addr - BaseAddr ) / ChunkSize;
    // Figure out how many chunks we need to free (round up)
    size_t numChunks    = ( Size + ChunkSize - 1 ) / ChunkSize;
    // Make sure were not trying to free beyond the end of the scratchpad
    if( BaseChunkNum + numChunks > FreeList.size() ) {
      output->fatal(
        CALL_INFO,
        6,
        "Error: Attempting to free memory starting at 0x%" PRIx64 " with size %zu\n"
        "While this scratchpad starts at 0x%" PRIx64 " and ends at 0x%" PRIx64 "\n",
        Addr,
        Size,
        BaseAddr,
        TopAddr
      );
    }
    // Free the chunks
    for( size_t i = BaseChunkNum; i < BaseChunkNum + numChunks; ++i ) {
      output->verbose( CALL_INFO, 4, 0, "Freeing chunk %zu which is at 0x%" PRIx64 "\n", i, BaseAddr + i * ChunkSize );
      FreeList.set( i );
    }
    return;
  }

  // Used to find N contiguous chunks that are free
  size_t FindContiguousChunks( size_t numChunks ) {
    size_t count = 0;
    for( size_t i = 0; i < FreeList.size(); ++i ) {
      if( FreeList.test( i ) ) {
        ++count;
        if( count == numChunks ) {
          // return the index of the first bit of the n contiguous bits
          return i - numChunks + 1;
        }
      } else {
        count = 0;
      }
    }
    return FreeList.size();  // special value indicating "not found"
  }

  bool Contains( uint64_t Addr ) { return ( Addr >= BaseAddr && Addr < BaseAddr + Size ); }

  /// RevScratchpad: Attempts to allocate numBytes in the scratchpad
  uint64_t ScratchpadAlloc( size_t numBytes );

  /// RevScratchpad: Attempts to allocate numBytes in the scratchpad
  bool ReadMem(
    unsigned      Hart,
    uint64_t      Addr,
    size_t        Len,
    void*         Target,
    const MemReq& req
  );  //Interfaces::StandardMem::Request::flags_t flags);

  /// RevScratchpad: Attempts to allocate numBytes in the scratchpad
  bool WriteMem( unsigned Hart, uint64_t Addr, size_t Len,
                 const void* Data );  // Interfaces::StandardMem::Request::flags_t flags);

  ~RevScratchpad() { delete[] BackingMem; }

private:
  unsigned                                      ZapNum;
  size_t                                        Size;
  size_t                                        ChunkSize;
  std::bitset<_SCRATCHPAD_SIZE_ / _CHUNK_SIZE_> FreeList;
  char*                                         BackingMem = nullptr;            ///< RevMem: Scratchpad memory container for FORZA
  uint64_t                                      BaseAddr   = _SCRATCHPAD_BASE_;  ///< RevMem: Base address of the scratchpad
  uint64_t                                      TopAddr;                         ///< RevScratchpad: Base address of the scratchpad
  SST::Output*                                  output;                          ///< RevScratchpad: Output stream
  VerilatorScratchpadAPI* VerScratch = nullptr;  ///< RevScratchpad: Pointer to verilator-backed scratchpad API
};
}  // namespace SST::RevCPU

#endif

// EOF
