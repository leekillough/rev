//
// _RevMem_h_
//
// Copyright (C) 2017-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVCPU_REVMEM_H_
#define _SST_REVCPU_REVMEM_H_

#define __PAGE_SIZE__ 4096

// -- C++ Headers
#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

// -- SST Headers
#include "SST.h"

// -- RevCPU Headers
#include "RevCommon.h"
#include "RevMemCtrl.h"
#include "RevOpts.h"
#include "RevRand.h"
#include "RevTracer.h"

// -- FORZA Headers
#include "RingNet.h"
#include "ZOPNET.h"

#ifndef _REVMEM_BASE_
#define _REVMEM_BASE_ 0x00000000
#endif

#define _STACK_SIZE_         ( size_t{ 1024 * 1024 } )

#define Z_ADDR_MASK          0xFFFFFFFFFF
#define Z_ZONE_MASK          0b111
#define Z_PREC_MASK          0x1FFF
#define Z_SP_MASK            0b01
#define Z_VIEW_MASK          0b01
#define Z_SEG_MASK           0x3F
#define Z_ADDR_SHIFT         0x00
#define Z_ZONE_SHIFT         40
#define Z_PREC_SHIFT         43
#define Z_SP_SHIFT           56
#define Z_VIEW_SHIFT         57
#define Z_SEG_SHIFT          58

// KL: Since depth == NHARTS, and each HART can only be requesting one spawn presently, I'm simplifying.
//#define SP_REQ_FIFO_D       = NHARTS;
//#define SP_REQ_FIFO_D_LOG2  = LOG_NHARTS;
#define SP_TRACK_FIFO_D      32
#define SP_TRACK_FIFO_D_LOG2 5
#define RING_REQ_FIFO_D      32
#define RING_REQ_FIFO_D_LOG2 5
#define WB_FIFO_D            16
#define WB_FIFO_D_LOG2       4

namespace SST::RevCPU {

class RevMem {
public:
  /// RevMem: standard constructor
  RevMem( uint64_t MemSize, RevOpts* Opts, SST::Output* Output );

  /// RevMem: standard memory controller constructor
  RevMem( uint64_t memSize, RevOpts* opts, RevMemCtrl* ctrl, SST::Output* output );

  /// RevMem: standard destructor
  ~RevMem() { delete[] physMem; }

  /// RevMem: disallow copying and assignment
  RevMem( const RevMem& )            = delete;
  RevMem& operator=( const RevMem& ) = delete;

  /* Virtual Memory Blocks  */
  class MemSegment {
  public:
    MemSegment( uint64_t baseAddr, uint64_t size ) : BaseAddr( baseAddr ), Size( size ), TopAddr( baseAddr + size ) {}

    uint64_t getTopAddr() const { return BaseAddr + Size; }

    uint64_t getBaseAddr() const { return BaseAddr; }

    uint64_t getSize() const { return Size; }

    void setBaseAddr( uint64_t baseAddr ) {
      BaseAddr = baseAddr;
      if( Size ) {
        TopAddr = Size + BaseAddr;
      }
    }

    void setSize( uint64_t size ) {
      Size    = size;
      TopAddr = BaseAddr + size;
    }

    /// MemSegment: Check if vAddr is included in this segment
    bool contains( const uint64_t& vAddr ) { return ( vAddr >= BaseAddr && vAddr < TopAddr ); };

    // Check if a given range is inside a segment
    bool contains( const uint64_t& vBaseAddr, const uint64_t& Size ) {
      // exclusive top address
      uint64_t vTopAddr = vBaseAddr + Size - 1;
      return contains( vBaseAddr ) && contains( vTopAddr );
    };

    /// MemSegment: Override for easy std::cout << *Seg << std::endl;
    friend std::ostream& operator<<( std::ostream& os, const MemSegment& Seg ) {
      os << "| 0x" << std::hex << std::setw( 16 ) << std::setfill( '0' ) << Seg.getBaseAddr() << " | 0x" << std::hex
         << std::setw( 16 ) << std::setfill( '0' ) << Seg.getTopAddr() << " | " << std::dec << std::setw( 10 )
         << std::setfill( ' ' ) << Seg.getSize() << " Bytes |";

      return os;
    }

