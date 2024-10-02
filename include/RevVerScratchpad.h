//
// _RevVerScratchpad_h_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_REVVERSCRATCHPAD_H_
#define _SST_REVVERSCRATCHPAD_H_

#include <queue>

#include "../common/include/RevCommon.h"
#include "../include/verilatorSSTAPI.h"
#include "SST.h"

namespace SST::RevCPU {

// struct representing one ReadMem or WriteMem call, allowing Reads/Writes to be buffered
// when the scratchpad is busy
struct MacroOp {
  uint64_t     Addr;
  size_t       Len;
  void*        Target;
  const void*  Data;
  const MemReq req;

  // constructor for an overarching Write operation
  MacroOp( uint64_t Addr, size_t Len, const void* Data ) : Addr( Addr ), Len( Len ), Target( nullptr ), Data( Data ) {}

  // constructor for an overarching Read operation
  MacroOp( uint64_t Addr, size_t Len, void* Target, const MemReq& req )
    : Addr( Addr ), Len( Len ), Target( Target ), Data( nullptr ), req( req ) {}
};

// struct representing a single operation on the scratchpad as is acceptable by the verilated module
// up to size 8 bytes
struct ScratchOp {
  uint8_t  write;    // write flag (1=write, 0=read)
  uint8_t  len;      // 2 bit op length in bytes(0=1, 1=2, 2=4, 3=8)
  uint8_t  en;       // enable signal
  uint8_t  rdata;    // serves as flag to indicate op is catching rdata; does not hold it
  uint64_t address;  // address of the op
  uint64_t wdata;    // write data

  // write op constructor
  ScratchOp( uint8_t len, uint64_t address, uint64_t wdata )
    : write( 1 ), len( len ), en( 1 ), rdata( 0 ), address( address ), wdata( wdata ) {}

  // read op constructor
  ScratchOp( uint8_t len, uint64_t address ) : write( 0 ), len( len ), en( 1 ), rdata( 0 ), address( address ), wdata( 0 ) {}

  // catch read op constructor. catch rdata and set en low
  ScratchOp( uint8_t len ) : write( 0 ), len( len ), en( 0 ), rdata( 1 ), address( 0 ), wdata( 0 ) {}

  // end write op constructor. sets en low
  ScratchOp() : write( 0 ), len( 0 ), en( 0 ), rdata( 0 ), address( 0 ), wdata( 0 ) {}
};

/**
 * VerilatorScratchpadAPI : Handles the subcomponent Scratchpad Verilator API
 */
class VerilatorScratchpadAPI : public SST::SubComponent {
public:
  SST_ELI_REGISTER_SUBCOMPONENT_API( SST::RevCPU::VerilatorScratchpadAPI )

  SST_ELI_DOCUMENT_PARAMS( { "verbose", "Set the verbosity of output for the memory controller", "0" } )

  /// VerilatorScratchpadAPI: constructor
  VerilatorScratchpadAPI( ComponentId_t id, Params& params );

  /// VerilatorScratchpadAPI: default destructor
  virtual ~VerilatorScratchpadAPI()       = default;

  /// VerilatorScratchpadAPI: initialization function
  virtual void init( unsigned int phase ) = 0;

  /// VerilatorScratchpadAPI: setup function (currently empty)
  virtual void setup() {}

  /// VerilatorScratchpadAPI: write data into the scratchpad module
  virtual bool WriteMem( unsigned Hart, uint64_t Addr, size_t Len, const void* Data )               = 0;

  /// VerilatorScratchpadAPI: read data from the scratchpad module
  virtual bool ReadMem( unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req ) = 0;

protected:
  SST::Output* output;  ///< VerilatorScratchpadAPI: SST output object

};  /// end VerilatorScratchpadAPI

/**
 * VerilatorScratchpadCtrl: the Rev verilator controller subcomponent
 */
class VerilatorScratchpadCtrl : public VerilatorScratchpadAPI {
public:
  // Register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    VerilatorScratchpadCtrl,
    "revcpu",
    "VerilatorScratchpadCtrl",
    SST_ELI_ELEMENT_VERSION( 1, 0, 0 ),
    "Controller for verilator-produced scratchpad module",
    SST::RevCPU::VerilatorScratchpadAPI
  )

