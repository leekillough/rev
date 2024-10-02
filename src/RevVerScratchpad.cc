//
// _RevVerScratchpad_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "../include/RevVerScratchpad.h"

namespace SST::RevCPU {

VerilatorScratchpadAPI::VerilatorScratchpadAPI( ComponentId_t id, Params& params ) : SubComponent( id ), output( nullptr ) {

  uint32_t verbosity = params.find<uint32_t>( "verbose" );
  output             = new SST::Output( "[VerilatorScratchpad @t]: ", verbosity, 0, SST::Output::STDOUT );
}

VerilatorScratchpadCtrl::VerilatorScratchpadCtrl( ComponentId_t id, Params& params ) : VerilatorScratchpadAPI( id, params ) {
  // setup the initial logging functions
  //int verbosity                  = params.find<int>( "verbose", 0 );
  //output                         = new SST::Output( "", verbosity, 0, SST::Output::STDOUT );

  const std::string verCtrlClock = params.find<std::string>( "clock", "1GHz" );
  registerClock( verCtrlClock, new Clock::Handler<VerilatorScratchpadCtrl>( this, &VerilatorScratchpadCtrl::clockTick ) );

  clkLink = configureLink(
    "clk", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 0 )
  );
  if( nullptr == clkLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "clk" );
  }
  enLink = configureLink(
    "en", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 1 )
  );
  if( nullptr == enLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "en" );
  }
  writeLink = configureLink(
    "write", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 2 )
  );
  if( nullptr == writeLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "write" );
  }
  lenLink = configureLink(
    "len", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 3 )
  );
  if( nullptr == lenLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "len" );
  }
  addrLink = configureLink(
    "addr", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 4 )
  );
  if( nullptr == addrLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "addr" );
  }
  wdataLink = configureLink(
    "wdata", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 5 )
  );
  if( nullptr == wdataLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "wdata" );
  }
  rdataLink = configureLink(
    "rdata", "0ns", new Event::Handler<VerilatorScratchpadCtrl, unsigned>( this, &VerilatorScratchpadCtrl::RecvPortEvent, 6 )
  );
  if( nullptr == rdataLink ) {
    output->fatal( CALL_INFO, -1, "Error: was unable to configureLink %s \n", "rdata" );
  }
}

VerilatorScratchpadCtrl::~VerilatorScratchpadCtrl() {
  delete output;
}

void VerilatorScratchpadCtrl::RecvPortEvent( SST::Event* ev, unsigned portId ) {
  output->verbose( CALL_INFO, 2, 0, "Received an event\n" );
  // Receive an event... should only be receiving port events

  SST::VerilatorSST::PortEvent* fromPort = static_cast<SST::VerilatorSST::PortEvent*>( ev );

  if( fromPort ) {
    output->verbose( CALL_INFO, 2, 0, "Received a port event from verilatorSST\n" );
    if( portId == 0 ) {
      //event from clk port

    } else if( portId == 1 ) {
      //event from en port

    } else if( portId == 2 ) {
      //event from write port

    } else if( portId == 3 ) {
      //event from len port

    } else if( portId == 4 ) {
      //event from addr port

    } else if( portId == 5 ) {
      //event from wdata port

    } else if( portId == 6 ) {
      //event from rdata port
      // really this is the only port that should be receiving events . . .
      output->verbose( CALL_INFO, 8, 0, "Receiving rdata (expected size %ld)\n", sizeReading );
      if( sizeReading == 1 ) {
        uint8_t tmp                                              = fromPort->getPacket()[0];
        *( reinterpret_cast<uint8_t*>( &readBuffer[sizeRead] ) ) = tmp;
        output->verbose( CALL_INFO, 8, 0, "rdata received: %x\n", tmp );
      } else if( sizeReading == 2 ) {
        uint16_t tmp                                              = ConvertToUint<uint16_t>( fromPort->getPacket() );
        *( reinterpret_cast<uint16_t*>( &readBuffer[sizeRead] ) ) = tmp;
        output->verbose( CALL_INFO, 8, 0, "rdata received: %x\n", tmp );
      } else if( sizeReading == 4 ) {
        uint32_t tmp                                              = ConvertToUint<uint32_t>( fromPort->getPacket() );
        *( reinterpret_cast<uint32_t*>( &readBuffer[sizeRead] ) ) = tmp;
        output->verbose( CALL_INFO, 8, 0, "rdata received: %x\n", tmp );
      } else if( sizeReading == 8 ) {
        uint64_t tmp                                              = ConvertToUint<uint64_t>( fromPort->getPacket() );
        *( reinterpret_cast<uint64_t*>( &readBuffer[sizeRead] ) ) = tmp;
        output->verbose( CALL_INFO, 8, 0, "rdata received: %lx\n", tmp );
      } else {
        output->fatal( CALL_INFO, -1, "Error: allegedly receiving rdata of invalid size . . .\n" );
      }
      sizeRead += sizeReading;

    } else {
      //error -- portId not recognized
      output->fatal( CALL_INFO, -1, "Error: portId not recognized\n" );
    }

    delete fromPort;
    return;
  }
}