    /// MemSegment: Override the less than operator
    bool operator<( const MemSegment& other ) const { return BaseAddr < other.BaseAddr; }

    /// MemSegment: Override the greater than operator
    bool operator>( const MemSegment& other ) const { return BaseAddr > other.BaseAddr; }

    /// MemSegment: Override the equality operator
    bool operator==( const MemSegment& other ) const { return BaseAddr == other.BaseAddr; }

    /// MemSegment: Override the not equal operator
    bool operator!=( const MemSegment& other ) const { return BaseAddr != other.BaseAddr; }

    /// MemSegment: Override the less than or equal operator
    bool operator<=( const MemSegment& other ) const { return BaseAddr <= other.BaseAddr; }

    /// MemSegment: Override the greater than or equal operator
    bool operator>=( const MemSegment& other ) const { return BaseAddr >= other.BaseAddr; }

  private:
    uint64_t BaseAddr{};  ///< MemSegment: Base address of the memory segment
    uint64_t Size{};      ///< MemSegment: Size of the memory segment
    uint64_t TopAddr{};   ///< MemSegment: Top address of the memory segment
  };

  /// RevMem: determine if there are any outstanding requests
  bool outstandingRqsts() const { return ctrl && ctrl->outstandingRqsts(); }

  /// RevMem: handle incoming memory event
  void handleEvent( StandardMem::Request* ev ) {}

  /// RevMem: handle memory injection
  void HandleMemFault( uint32_t width );

  /// RevMem: get the stack_top address
  uint64_t GetStackTop() { return stacktop; }

  /// RevMem: set the stack_top address
  void SetStackTop( uint64_t Addr ) { stacktop = Addr; }

  /// RevMem: tracer pointer
  RevTracer* Tracer = nullptr;

  /// RevMem: retrieve the address of the top of memory (not stack)
  uint64_t GetMemTop() { return ( _REVMEM_BASE_ + memSize ); }

  /// RevMem: get the stack_top address
  uint64_t GetStackBottom() { return stacktop - _STACK_SIZE_; }

  /// RevMem: initiate a memory fence
  bool FenceMem( uint32_t Hart ) {
    if( ctrl ) {
      return ctrl->sendFENCE( Hart );
    } else if( zNic && !isRZA ) {
      // generate a Fence packet
      return __ZOP_FENCEHart( Hart );
    }
    return true;  // base RevMem support does nothing here
  }

  /// RevMem: retrieves the cache line size.  Returns 0 if no cache is configured
  uint32_t getLineSize() { return ctrl ? ctrl->getLineSize() : 64; }

  /// RevMem: Enable tracing of load and store instructions.
  void SetTracer( RevTracer* tracer ) { Tracer = tracer; }

  /// RevMem: Allocate and zero thread done vector
  void ThreadQuitSetup( uint32_t num_harts ) { threadQuits.assign( num_harts, 0 ); }

  /// RevMem: Mark quitting thread
  void IssueThreadQuit( uint32_t hart ) { threadQuits[hart] = true; }

  /// RevMem: Finish quitting -- unmark thread quit at minimum
  void FinalizeThreadQuit( uint32_t hart ) { threadQuits[hart] = false; }

  /// RevMem: Check for quitting thread
  bool CheckThreadQuit( uint32_t hart ) { return threadQuits[hart]; }

  // ----------------------------------------------------
  // ---- Base Memory Interfaces
  // ----------------------------------------------------
  /// RevMem: write to the target memory location with the target flags
  bool WriteMem( uint32_t Hart, uint64_t Addr, uint32_t Len, const void* Data, RevFlag flags = RevFlag::F_NONE );

  /// RevMem: read data from the target memory location
  bool ReadMem( uint32_t Hart, uint64_t Addr, uint32_t Len, void* Target, const MemReq& req, RevFlag flags = RevFlag::F_NONE );