  // Register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "clock", "Clock frequency of the controller", "1Ghz" },
    { "port", "Port to use, if loaded as an anonymous subcomponent", "network" },  //TODO
    { "verbose", "Verbosity for output (0 = nothing)", "0" },
  )

  // Register the ports
  SST_ELI_DOCUMENT_PORTS(
    { "clk", "clock port", { "" } },  // TODO: should clk port also be limited to PortEvent or driven by the subcomponent's clock?
    { "en", "enable port", { "SST::VerilatorSST::PortEvent" } },
    { "write", "write or read flag (write is high)", { "SST::VerilatorSST::PortEvent" } },
    { "len", "sets read/write width (1, 2, 4, 8 bytes; 2 bit)", { "SST::VerilatorSST::PortEvent" } },
    { "addr", "address for the operation (64-bit)", { "SST::VerilatorSST::PortEvent" } },
    { "wdata", "data to be written (64-bit)", { "SST::VerilatorSST::PortEvent" } },
    { "rdata", "data fetched by the read (64-bit)", { "SST::VerilatorSST::PortEvent" } }
  )

  // Register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()  //TODO

  /// VerilatorScratchpadCtrl: constructor
  VerilatorScratchpadCtrl( ComponentId_t id, Params& params );

  /// VerilatorScratchpadCtrl: destructor
  virtual ~VerilatorScratchpadCtrl();

  /// VerilatorScratchpadCtrl: initialization function
  virtual void init( unsigned int phase );

  /// VerilatorScratchpadCtrl: setup function
  virtual void setup();

  /// VerilatorScratchpadCtrl: clock function
  virtual bool clockTick( Cycle_t cycle );

  /// VerilatorScratchpadCtrl: write memory to the scratchpad
  virtual bool WriteMem( unsigned Hart, uint64_t Addr, size_t Len, const void* Data );

  /// VerilatorScratchpadCtrl: read memory from the scratchpad
  virtual bool ReadMem( unsigned Hart, uint64_t Addr, size_t Len, void* Target, const MemReq& req );

  /// VerilatorScratchpadCtrl: helper encapsulating one write operation
  void ScheduleWriteOp( uint64_t Address, uint64_t Data, uint8_t Len );

  /// VerilatorScratchpadCtrl: helper encapsulating one read operation
  void ScheduleReadOp( uint64_t Address, uint8_t Len );

  /// VerilatorScratchpadCtrl: takes params from WriteMem call and buffers the relevant ScratchOps
  void SplitWrite( uint64_t Addr, size_t Len );

  /// VerilatorScratchpadCtrl: takes params from ReadMem call and buffers the relevant ScratchOps
  void SplitRead( uint64_t Addr, size_t Len, void* Target, const MemReq& req );

  /// VerilatorScratchpadCtrl: sends a single "op" to the verilated scratchpad
  void DoScratchOp( const ScratchOp& toDo );

  void DumpReadBuff();

  /// VerilatorScratchpadCtrl: convert a data type (uint) to a uint8 vector; helper for creating PortEvents
  template<typename T>
  std::vector<uint8_t> ConvertToPacket( T Data ) {
    std::vector<uint8_t> Packet;
    for( size_t i = 0; i < sizeof( T ); i++ ) {
      Packet.push_back( ( Data >> ( i * 8 ) ) & 255 );
    }
    return Packet;
  }

  /// VerilatorScratchpadCtrl: convert a std::vector<uint8_t> to a uint of requested size; helper for receiving rdata
  template<typename T>
  T ConvertToUint( const std::vector<uint8_t>& Packet ) {
    T tmp = 0;
    for( size_t i = 0; i < sizeof( T ) - 1; i++ ) {
      tmp += ( Packet[sizeof( T ) - i - 1] & 255U );
      tmp = tmp << 8;
    }
    tmp += ( Packet[0] & 255U );
    return tmp;
  }

protected:
  SST::Link*            clkLink;
  SST::Link*            enLink;
  SST::Link*            writeLink;
  SST::Link*            lenLink;
  SST::Link*            addrLink;
  SST::Link*            wdataLink;
  SST::Link*            rdataLink;
  bool                  scratchBusy = false;
  uint8_t*              readBuffer;   // destination for next received read data
  size_t                sizeRead;     // size of data that has already been read
  size_t                sizeReading;  // size of read for current scratchOp
  std::queue<uint8_t*>  writeBuffer;  // place to store write data to ensure it doesn't change prior to the operation occurring
  std::queue<MacroOp>   opQueue;
  std::queue<ScratchOp> scratchQueue;

  void RecvPortEvent( SST::Event* ev, unsigned portId );

  /// VerilatorScratchpadCtrl: disallow copying and assignment
  VerilatorScratchpadCtrl( const VerilatorScratchpadCtrl& )            = delete;
  VerilatorScratchpadCtrl& operator=( const VerilatorScratchpadCtrl& ) = delete;
};  // end VerilatorScratchpadCtrl

}  // namespace SST::RevCPU

#endif  // _SST_REVVERSCRATCHPAD_H_