void VerilatorScratchpadCtrl::ScheduleWriteOp( uint64_t Address, uint64_t Data, uint8_t Len ) {
  if( Len > 3 ) {
    output->fatal( CALL_INFO, -1, "Error: ScheduleWriteOp failed because of invalid data size\n" );
  }
  output->verbose( CALL_INFO, 10, 0, "Write scratch op queued with Len=%d Address=%lx Data=%lx\n", Len, Address, Data );
  // in VerScratchpad module, 11->8 bytes, 10->4bytes, 01->2 bytes, 00->1 byte
  scratchQueue.emplace( Len, Address, Data );  // push write op
  scratchQueue.emplace();                      // push end write op
}

void VerilatorScratchpadCtrl::ScheduleReadOp( uint64_t Address, uint8_t Len ) {
  if( Len > 3 ) {
    output->fatal( CALL_INFO, -1, "Error: ScheduleReadOp failed because of invalid data size\n" );
  }
  output->verbose( CALL_INFO, 10, 0, "Read scratch op queued with Len=%d Address=%lx\n", Len, Address );
  scratchQueue.emplace( Len, Address );  // push read operation
  scratchQueue.emplace( Len );           // push catch rdata operation
}

bool VerilatorScratchpadCtrl::WriteMem( uint64_t Addr, size_t Len, const void* Data ) {
  // Len is in bytes here...
  opQueue.emplace( Addr, Len, Data );
  uint8_t* toWrite = new uint8_t[Len];
  memcpy( toWrite, Data, Len );  // buffer up the write data
  writeBuffer.emplace( toWrite );
  return true;
}

/* Consider reorganizing these to be sequential instead of all buffered on the verilog side, same for writes */
bool VerilatorScratchpadCtrl::ReadMem( uint64_t Addr, size_t Len, void* Target, const MemReq& req ) {
  opQueue.emplace( Addr, Len, Target, req );
  return true;
}

void VerilatorScratchpadCtrl::SplitWrite( uint64_t Addr, size_t Len ) {
  int partialSize  = Len % 8;
  int bytesWritten = 0;
  output->verbose( CALL_INFO, 10, 0, "Splitting write op\n" );
  const uint8_t* Data = writeBuffer.front();
  if( Len >= 8 ) {
    size_t ops = Len / 8;
    for( size_t i = 0; i < ops; i++ ) {
      const uint64_t* typedData = reinterpret_cast<const uint64_t*>( Data ) + i;
      output->verbose( CALL_INFO, 10, 0, "Write 8 byte data chunk: %lx\n", *typedData );
      ScheduleWriteOp( Addr + i * 8, *typedData, 3 );
      bytesWritten += 8;
    }
  }
  if( partialSize >= 4 ) {
    //const uint32_t* typedData = reinterpret_cast<const uint32_t*>( static_cast<const uint8_t*>( Data ) + bytesWritten );
    const uint32_t* typedData = reinterpret_cast<const uint32_t*>( Data + bytesWritten );
    output->verbose( CALL_INFO, 10, 0, "Write 4 byte data chunk: %x\n", *typedData );
    ScheduleWriteOp( Addr + bytesWritten, *typedData, 2 );
    partialSize -= 4;
    bytesWritten += 4;
  }
  if( partialSize >= 2 ) {
    //const uint16_t* typedData = reinterpret_cast<const uint16_t*>( static_cast<const uint8_t*>( Data ) + bytesWritten );
    const uint16_t* typedData = reinterpret_cast<const uint16_t*>( Data + bytesWritten );
    output->verbose( CALL_INFO, 10, 0, "Write 4 byte data chunk: %x\n", *typedData );
    ScheduleWriteOp( Addr + bytesWritten, *typedData, 1 );
    partialSize -= 2;
    bytesWritten += 2;
  }
  if( partialSize > 0 ) {
    const uint8_t* typedData = Data + bytesWritten;
    output->verbose( CALL_INFO, 10, 0, "Write 1 byte data chunk: %x\n", *typedData );
    ScheduleWriteOp( Addr + bytesWritten, *typedData, 0 );
  }
}