  /// RevMem: flush a cache line
  bool FlushLine( uint32_t Hart, uint64_t Addr ) {
    return !ctrl || ctrl->sendFLUSHRequest( Hart, Addr, 0, getLineSize(), RevFlag::F_NONE, false );
  }

  /// RevMem: invalidate a cache line
  bool InvLine( uint32_t Hart, uint64_t Addr ) {
    return !ctrl || ctrl->sendFLUSHRequest( Hart, Addr, 0, getLineSize(), RevFlag::F_NONE, true );
  }

  /// RevMem: clean a line
  bool CleanLine( uint32_t Hart, uint64_t Addr ) { return !ctrl || ( ctrl->sendFENCE( Hart ) && FlushLine( Hart, Addr ) ); }

  // ----------------------------------------------------
  // ---- Read Memory Interfaces
  // ----------------------------------------------------
  /// RevMem: template read memory interface
  template<typename T>
  bool ReadVal( uint32_t Hart, uint64_t Addr, T* Target, const MemReq& req, RevFlag flags ) {
    return ReadMem( Hart, Addr, sizeof( T ), Target, req, flags );
  }

  ///  RevMem: LOAD RESERVE memory interface
  void LR( uint32_t hart, uint64_t addr, size_t len, void* target, const MemReq& req, RevFlag flags );

  ///  RevMem: STORE CONDITIONAL memory interface
  bool SC( uint32_t Hart, uint64_t addr, uint32_t len, void* data, RevFlag flags );

  /// RevMem: template AMO memory interface
  template<typename T>
  bool AMOVal( uint32_t Hart, uint64_t Addr, T* Data, T* Target, const MemReq& req, RevFlag flags ) {
    return AMOMem( Hart, Addr, uint32_t{ sizeof( T ) }, Data, Target, req, flags );
  }

  /// RevMem: Initiated an AMO request
  bool AMOMem( uint32_t Hart, uint64_t Addr, uint32_t Len, void* Data, void* Target, const MemReq& req, RevFlag flags );

  // ----------------------------------------------------
  // ---- Write Memory Interfaces
  // ----------------------------------------------------

  template<typename T>
  void Write( uint32_t Hart, uint64_t Addr, T Value ) {
    if( std::is_same_v<T, float> ) {
      memStats.floatsWritten++;
    } else if( std::is_same_v<T, double> ) {
      memStats.doublesWritten++;
    }

    if( !WriteMem( Hart, Addr, sizeof( T ), &Value ) ) {
      output->fatal(
        CALL_INFO,
        -1,
        std::is_floating_point_v<T> ? "Error: could not write memory (FP%zu)\n" : "Error: could not write memory (U%zu)\n",
        sizeof( T ) * 8
      );
    }
  }

  // ----------------------------------------------------
  // ---- Atomic/Future/LRSC Interfaces
  // ----------------------------------------------------

  /// RevMem: Invalidate Matching LR reservations
  bool InvalidateLRReservations( uint32_t hart, uint64_t addr, size_t len );

  /// RevMem: Initiates a future operation [RV64P only]
  bool SetFuture( uint64_t Addr );

  /// RevMem: Revokes a future operation [RV64P only]
  bool RevokeFuture( uint64_t Addr );

  /// RevMem: Interrogates the target address and returns 'true' if a future reservation is present [RV64P only]
  bool StatusFuture( uint64_t Addr );

  /// RevMem: Randomly assign a memory cost
  uint32_t RandCost( uint32_t Min, uint32_t Max ) { return RevRand( Min, Max ); }

  /// RevMem: Used to access & incremenet the global software PID counter
  uint32_t GetNewThreadPID();

  /// RevMem: Used to set the size of the TLBSize
  void SetTLBSize( uint32_t numEntries ) { tlbSize = numEntries; }

  /// RevMem: Used to set the size of the TLBSize
  void SetMaxHeapSize( uint64_t MaxHeapSize ) { maxHeapSize = MaxHeapSize; }

  /// RevMem: Get memSize value set in .py file
  uint64_t GetMemSize() const { return memSize; }

  ///< RevMem: Get MemSegs vector
  std::vector<std::shared_ptr<MemSegment>>& GetMemSegs() { return MemSegs; }

  ///< RevMem: Get ThreadMemSegs vector
  std::vector<std::shared_ptr<MemSegment>>& GetThreadMemSegs() { return ThreadMemSegs; }

  ///< RevMem: Get FreeMemSegs vector
  std::vector<std::shared_ptr<MemSegment>>& GetFreeMemSegs() { return FreeMemSegs; }

  ///< RevMem: Get DumpRanges vector
  std::map<std::string, std::shared_ptr<MemSegment>>& GetDumpRanges() { return DumpRanges; }

  ///< RevMem: Get DumpRanges vector (const version)
  const std::map<std::string, std::shared_ptr<MemSegment>>& GetDumpRanges() const { return DumpRanges; }

  /// RevMem: Add new MemSegment (anywhere) --- Returns BaseAddr of segment
  uint64_t AddMemSeg( const uint64_t& SegSize );

  /// RevMem: Add new thread mem (starting at TopAddr [growing down])
  std::shared_ptr<MemSegment> AddThreadMem();

  /// RevMem: Add new MemSegment (starting at BaseAddr)
  uint64_t AddMemSegAt( const uint64_t& BaseAddr, const uint64_t& SegSize );

  /// RevMem: Add new MemSegment (starting at BaseAddr) and round it up to the nearest page
  uint64_t AddRoundedMemSeg( uint64_t BaseAddr, const uint64_t& SegSize, size_t RoundUpSize );

  /// RevMem: Add new MemSegment that will be dumped at the dump points specified in the configuration
  void AddDumpRange( const std::string& Name, const uint64_t BaseAddr, const uint64_t SegSize );

  /// RevMem: Removes or shrinks segment
  uint64_t DeallocMem( uint64_t BaseAddr, uint64_t Size );

  /// RevMem: Removes or shrinks segment
  uint64_t AllocMem( const uint64_t& Size );

  /// RevMem: Attempts to allocate memory at a specific address
  uint64_t AllocMemAt( const uint64_t& BaseAddr, const uint64_t& Size );

  /// RevMem: Sets the HeapStart & HeapEnd to EndOfStaticData
  void InitHeap( const uint64_t& EndOfStaticData );

  void SetHeapStart( const uint64_t& HeapStart ) { heapstart = HeapStart; }

  void SetHeapEnd( const uint64_t& HeapEnd ) { heapend = HeapEnd; }

  const uint64_t& GetHeapEnd() { return heapend; }

  // FIXME:
  uint64_t GetBrk() { return brk; }

  void AdjustBrk( const uint64_t NumBytes ) { brk += NumBytes; }

  uint64_t ExpandHeap( uint64_t Size );

  void SetTLSInfo( const uint64_t& BaseAddr, const uint64_t& Size );

  // RevMem: Used to get the TLS BaseAddr & Size
  const uint64_t& GetTLSBaseAddr() { return TLSBaseAddr; }

  const uint64_t& GetTLSSize() { return TLSSize; }

  struct RevMemStats {
    uint64_t TLBHits;
    uint64_t TLBMisses;
    uint64_t floatsRead;
    uint64_t floatsWritten;
    uint64_t doublesRead;
    uint64_t doublesWritten;
    uint64_t bytesRead;
    uint64_t bytesWritten;
  };

  RevMemStats GetAndClearStats() {
    // Add each field from memStats into memStatsTotal
    for( auto stat :
         { &RevMemStats::TLBHits,
           &RevMemStats::TLBMisses,
           &RevMemStats::floatsRead,
           &RevMemStats::floatsWritten,
           &RevMemStats::doublesRead,
           &RevMemStats::doublesWritten,
           &RevMemStats::bytesRead,
           &RevMemStats::bytesWritten } ) {
      memStatsTotal.*stat += memStats.*stat;
    }

    auto ret = memStats;
    memStats = {};  // Zero out memStats
    return ret;
  }