void VerilatorScratchpadCtrl::SplitRead( uint64_t Addr, size_t Len, void* Target, const MemReq& req ) {
  int partialSize = Len % 8;
  int bytesRead   = 0;
  output->verbose( CALL_INFO, 10, 0, "Splitting read op\n" );
  if( Len >= 8 ) {
    size_t ops = Len / 8;
    for( size_t i = 0; i < ops; i++ ) {
      ScheduleReadOp( Addr + i * 8, 3 );
      bytesRead += 8;
    }
  }
  if( partialSize >= 4 ) {
    ScheduleReadOp( Addr + bytesRead, 2 );
    partialSize -= 4;
    bytesRead += 4;
  }
  if( partialSize >= 2 ) {
    ScheduleReadOp( Addr + bytesRead, 1 );
    partialSize -= 2;
    bytesRead += 2;
  }
  if( partialSize > 0 ) {
    ScheduleReadOp( Addr + bytesRead, 0 );
  }
}

void VerilatorScratchpadCtrl::DoScratchOp( const ScratchOp& toDo ) {
  if( !toDo.en ) {
    // set en low
    output->verbose( CALL_INFO, 10, 0, "Doing en low op\n" );
    std::vector<uint8_t>          enLow   = { 0 };
    SST::VerilatorSST::PortEvent* enEvent = new SST::VerilatorSST::PortEvent( enLow );
    enLink->send( enEvent );
    if( toDo.rdata ) {
      output->verbose( CALL_INFO, 10, 0, "Polling read data\n" );
      // catch rdata
      SST::VerilatorSST::PortEvent* rdEvent = new SST::VerilatorSST::PortEvent();
      rdataLink->send( rdEvent );
    }
  } else {
    // set en high
    std::vector<uint8_t>          enHigh  = { 1 };
    SST::VerilatorSST::PortEvent* enEvent = new SST::VerilatorSST::PortEvent( enHigh );
    enLink->send( enEvent );
    std::vector<uint8_t>          setAddr   = ConvertToPacket<uint64_t>( toDo.address );
    SST::VerilatorSST::PortEvent* addrEvent = new SST::VerilatorSST::PortEvent( setAddr );
    addrLink->send( addrEvent );
    std::vector<uint8_t>          setLen   = { toDo.len };
    SST::VerilatorSST::PortEvent* lenEvent = new SST::VerilatorSST::PortEvent( setLen );
    lenLink->send( lenEvent );
    if( toDo.write ) {
      // write op
      std::vector<uint8_t>          writeFlag  = { 1 };
      SST::VerilatorSST::PortEvent* writeEvent = new SST::VerilatorSST::PortEvent( writeFlag );
      writeLink->send( writeEvent );
      std::vector<uint8_t> toWrite;
      switch( toDo.len ) {
      case 0: toWrite.push_back( static_cast<uint8_t>( toDo.wdata ) ); break;
      case 1: toWrite = ConvertToPacket<uint16_t>( toDo.wdata ); break;
      case 2: toWrite = ConvertToPacket<uint32_t>( toDo.wdata ); break;
      case 3: toWrite = ConvertToPacket<uint64_t>( toDo.wdata ); break;
      }
      SST::VerilatorSST::PortEvent* wdataEvent = new SST::VerilatorSST::PortEvent( toWrite );
      wdataLink->send( wdataEvent );
      output->verbose( CALL_INFO, 10, 0, "Doing write with len=%d addr=%lx wdata=%lx\n", toDo.len, toDo.address, toDo.wdata );
    } else {
      // read op
      std::vector<uint8_t>          writeFlag  = { 0 };
      SST::VerilatorSST::PortEvent* writeEvent = new SST::VerilatorSST::PortEvent( writeFlag );
      writeLink->send( writeEvent );
      sizeReading = ( toDo.len == 3 ) ? 8 : ( toDo.len == 2 ) ? 4 : ( toDo.len == 1 ) ? 2 : 1;
      output->verbose( CALL_INFO, 10, 0, "Doing read with len=%d addr=%lx\n", toDo.len, toDo.address );
    }
  }
}