  RevMemStats GetMemStatsTotal() const { return memStatsTotal; }

  // ----------------------------------------------------
  // ---- FORZA Interfaces
  // ----------------------------------------------------

  /// FORZA: set the Zone Ring object
  void setZRing( Forza::RingNetAPI* R ) { zoneRing = R; }

  /// FORZA: set the ZOP NIC object
  void setZNic( Forza::zopAPI* Z ) { zNic = Z; }

  /// FORZA: set the RZA flag for this instance of RevMem
  void setRZA() { isRZA = true; }

  /// FORZA: disable the RZA for this instance of RevMem
  void unsetRZA() { isRZA = false; }

  /// FORZA: handle message response
  bool handleRZAResponse( Forza::zopEvent* zev );

  /// FORZA: insert a new ZOP address request
  void insertZRqst( uint64_t Addr, Forza::zopEvent* zev );

  /// FORZA: check to see if the target address is in the zop request hazard map
  bool isZRqst( uint64_t Addr );

  /// FORZA: clear the taret address from the zop request hazard map
  void clearZRqst( uint64_t Addr );

  /// FORZA: Determine if the target address resides on the same zone
  ///        Returns false if the address is not local and places the
  ///        target Zone and Precinct IDs in `Zone` and `Precinct`, respectively.
  ///        Returns true if the address is local
  bool isLocalAddr( uint64_t vAddr, uint32_t& Zone, uint32_t& Precinct );

  /// FORZA: send a thread migration request
  bool ZOP_ThreadMigrate( uint32_t Hart, std::vector<uint64_t> Payload, uint32_t Zone, uint32_t Precinct );

  // Add Physical Addresss Information
  /// FORZA: update the physical history from the input file
  void updatePhysHistoryfromInput( const std::string& InputFile );

  /// FORZA: update history to the output file
  void updatePhysHistorytoOutput();

  /// FORZA: enable physical history logging
  void enablePhysHistoryLogging();

  /// FORZA: validate a physical address
  std::pair<bool, std::string> validatePhysAddr( uint64_t pAddr, int appID );

  /// FORZA: update physical history
  void updatePhysHistory( uint64_t pAddr, int appID );

  /// FORZA: set the output file name
  void setOutputFile( std::string name );

  /// RevMem: Dump the memory contents
  void DumpMem(
    const uint64_t startAddr, const uint64_t numBytes, const uint64_t bytesPerRow = 16, std::ostream& outputStream = std::cout
  );

  void DumpValidMem( const uint64_t bytesPerRow = 16, std::ostream& outputStream = std::cout );

  void DumpMemSeg(
    const std::shared_ptr<MemSegment>& MemSeg, const uint64_t bytesPerRow = 16, std::ostream& outputStream = std::cout
  );

  void DumpThreadMem( const uint64_t bytesPerRow = 16, std::ostream& outputStream = std::cout );

  /// FORZA: determines if the current spawn instruction has been previously fired
  bool ThreadQIsActive( uint32_t Hart );

  /// FORZA: determines if the current spawn instruction FSM is complete
  bool ThreadQIsComplete( uint32_t Hart );

  /// FORZA: injects a new thread into the ThreadQ FSM
  bool ThreadQInsert( uint32_t Hart, uint64_t TPC, uint64_t X31 );

  /// FORZA: process the ThreadQ FSM: This should ONLY be called from the CPU's clock method
  bool ThreadQProcess();

  /// FORZA: receive response from ZEN during spawn FSM
  void ThreadQReceiveZen( Forza::ringEvent* ev );

  /// FORZA: send word to Zen for spawning
  void FSMSendZenWord( uint16_t Hart, uint64_t Datum );

private:
  /// FORZA: convert a standard RISC-V AMO opcode to a ZOP opcode
  Forza::zopOpc flagToZOP( RevFlag flags, size_t Len );

  /// FORZA: convert a standard RISC-V memory request to a ZOP opcode
  Forza::zopOpc memToZOP( RevFlag flags, size_t Len, bool Write );

  /// FORZA: send an AMO request
  bool ZOP_AMOMem( uint32_t Hart, uint64_t Addr, size_t Len, void* Data, void* Target, const MemReq& req, RevFlag flags );

  /// FORZA: send a READ request
  bool ZOP_READMem( uint32_t Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req, RevFlag flags );

  /// FORZA : send READ request to Zen for HART
  void FSMReadZen( uint16_t Hart );

  /// FORZA: send a WRITE request
  bool ZOP_WRITEMem( uint32_t Hart, uint64_t Addr, size_t Len, const void* Data, RevFlag flags );

  /// FORZA: send a large raw WRITE request: DO NOT USE
  bool __ZOP_WRITEMemLarge( uint32_t Hart, uint64_t Addr, size_t Len, const void* Data, RevFlag flags );

  /// FORZA: send a WRITE request using the target opcode: DO NOT USE
  bool __ZOP_WRITEMemBase( uint32_t Hart, uint64_t Addr, size_t Len, const void* Data, RevFlag flags, SST::Forza::zopOpc opc );

  /// FORZA: send a HART fence request
  bool __ZOP_FENCEHart( uint32_t Hart );

protected:
  unsigned char* physMem = nullptr;  ///< RevMem: memory container

private:
  RevMemStats memStats{};
  RevMemStats memStatsTotal{};

  uint64_t memSize{};      ///< RevMem: size of the target memory
  uint32_t tlbSize{};      ///< RevMem: number of entries in the TLB
  uint64_t maxHeapSize{};  ///< RevMem: maximum size of the heap
  std::unordered_map<uint64_t, std::pair<uint64_t, std::list<uint64_t>::iterator>> TLB{};
  std::list<uint64_t> LRUQueue{};  ///< RevMem: List ordered by last access for implementing LRU policy when TLB fills up
  RevOpts*            opts{};      ///< RevMem: options object
  RevMemCtrl*         ctrl{};      ///< RevMem: memory controller object
  SST::Output*        output{};    ///< RevMem: output handler

  Forza::RingNetAPI* zoneRing{};  ///< RevMem: FORZA RingNet; Necessary for spawn messages
  Forza::zopAPI*     zNic{};      ///< RevMem: FORZA ZOP NIC
  bool               isRZA{};     ///< RevMem: FORZA RZA flag; true if this device is an RZA

  std::vector<std::shared_ptr<MemSegment>> MemSegs{};        // Currently Allocated MemSegs
  std::vector<std::shared_ptr<MemSegment>> FreeMemSegs{};    // MemSegs that have been unallocated
  std::vector<std::shared_ptr<MemSegment>> ThreadMemSegs{};  // For each RevThread there is a corresponding MemSeg (TLS & Stack)
  std::map<std::string, std::shared_ptr<MemSegment>> DumpRanges{};  // Mem ranges to dump at points specified in the configuration

  uint64_t TLSBaseAddr       = 0;                   ///< RevMem: TLS Base Address
  uint64_t TLSSize           = sizeof( uint32_t );  ///< RevMem: TLS Size (minimum size is enough to write the TID)
  uint64_t ThreadMemSize     = _STACK_SIZE_;        ///< RevMem: Size of a thread's memory segment (StackSize + TLSSize)
  uint64_t NextThreadMemAddr = memSize;             ///< RevMem: Next top address for a new thread's memory

  uint64_t SearchTLB( uint64_t vAddr );                    ///< RevMem: Used to check the TLB for an entry
  void     AddToTLB( uint64_t vAddr, uint64_t physAddr );  ///< RevMem: Used to add a new entry to TLB & LRUQueue
  void     FlushTLB();                                     ///< RevMem: Used to flush the TLB & LRUQueue
  uint64_t
    CalcPhysAddr( uint64_t pageNum, uint64_t vAddr );  ///< RevMem: Used to calculate the physical address based on virtual address
  std::tuple<uint64_t, uint64_t, uint64_t>
       AdjPageAddr( uint64_t Addr, uint64_t Len );  ///< RevMem: Used to adjust address crossing pages
  bool isValidVirtAddr( uint64_t vAddr );           ///< RevMem: Used to check if a virtual address exists in MemSegs