void VerilatorScratchpadCtrl::DumpReadBuff() {
  output->verbose( CALL_INFO, 10, 0, "Dumping read buffer (%ld bytes):\n", sizeRead );
  uint64_t* casted;
  if( !readBuffer ) {
    output->fatal( CALL_INFO, -1, "Error: Trying to dump read buffer which isn't allocated\n" );
  }
  casted = reinterpret_cast<uint64_t*>( readBuffer );
  if( sizeRead > 8 ) {
    for( size_t i = 0; i < sizeRead / 8; i++ ) {
      output->verbose( CALL_INFO, 10, 0, "%lx:\t%lx\n", i * 8, *( casted + i ) );
    }
  } else {
    output->verbose( CALL_INFO, 10, 0, "%x:\t%lx\n", 0, *casted );
  }
}

void VerilatorScratchpadCtrl::init( unsigned int phase ) {}

void VerilatorScratchpadCtrl::setup() {}

bool VerilatorScratchpadCtrl::clockTick( Cycle_t cycle ) {
  if( !scratchBusy && !opQueue.empty() ) {
    // check op buffer and perform next op (set read, set write, or catch rdata)
    MacroOp nextOp = opQueue.front();
    scratchBusy    = true;
    if( !nextOp.Target ) {
      output->verbose( CALL_INFO, 2, 0, "New write op Addr=%lx Len=%ld Data=%p\n", nextOp.Addr, nextOp.Len, nextOp.Data );
      // no Target means its a overarching write op
      // queue up the required scratchpad operations
      // Do not pass data, it will be pulled from the writeBuffer front
      SplitWrite( nextOp.Addr, nextOp.Len );
    } else {
      // overarching read op
      // queue up the required scratchpad operations
      output->verbose( CALL_INFO, 2, 0, "New read op Addr=%lx Len=%ld Target=%p\n", nextOp.Addr, nextOp.Len, nextOp.Target );
      sizeRead   = 0;                        // reset size of data already read
      readBuffer = new uint8_t[nextOp.Len];  // allocate buffer space for requested read data
      SplitRead( nextOp.Addr, nextOp.Len, nextOp.Target, nextOp.req );
    }

  } else {
    if( !scratchQueue.empty() ) {
      ScratchOp currScratchOp = scratchQueue.front();
      // perform the scratch op
      DoScratchOp( currScratchOp );
      scratchQueue.pop();
    } else if( scratchQueue.empty() && !opQueue.empty() ) {  // shouldn't need to check if opQueue is empty here but TODO in case
      MacroOp currOp = opQueue.front();
      if( currOp.Target ) {  // read op finishing
        // copy data back to requested destination
        output->verbose( CALL_INFO, 4, 0, "Copying back data of size %ld to %p\n", currOp.Len, currOp.Target );
        if( currOp.Len != sizeRead ) {
          output->fatal( CALL_INFO, -1, "Error: Len of read op was %ld but only read %ld \n", currOp.Len, sizeRead );
        }
        DumpReadBuff();  // to debug
        memcpy( currOp.Target, readBuffer, currOp.Len );
        delete[] readBuffer;
        // clear the hazard
        output->verbose( CALL_INFO, 4, 0, "Ending read and marking complete\n" );
        if( currOp.req.MarkLoadCompleteFunc != nullptr ) {
          currOp.req.MarkLoadComplete();
        }
        sizeRead    = 0;
        sizeReading = 0;
      } else {
        // if write op ending, need to free the write buffer space and remove the pointer
        uint8_t* bufferedData = writeBuffer.front();
        delete[] bufferedData;
        writeBuffer.pop();
      }
      opQueue.pop();
      scratchBusy = false;
    }
  }
  // cycle verilated model's clock
  std::vector<uint8_t>          clkHigh    = { 1 };
  SST::VerilatorSST::PortEvent* risingEdge = new SST::VerilatorSST::PortEvent( clkHigh );
  clkLink->send( risingEdge );
  std::vector<uint8_t>          clkLow      = { 0 };
  SST::VerilatorSST::PortEvent* fallingEdge = new SST::VerilatorSST::PortEvent( clkLow );
  clkLink->send( fallingEdge );

  return false;
}

}  // namespace SST::RevCPU