  std::map<uint64_t, std::pair<uint32_t, bool>> pageMap{};    ///< RevMem: map of logical to pair<physical addresses, allocated>
  uint32_t                                      pageSize{};   ///< RevMem: size of allocated pages
  uint32_t                                      addrShift{};  ///< RevMem: Bits to shift to caclulate page of address
  uint32_t                                      nextPage{};   ///< RevMem: next physical page to be allocated. Will result in index
  /// nextPage * pageSize into physMem

  uint64_t brk{};         ///< RevMem: Program BRK FIXME: HACK
  uint64_t mmapRegion{};  ///< RevMem: FIXME: HACK
  uint64_t heapend{};     ///< RevMem: top of the stack
  uint64_t heapstart{};   ///< RevMem: top of the stack
  uint64_t stacktop{};    ///< RevMem: top of the stack

  std::vector<bool> threadQuits;  ///< RevMem: array of threads to enable quit signaling to RevCore

  std::vector<uint64_t>                                     FutureRes{};  ///< RevMem: future operation reservations
  std::unordered_map<uint32_t, std::pair<uint64_t, size_t>> LRSC{};       ///< RevMem: load reserve/store conditional set

  // -- FORZA
  std::map<uint64_t, Forza::zopEvent*> ZRqst;  ///< RevMem: zop request address map

  // FORZA Security Test
  std::map<uint64_t, std::tuple<std::string, bool, int>>              OutputPhysAddrHist;  //History to Output file
  std::map<uint64_t, std::tuple<std::string, bool, std::vector<int>>> InputPhysAddrHist;   //Read from Input file
  bool                                                                PhysAddrCheck{};
  bool                                                                PhysAddrLogging{};
  std::string                                                         outputFile;

  // KL: WIP SPAWN/PZOP LOGIC HEREIN
  // FORZA Thread State
  enum class ThreadQState {
    Inserted = 0,
    Complete = 1,
  };
  // Hart, TPC, X31, ThreadQState enum
  std::vector<std::tuple<uint32_t, uint64_t, uint64_t, ThreadQState>> ThreadQ;

  // I'm using this to make it so an entire vector isn't traversed per check-in from
  // HART on its spawns.
  std::map<uint32_t, ThreadQState> InsertionStatTrack;

  /* // Hold control info while status is read
   typedef struct packed {
      logic        pzp_valid;
      logic        spn_valid;
      tcb_t        tcb;
      logic [4:0]  func5; // this has something to do with instructions to run -- and apparently does take 3 registers at times. It shows up in instruction formats.
      logic [63:0] rs1_data;
      logic [63:0] rs2_data;
      logic [63:0] rs3_data;
      regnum_t     rd;
   } sp_track_t;*/

  // Every bool herein is tied to a 'logic' in RTL, which supports 4 states.
  // However, in terms of behavior, they seem to be evaluated in terms of on/off, so I use a bool here instead
  // of managing 2 bits.
  struct spawnTracker {
    bool     pzpValid;
    bool     spnValid;
    uint32_t tcb;
    uint8_t  func5;  // Technically too big -- defined as 4 bits.
    uint64_t rs1Data;
    uint64_t rs2Data;
    uint64_t rs3Data;
    uint8_t  rd;  // I think this is too big -- refers to a register number, defined in ZAP doc as 4 bits.
  };

  /*
   typedef struct  packed {
      logic        valid;
      exec_op_t    op;
      hart_t       hart;
      tcb_t        tcb;
      logic [4:0]  func5;
      logic [63:0] rs1_data;
      logic [63:0] rs2_data;
      logic [63:0] rs3_data;
      regnum_t     rd;
   } spn_pzp_late_t;     //used
*/

  struct spawnPZOPInput {
    bool     valid;
    uint8_t  op;
    uint32_t hart;
    uint32_t tcb;
    uint8_t  func5;
    uint64_t rs1Data;
    uint64_t rs2Data;
    uint64_t rs3Data;
    uint8_t  rd;
  };

  /* for
   inst_sp_req_q (
                  .clk         (clk),
                  .resetn      (resetn),
                  .i_data_in   (sp_req_q_wdata),
                  .i_write     (sp_req_q_wr),
                  .o_data_out  (sp_req_q_rdata),
                  // verilator lint_off PINCONNECTEMPTY
                  .o_full      (),
                  .o_near_full (),
                  .o_used      (),
                  // lint_on
                  .o_empty     (sp_req_q_empty),
                  .i_read      (sp_req_q_rd)
                  );
		  */
  // request queue
  std::queue<spawnPZOPInput> sp_req_q;
  // data waiting to be used for request fulfillment
  std::queue<spawnTracker> sp_track_q;

  struct zenSpawnStatus {
    uint16_t hart;
    bool     zenEn;
    bool     SPWriteError;
    bool     busy;
  };

  std::queue<zenSpawnStatus> ring_rtn_q;

  enum class FSMState {
    IDLE        = 0,
    STATUS_RD   = 1,
    STATUS_RTN  = 2,
    SEND_PC_TCB = 3,
    SEND_WRD_1  = 4,
    SEND_WRD_2  = 5,
    SEND_WRD_3  = 6,
    SEND_WRD_4  = 7,
    WR_BACK     = 8,
  };

  FSMState sp_state      = FSMState::IDLE;
  FSMState next_sp_state = FSMState::IDLE;

  const char* FSMReportState( FSMState state );

  /*   typedef enum [1:0] {
                 NA,
                 CSR_READ,
                 CSR_WRITE,
                 CSR_RMW
                 } csr_func_t;*/

  // use Forza::ringMsgT to represent csr_func_t
  // update == write, read == read, return_data isn't used here but is a simulator-only command
  // for returning data from ring to service read.

  /*typedef enum logic [ZXB_ZC_WID-1:0]
{
  ZAP0  = 0,
  ZAP1  = 1,
  ZAP2  = 2,
  ZAP3  = 3,
  RZA0  = 4, //CXL0
  RZA1  = 5, //CXL1
  RZA2  = 6,
  RZA3  = 7, // HBM
  ZEN   = 8,
  ZQM   = 9
} zone_comp_t;*/
  // Satisfied by Forza::zopCompID

  /*   typedef struct packed
                  {
                     csr_func_t                        func;
                     zone_comp_t                       dev_no;
                     logic [CSR_AID_WID-1:0]           aid;
                     logic [CSR_RING_ADDRESS_WID-1:0]  addr;
                     logic                             sp_vld;
                     logic [ZXB_ZAPS_PER_ZONE_WID-1:0] zapid;
                     logic [ZXB_HART_ID_WID-1:0]       hartid;
                     logic [ZXB_DATA_WID-1:0]          data;
                  } csr_ring_t;*/

  // Might be defined elsewhere?
  struct ringMessage {
    Forza::ringMsgT  func;
    Forza::zopCompID dev_no;
    uint32_t         aid;
    uint32_t         addr;
    bool             sp_vld;
    uint32_t         zapid;
    uint32_t         hartid;
    uint32_t         data;
  };

  std::queue<ringMessage> ring_req_q;

  /*    typedef struct packed { // Send to pc_tcb_mux without tcb update info
      logic        reg_valid;
      hart_t       hart;
      regnum_t     reg_no;
      logic [63:0] data;
   } wb_tcb_pc_t;          //used*/

  struct wb_tcb_pc_t {
    bool     regValid;
    uint32_t hartID;
    uint32_t regNumber;
    uint64_t data;
  };

  std::queue<wb_tcb_pc_t> wbQueue;
  spawnTracker            sp_track_busy_data;
  bool                    sp_track_data_vld;
  uint16_t                sp_wr_hart;
  uint16_t                status_rtn_hart;
  bool                    status_busy;
  bool                    spn_valid;
  bool                    pzp_valid;
  // std::ofstream output_file;
  // std::ofstream input_file;

};  // class RevMem

}  // namespace SST::RevCPU

#endif

// EOF
