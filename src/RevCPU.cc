//
// _RevCPU_cc_
//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RevCPU.h"
#include "RevMem.h"
#include "RevThread.h"
#include <cmath>
#include <fstream>
#include <memory>

namespace SST::RevCPU {

using MemSegment        = RevMem::MemSegment;

const char splash_msg[] = R"(
*******
/**////**
/**   /**   *****  **    **
/*******   **///**/**   /**
/**///**  /*******//** /**
/**  //** /**////  //****
/**   //**//******  //**
//     //  //////    //
)";

RevCPU::RevCPU( SST::ComponentId_t id, const SST::Params& params ) : SST::Component( id ) {

  const int Verbosity = params.find<int>( "verbose", 0 );

  // Initialize the output handler
  output.init( "RevCPU[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  // Register a new clock handler
  const std::string cpuClock = params.find<std::string>( "clock", "1GHz" );
  ClockHandler               = new SST::Clock::Handler<RevCPU>( this, &RevCPU::clockTick );
  timeConverter              = registerClock( cpuClock, ClockHandler );

  // Inform SST to wait until we authorize it to exit
  EnableRZA                  = params.find<bool>( "enableRZA", 0 );
  if( !EnableRZA ) {
    // RZA's do not call these are they are effectively peripheral components
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();
  }

  // std::string ClockFreq = params.find<std::string>("clock", "1Ghz");
  // printf("given Txt files : %s\n",txtFile.c_str());
  // Derive the simulation parameters
  // We must always derive the number of cores before initializing the options
  numCores = params.find<uint32_t>( "numCores", "1" );
  numHarts = params.find<uint32_t>( "numHarts", "1" );

  // Make sure someone isn't trying to have more than _MAX_HARTS_ harts per core
  if( numHarts > _MAX_HARTS_ ) {
    output.fatal( CALL_INFO, -1, "Error: number of harts must be <= %" PRIu32 "\n", uint32_t{ _MAX_HARTS_ } );
  }
  output.verbose(
    CALL_INFO, 1, 0, "Building Rev with %" PRIu32 " cores and %" PRIu32 " hart(s) on each core \n", numCores, numHarts
  );

  // read the binary executable name
  auto Exe = params.find<std::string>( "program", "a.out" );

  // Create the options object
  Opts     = std::make_unique<RevOpts>( numCores, numHarts, Verbosity );

  // Program arguments
  Opts->SetArgs( params );

  // Initialize the remaining options
  for( auto [ParamName, InitFunc] : {
         std::pair( "startAddr", &RevOpts::InitStartAddrs ),
         std::pair( "startSymbol", &RevOpts::InitStartSymbols ),
         std::pair( "machine", &RevOpts::InitMachineModels ),
         std::pair( "table", &RevOpts::InitInstTables ),
         std::pair( "memCost", &RevOpts::InitMemCosts ),
         std::pair( "prefetchDepth", &RevOpts::InitPrefetchDepth ),
       } ) {
    std::vector<std::string> optList;
    params.find_array( ParamName, optList );
    if( !( Opts.get()->*InitFunc )( optList ) )
      output.fatal( CALL_INFO, -1, "Error: failed to initialize %s\n", ParamName );
  }

  // See if we should load the network interface controller
  EnableNIC = params.find<bool>( "enable_nic", 0 );

  if( EnableNIC ) {
    // Look up the network component

    Nic = loadUserSubComponent<nicAPI>( "nic" );

    // check to see if the nic was loaded.  if not, DO NOT load an anonymous endpoint
    if( !Nic )
      output.fatal( CALL_INFO, -1, "Error: no NIC object loaded into RevCPU\n" );

    Nic->setMsgHandler( new Event::Handler<RevCPU>( this, &RevCPU::handleMessage ) );

    // record the number of injected messages per cycle
    msgPerCycle = params.find<unsigned>( "msgPerCycle", 1 );
  }

  // Look for the fault injection logic
  EnableFaults = params.find<bool>( "enable_faults", 0 );
  if( EnableFaults ) {
    std::vector<std::string> faults;
    params.find_array<std::string>( "faults", faults );
    DecodeFaultCodes( faults );

    std::string width = params.find<std::string>( "fault_width", "1" );
    DecodeFaultWidth( width );

    fault_width = params.find<int64_t>( "fault_range", "65536" );
    FaultCntr   = fault_width;
  }

  // Create the memory object
  const uint64_t memSize = params.find<unsigned long>( "memSize", 1073741824 );
  EnableMemH             = params.find<bool>( "enableMemH", 0 );

  // Added for the security test
  EnableForzaSecurity    = params.find<bool>( "enableForzaSecurity", false );

  if( EnableForzaSecurity ) {
    memTrafficInput  = params.find<std::string>( "memTrafficInput", "" );
    memTrafficOutput = params.find<std::string>( "memTrafficOutput", "" );
  }

  if( !EnableMemH ) {
    Mem = std::make_unique<RevMem>( memSize, Opts.get(), &output );
  } else {
    Ctrl = std::unique_ptr<RevMemCtrl>( loadUserSubComponent<RevMemCtrl>( "memory" ) );
    if( !Ctrl )
      output.fatal( CALL_INFO, -1, "Error : failed to inintialize the memory controller subcomponent\n" );
    Mem = std::make_unique<RevMem>( memSize, Opts.get(), Ctrl.get(), &output );

    if( EnableFaults )
      output.verbose( CALL_INFO, 1, 0, "Warning: memory faults cannot be enabled with memHierarchy support\n" );
  }

  // if the forza security models are enabled, inform RevMem that logging as been enabled
  if( EnableForzaSecurity ) {
    if( memTrafficInput != "nil" ) {
      output.verbose( CALL_INFO, 1, 0, "Enabling MemTrafficInput : %s\n", memTrafficInput.c_str() );
      Mem->updatePhysHistoryfromInput( memTrafficInput );
    }
    if( memTrafficOutput != "nil" ) {
      output.verbose( CALL_INFO, 1, 0, "Enabling MemTrafficOutput : %s\n", memTrafficOutput.c_str() );
      Mem->setOutputFile( memTrafficOutput );
      Mem->enablePhysHistoryLogging();
    }
  }

  // FORZA: initialize the network
  // setup the FORZA NoC NIC endpoint for the Zone
  // Note that this must occur AFTER memory initialization
  EnableZopNIC = params.find<bool>( "enableZoneNIC", 0 );
  if( EnableZopNIC ) {
    output.verbose( CALL_INFO, 4, 0, "[FORZA] Enabling zone NIC on device=%s\n", getName().c_str() );
    Precinct = params.find<unsigned>( "precinctId", 0 );
    Zone     = params.find<unsigned>( "zoneId", 0 );
    zNic     = loadUserSubComponent<Forza::zopAPI>( "zone_nic" );
    if( !zNic ) {
      output.fatal( CALL_INFO, -1, "Error: no ZONE NIC object loaded into RevCPU\n" );
    }
    zNic->setNumHarts( numHarts );
    zNic->setPrecinctID( Precinct );
    zNic->setZoneID( Zone );

    // set the message handler for the NoC interface
    zNic->setMsgHandler( new Event::Handler<RevCPU>( this, &RevCPU::handleZOPMessage ) );

    // set the message id generator
    zNicMsgIds = new Forza::zopMsgID();
    Mem->setZNic( zNic );

    // now that the NIC has been loaded, we need to ensure that the NIC knows
    // what type of endpoint it is
    if( EnableRZA ) {
      // This Rev instance is an RZA
      if( !EnableMemH ) {
        output.fatal(
          CALL_INFO,
          -1,
          "Error : memHierarchy is required if the respective Rev "
          "instance is an RZA\n"
        );
      }
      zNic->setEndpointType( Forza::zopCompID::Z_RZA );
      output.verbose( CALL_INFO, 4, 0, "[FORZA] device=%s initialized as RZA device\n", getName().c_str() );
      // ensure the memory controller knows that it is an RZA device
      Mem->setRZA();
    } else {
      // This Rev instance is a ZAP
      Mem->unsetRZA();
      unsigned         zap   = params.find<unsigned>( "zapId", 0 );
      Forza::zopCompID zapId = Forza::zopCompID::Z_ZAP0;
      switch( zap ) {
      case 0: zapId = Forza::zopCompID::Z_ZAP0; break;
      case 1: zapId = Forza::zopCompID::Z_ZAP1; break;
      case 2: zapId = Forza::zopCompID::Z_ZAP2; break;
      case 3: zapId = Forza::zopCompID::Z_ZAP3; break;
      default: output.fatal( CALL_INFO, -1, "Error: zapId is out of range [0-7]\n" ); break;
      }
      zNic->setEndpointType( zapId );
      output.verbose( CALL_INFO, 4, 0, "[FORZA] device=%s initialized as ZAP device: ZAP%d\n", getName().c_str(), zap );

      zoneRing = loadUserSubComponent<SST::Forza::RingNetAPI>( "ring_nic" );
      if( zoneRing ) {
        output.verbose( CALL_INFO, 4, 0, "[FORZA] device=%s create zone ring\n", getName().c_str() );
        zoneRing->setMsgHandler( new Event::Handler<RevCPU>( this, &RevCPU::handleRingMsg ) );
        zoneRing->setEndpointType( zapId );
      } else {
        output.verbose( CALL_INFO, 4, 0, "[FORZA] device=%s failed to create zone ring\n", getName().c_str() );
      }
      output.flush();
    }
  }

  // Set TLB Size
  const uint64_t tlbSize = params.find<unsigned long>( "tlbSize", 512 );
  Mem->SetTLBSize( tlbSize );

  // Set max heap size
  const uint64_t maxHeapSize = params.find<unsigned long>( "maxHeapSize", memSize / 4 );
  Mem->SetMaxHeapSize( maxHeapSize );

  // Load the binary into memory
  Loader       = std::make_unique<RevLoader>( Exe, Opts->GetArgv(), Mem.get(), &output, !EnableZopNIC || EnableRZA );

  EnableCoProc = params.find<bool>( "enableCoProc", 0 );
  if( EnableCoProc ) {

    // Create the processor objects
    Procs.reserve( Procs.size() + numCores );
    for( unsigned i = 0; i < numCores; i++ ) {
      RevCore* tmpNewRevCore = new RevCore( i, Opts.get(), numHarts, Mem.get(), Loader.get(), this->GetNewTID(), &output );
      tmpNewRevCore->setZNic( zNic );
      Procs.push_back( std::unique_ptr<RevCore>( tmpNewRevCore ) );
    }
    // Create the co-processor objects
    for( unsigned i = 0; i < numCores; i++ ) {
      RevCoProc* CoProc = loadUserSubComponent<RevCoProc>( "co_proc", SST::ComponentInfo::SHARE_NONE, Procs[i].get() );
      if( !CoProc ) {
        output.fatal( CALL_INFO, -1, "Error : failed to inintialize the co-processor subcomponent\n" );
      }
      Procs[i]->SetCoProc( CoProc );
      CoProcs.push_back( std::unique_ptr<RevCoProc>( CoProc ) );
    }
  } else if( EnableRZA ) {
    // retrieve each of the RZA pipeline models
    // NOTE:
    // We currently disable the RZOP pipeline as the opcodes
    // are not defined in version 3.3.0 of the spec
    if( numCores != 2 ) {
      output.fatal( CALL_INFO, -1, "Error : FORZA RZA devices require at least 2 cores\n" );
    }

    // Force the coprocs to be enabled
    EnableCoProc = true;

    Procs.reserve( Procs.size() + numCores );
    for( unsigned i = 0; i < numCores; i++ ) {
      auto tmpNewRevCore =
        std::make_unique<RevCore>( i, Opts.get(), numHarts, Mem.get(), Loader.get(), this->GetNewTID(), &output );
      tmpNewRevCore->setZNic( zNic );
      Procs.push_back( std::move( tmpNewRevCore ) );
    }

    RevCoProc* LSProc =
      loadUserSubComponent<RevCoProc>( "rza_ls", SST::ComponentInfo::SHARE_NONE, Procs[Forza::Z_MZOP_PIPE_HART].get() );
    if( !LSProc ) {
      output.fatal( CALL_INFO, -1, "Error : failed to initialize the RZA LS pipeline\n" );
    }
    LSProc->setMem( Mem.get() );
    LSProc->setZNic( zNic );

    RevCoProc* AMOProc =
      loadUserSubComponent<RevCoProc>( "rza_amo", SST::ComponentInfo::SHARE_NONE, Procs[Forza::Z_HZOP_PIPE_HART].get() );
    if( !AMOProc ) {
      output.fatal( CALL_INFO, -1, "Error : failed to initialize the RZA AMO pipeline\n" );
    }
    AMOProc->setMem( Mem.get() );
    AMOProc->setZNic( zNic );

    CoProcs.push_back( std::unique_ptr<RevCoProc>( LSProc ) );
    CoProcs.push_back( std::unique_ptr<RevCoProc>( AMOProc ) );
    Procs[Forza::Z_MZOP_PIPE_HART]->SetCoProc( LSProc );
    Procs[Forza::Z_HZOP_PIPE_HART]->SetCoProc( AMOProc );
  } else {
    // Create the processor objects
    Procs.reserve( Procs.size() + numCores );
    for( unsigned i = 0; i < numCores; i++ ) {
      auto tmpNewRevCore =
        std::make_unique<RevCore>( i, Opts.get(), numHarts, Mem.get(), Loader.get(), this->GetNewTID(), &output );
      tmpNewRevCore->setZNic( zNic );
      tmpNewRevCore->setZNicMsgIds( zNicMsgIds );
      tmpNewRevCore->setZRing( zoneRing );
      Procs.push_back( std::move( tmpNewRevCore ) );
    }
  }

  // Memory dumping option(s)
  std::vector<std::string> memDumpRanges;
  params.find_array( "memDumpRanges", memDumpRanges );
  if( !memDumpRanges.empty() ) {
    for( auto& segName : memDumpRanges ) {
      // FIXME: Figure out how to parse units (GB, MB, KB, etc.)
      // segName is a string... Look for scoped params for this segment
      const auto& scopedParams = params.get_scoped_params( segName );
      // Check scopedParams for the following:
      // - startAddr (hex)
      // - size (bytes)
      if( !scopedParams.contains( "startAddr" ) ) {
        output.fatal(
          CALL_INFO,
          -1,
          "Error: memDumpRanges requires startAddr. Please specify this scoped "
          "param as %s.startAddr in the configuration file\n",
          segName.c_str()
        );
      } else if( !scopedParams.contains( "size" ) ) {
        output.fatal(
          CALL_INFO,
          -1,
          "Error: memDumpRanges requires size. Please specify this "
          "scoped param as %s.size in the configuration file\n",
          segName.c_str()
        );
      }
      const uint64_t startAddr = scopedParams.find<uint64_t>( "startAddr" );
      const uint64_t size      = scopedParams.find<uint64_t>( "size" );
      Mem->AddDumpRange( segName, startAddr, size );
    }
  }

#ifndef NO_REV_TRACER
  // Configure tracer and assign to each core
  if( output.getVerboseLevel() >= 5 ) {
    for( unsigned i = 0; i < numCores; i++ ) {
      // Each core gets its very own tracer
      RevTracer*  trc = new RevTracer( getName(), &output );
      std::string diasmType;
      Opts->GetMachineModel( 0, diasmType );  // TODO first param is core
      if( trc->SetDisassembler( diasmType ) )
        output.verbose( CALL_INFO, 1, 0, "Warning: tracer could not find disassembler. Using REV default\n" );

      trc->SetTraceSymbols( Loader->GetTraceSymbols() );

      // tracer user controls - cycle on and off. Ignored unless > 0
      trc->SetStartCycle( params.find<uint64_t>( "trcStartCycle", 0 ) );
      trc->SetCycleLimit( params.find<uint64_t>( "trcLimit", 0 ) );
      trc->SetCmdTemplate( params.find<std::string>( "trcOp", TRC_OP_DEFAULT ).c_str() );

      // clear trace states
      trc->Reset();

      // Assign to components
      Procs[i]->SetTracer( trc );
      if( Ctrl )
        Ctrl->setTracer( trc );
    }
  }
#endif
  // Setup timeConverter
  for( size_t i = 0; i < Procs.size(); i++ ) {
    Procs[i]->SetTimeConverter( timeConverter );
  }

  // Initial thread setup
  uint32_t MainThreadID = id + 1;  // Prevents having MainThreadID == 0 which is reserved for INVALID

  uint64_t    StartAddr = 0x00ull;
  std::string StartSymbol;

  bool     IsStartSymbolProvided   = Opts->GetStartSymbol( id, StartSymbol );
  bool     IsStartAddrProvided     = Opts->GetStartAddr( id, StartAddr ) && StartAddr != 0x00ull;
  uint64_t ResolvedStartSymbolAddr = ( IsStartSymbolProvided ) ? Loader->GetSymbolAddr( StartSymbol ) : 0x00ull;

  // If no start address has been provided ...
  if( !IsStartAddrProvided ) {
    // ... check if symbol was provided ...
    if( !IsStartSymbolProvided ) {
      // ... no, try to default to 'main' ...
      StartAddr = Loader->GetSymbolAddr( "main" );
      if( StartAddr == 0x00ull ) {
        // ... no hope left!
        output.fatal(
          CALL_INFO,
          -1,
          "Error: failed to auto discover address for <main> for "
          "main thread\n"
        );
      }
    } else {
      // ... if symbol was provided, check whether it is valid or not ...
      if( !ResolvedStartSymbolAddr ) {
        // ... not valid, error out
        output.fatal( CALL_INFO, -1, "Error: failed to resolve address for symbol <%s>\n", StartSymbol.c_str() );
      }
      // ... valid use the resolved symbol
      StartAddr = ResolvedStartSymbolAddr;
    }
  } else {  // A start address was provided ...
    // ... check if a symbol was provided and is compatible with the start address ...
    if( ( IsStartSymbolProvided ) && ( ResolvedStartSymbolAddr != StartAddr ) ) {
      // ... they are different, don't know the user intent so error out now
      output.fatal(
        CALL_INFO,
        -1,
        "Error: start address and start symbol differ startAddr=0x%" PRIx64 " StartSymbol=%s ResolvedStartSymbolAddr=0x%" PRIx64
        "\n",
        StartAddr,
        StartSymbol.c_str(),
        ResolvedStartSymbolAddr
      );
    }  // ... else no symbol provided, continue on with StartAddr as the target
  }

  output.verbose( CALL_INFO, 11, 0, "Start address is 0x%" PRIx64 "\n", StartAddr );

  InitMainThread( MainThreadID, StartAddr );

  // setup the per-proc statistics
  TotalCycles.reserve( numCores );
  CyclesWithIssue.reserve( numCores );
  FloatsRead.reserve( numCores );
  FloatsWritten.reserve( numCores );
  DoublesRead.reserve( numCores );
  DoublesWritten.reserve( numCores );
  BytesRead.reserve( numCores );
  BytesWritten.reserve( numCores );
  FloatsExec.reserve( numCores );
  TLBHitsPerCore.reserve( numCores );
  TLBMissesPerCore.reserve( numCores );

  for( unsigned s = 0; s < numCores; s++ ) {
    auto core = "core_" + std::to_string( s );
    TotalCycles.push_back( registerStatistic<uint64_t>( "TotalCycles", core ) );
    CyclesWithIssue.push_back( registerStatistic<uint64_t>( "CyclesWithIssue", core ) );
    FloatsRead.push_back( registerStatistic<uint64_t>( "FloatsRead", core ) );
    FloatsWritten.push_back( registerStatistic<uint64_t>( "FloatsWritten", core ) );
    DoublesRead.push_back( registerStatistic<uint64_t>( "DoublesRead", core ) );
    DoublesWritten.push_back( registerStatistic<uint64_t>( "DoublesWritten", core ) );
    BytesRead.push_back( registerStatistic<uint64_t>( "BytesRead", core ) );
    BytesWritten.push_back( registerStatistic<uint64_t>( "BytesWritten", core ) );
    FloatsExec.push_back( registerStatistic<uint64_t>( "FloatsExec", core ) );
    TLBHitsPerCore.push_back( registerStatistic<uint64_t>( "TLBHitsPerCore", core ) );
    TLBMissesPerCore.push_back( registerStatistic<uint64_t>( "TLBMissesPerCore", core ) );
  }

  // determine whether we need to enable/disable manual coproc clocking
  DisableCoprocClock    = params.find<bool>( "independentCoprocClock", 0 );

  // Create the completion array
  Enabled               = std::vector<bool>( numCores );

  const unsigned Splash = params.find<bool>( "splash", 0 );

  if( Splash > 0 )
    output.verbose( CALL_INFO, 1, 0, splash_msg );

  // Done with initialization
  output.verbose( CALL_INFO, 1, 0, "Initialization of RevCPUs complete.\n" );

  for( const auto& [Name, Seg] : Mem->GetDumpRanges() ) {
    // Open a file called '{Name}.init.dump'
    std::ofstream dumpFile( Name + ".dump.init", std::ios::binary );
    Mem->DumpMemSeg( Seg, 16, dumpFile );
  }
}

void RevCPU::DecodeFaultWidth( const std::string& width ) {
  fault_width = 1;  // default to single bit failures

  if( width == "single" ) {
    fault_width = 1;
  } else if( width == "word" ) {
    fault_width = 8;
  } else {
    fault_width = std::stoi( width );
  }

  if( fault_width > 64 ) {
    output.fatal( CALL_INFO, -1, "Fault width must be <= 64 bits" );
  }
}

void RevCPU::DecodeFaultCodes( const std::vector<std::string>& faults ) {
  if( faults.empty() ) {
    output.fatal( CALL_INFO, -1, "No fault codes defined" );
  }

  EnableCrackFaults = EnableMemFaults = EnableRegFaults = EnableALUFaults = false;

  for( auto& fault : faults ) {
    if( fault == "decode" ) {
      EnableCrackFaults = true;
    } else if( fault == "mem" ) {
      EnableMemFaults = true;
    } else if( fault == "reg" ) {
      EnableRegFaults = true;
    } else if( fault == "alu" ) {
      EnableALUFaults = true;
    } else if( fault == "all" ) {
      EnableCrackFaults = EnableMemFaults = EnableRegFaults = EnableALUFaults = true;
    } else {
      output.fatal( CALL_INFO, -1, "Undefined fault code: %s", fault.c_str() );
    }
  }
}

void RevCPU::registerStatistics() {
  SyncGetSend        = registerStatistic<uint64_t>( "SyncGetSend" );
  SyncPutSend        = registerStatistic<uint64_t>( "SyncPutSend" );
  AsyncGetSend       = registerStatistic<uint64_t>( "AsyncGetSend" );
  AsyncPutSend       = registerStatistic<uint64_t>( "AsyncPutSend" );
  SyncStreamGetSend  = registerStatistic<uint64_t>( "SyncStreamGetSend" );
  SyncStreamPutSend  = registerStatistic<uint64_t>( "SyncStreamPutSend" );
  AsyncStreamGetSend = registerStatistic<uint64_t>( "AsyncStreamGetSend" );
  AsyncStreamPutSend = registerStatistic<uint64_t>( "AsyncStreamPutSend" );
  ExecSend           = registerStatistic<uint64_t>( "ExecSend" );
  StatusSend         = registerStatistic<uint64_t>( "StatusSend" );
  CancelSend         = registerStatistic<uint64_t>( "CancelSend" );
  ReserveSend        = registerStatistic<uint64_t>( "ReserveSend" );
  RevokeSend         = registerStatistic<uint64_t>( "RevokeSend" );
  HaltSend           = registerStatistic<uint64_t>( "HaltSend" );
  ResumeSend         = registerStatistic<uint64_t>( "ResumeSend" );
  ReadRegSend        = registerStatistic<uint64_t>( "ReadRegSend" );
  WriteRegSend       = registerStatistic<uint64_t>( "WriteRegSend" );
  SingleStepSend     = registerStatistic<uint64_t>( "SingleStepSend" );
  SetFutureSend      = registerStatistic<uint64_t>( "SetFutureSend" );
  RevokeFutureSend   = registerStatistic<uint64_t>( "RevokeFutureSend" );
  StatusFutureSend   = registerStatistic<uint64_t>( "StatusFutureSend" );
  SuccessSend        = registerStatistic<uint64_t>( "SuccessSend" );
  FailedSend         = registerStatistic<uint64_t>( "FailedSend" );
  BOTWSend           = registerStatistic<uint64_t>( "BOTWSend" );
  SyncGetRecv        = registerStatistic<uint64_t>( "SyncGetRecv" );
  SyncPutRecv        = registerStatistic<uint64_t>( "SyncPutRecv" );
  AsyncGetRecv       = registerStatistic<uint64_t>( "AsyncGetRecv" );
  AsyncPutRecv       = registerStatistic<uint64_t>( "AsyncPutRecv" );
  SyncStreamGetRecv  = registerStatistic<uint64_t>( "SyncStreamGetRecv" );
  SyncStreamPutRecv  = registerStatistic<uint64_t>( "SyncStreamPutRecv" );
  AsyncStreamGetRecv = registerStatistic<uint64_t>( "AsyncStreamGetRecv" );
  AsyncStreamPutRecv = registerStatistic<uint64_t>( "AsyncStreamPutRecv" );
  ExecRecv           = registerStatistic<uint64_t>( "ExecRecv" );
  StatusRecv         = registerStatistic<uint64_t>( "StatusRecv" );
  CancelRecv         = registerStatistic<uint64_t>( "CancelRecv" );
  ReserveRecv        = registerStatistic<uint64_t>( "ReserveRecv" );
  RevokeRecv         = registerStatistic<uint64_t>( "RevokeRecv" );
  HaltRecv           = registerStatistic<uint64_t>( "HaltRecv" );
  ResumeRecv         = registerStatistic<uint64_t>( "ResumeRecv" );
  ReadRegRecv        = registerStatistic<uint64_t>( "ReadRegRecv" );
  WriteRegRecv       = registerStatistic<uint64_t>( "WriteRegRecv" );
  SingleStepRecv     = registerStatistic<uint64_t>( "SingleStepRecv" );
  SetFutureRecv      = registerStatistic<uint64_t>( "SetFutureRecv" );
  RevokeFutureRecv   = registerStatistic<uint64_t>( "RevokeFutureRecv" );
  StatusFutureRecv   = registerStatistic<uint64_t>( "StatusFutureRecv" );
  SuccessRecv        = registerStatistic<uint64_t>( "SuccessRecv" );
  FailedRecv         = registerStatistic<uint64_t>( "FailedRecv" );
  BOTWRecv           = registerStatistic<uint64_t>( "BOTWRecv" );
}

void RevCPU::setup() {
  if( EnableNIC ) {
    Nic->setup();
    address = Nic->getAddress();
  }
  if( EnableMemH ) {
    Ctrl->setup();
  }
  if( EnableZopNIC ) {
    zNic->setup();
  }
  if( zoneRing )
    zoneRing->setup();
}

void RevCPU::finish() {}

void RevCPU::init( unsigned int phase ) {
  if( EnableNIC )
    Nic->init( phase );
  if( EnableMemH )
    Ctrl->init( phase );
  if( EnableZopNIC )
    zNic->init( phase );
  if( zoneRing )
    zoneRing->init( phase );
}

void RevCPU::processZOPQ() {
  if( ZIQ.size() == 0 ) {
    return;
  }

  bool     flag = false;
  uint64_t Addr = 0x00ull;

  /// put the ZRqst structure in RevMem

  for( unsigned i = 0; i < ZIQ.size(); i++ ) {
    auto zev = ZIQ[i];
    if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &Addr ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][RZA] Erroneous packet contents for ZOP : "
        "[SrcZCID:SrcPCID:Type:Opc:ID]=[%" PRIu8 ":%" PRIu8 ":%s:%" PRIu8 ":%" PRIu16 "]\n",
        zev->getSrcZCID(),
        zev->getSrcPCID(),
        zNic->msgTToStr( zev->getType() ).c_str(),
        safe_static_cast<uint8_t>( zev->getOpc() ),
        zev->getID()
      );
    }

    //if( ZRqst.find(Addr) == ZRqst.end() ){
    if( !Mem->isZRqst( Addr ) ) {
      // did not find an outstanding memory request with the same address
      // process this request
      switch( zev->getType() ) {
      case Forza::zopMsgT::Z_MZOP:
        // send to the MZOP pipeline
        if( !CoProcs[Forza::Z_MZOP_PIPE_HART]->InjectZOP( zev, flag ) ) {
          output.fatal( CALL_INFO, -1, "[FORZA][RZA] Failed to inject MZOP into pipeline; ID=%" PRIu16 "\n", zev->getID() );
        }
        break;
      case Forza::zopMsgT::Z_HZOPAC:
        // send to the HZOP pipeline
        if( !CoProcs[Forza::Z_HZOP_PIPE_HART]->InjectZOP( zev, flag ) ) {
          output.fatal( CALL_INFO, -1, "[FORZA][RZA] Failed to inject HZOP into pipeline; ID=%" PRIu16 "\n", zev->getID() );
        }
        break;
#if 0
      case Forza::zopMsgT::Z_HZOPV:
        // send to the HZOP pipeline
        output.fatal(
          CALL_INFO,
          -1,
          "[FORZA][RZA] RZA's disable handling of ZOP messages of Type=%s\n",
          zNic->msgTToStr( zev->getType() ).c_str()
        );
        break;
#endif
      default:
        output.fatal(
          CALL_INFO, -1, "[FORZA][RZA] RZA's cannot handle ZOP messages of Type=%s\n", zNic->msgTToStr( zev->getType() ).c_str()
        );
        break;
      }

      // If the request was NOT flagged as a store (mem write),
      // add the request to the outstanding address map
      // When the load hazard is cleared, we also need to clear this value
      if( !flag ) {
        //ZRqst[Addr] = zev;
        Mem->insertZRqst( Addr, zev );
      }

      // eject the request from the ZIQ
      ZIQ.erase( ZIQ.begin() + i );

      // signal completion for this cycle
      return;
    }
  }
}

void RevCPU::MarkLoadCompleteDummy( const MemReq& req ) {
  // Note: this is a placeholder MarkLoadComplete hazard
  // function that is ONLY used for scratchpad loads
  // These loads should return immediately.  If they don't,
  // then the user has specified an erroneous address and YMMV
}

void RevCPU::handleZOPMessageRZA( Forza::zopEvent* zev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][RZA] Injecting ZOP Message into ZIQ\n" );
  if( zev == nullptr ) {
    output.fatal( CALL_INFO, -1, "[FORZA][RZA]: Cannot inject null ZOP packet into ZIQ\n" );
  }

  ZIQ.push_back( zev );
}

void RevCPU::handleZOPMZOP( Forza::zopEvent* zev ) {

  output.fatal( CALL_INFO, -1, "[FORZA][ZAP]: Unexpected MZOP packet - this function only handled Scratchpad requests\n" );
#if 0
  output.verbose( CALL_INFO, 9, 0, "[FORZA][RZA] Handling MZOP\n" );

  if( zev == nullptr ) {
    output.fatal( CALL_INFO, -1, "[FORZA][ZAP]: Cannot handle null MZOP\n" );
  }

  uint64_t addr                                                  = 0x00ull;
  uint64_t data                                                  = 0x00ull;
  bool     isLoad                                                = false;

  // used only for load operations
  std::function<void( const MemReq& )> LocalMarkLoadCompleteFunc = [=]( const MemReq& req ) { this->MarkLoadCompleteDummy( req ); };
  MemReq req{ addr, 0x00, RevRegClass::RegGPR, zev->getSrcHart(), MemOp::MemOpREAD, true, LocalMarkLoadCompleteFunc };

  // retrieve the address
  if( !zev->getFLIT( Forza::Z_FLIT_ADDR, &addr ) ) {
    output.fatal(
      CALL_INFO,
      -1,
      "[FORZA][ZAP]: MZOP packet has no address FLIT: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
  }

  switch( zev->getOpc() ) {
  case Forza::zopOpc::Z_MZOP_SCLB:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<uint8_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLH:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<uint16_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLW:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<uint32_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLD:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<uint64_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLSB:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<int8_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLSH:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<int16_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCLSW:
    Mem->ReadVal( zev->getSrcHart(), addr, reinterpret_cast<int32_t*>( &data ), req, RevFlag::F_SEXT64 );
    isLoad = true;
    break;
  case Forza::zopOpc::Z_MZOP_SCSB:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<uint8_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSH:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<uint16_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSW:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<uint32_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSD:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<uint64_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSSB:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<int8_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSSH:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<int16_t>( data ) );
    break;
  case Forza::zopOpc::Z_MZOP_SCSSW:
    if( !zev->getFLIT( Z_FLIT_DATA, &data ) ) {
      output.fatal(
        CALL_INFO,
        -1,
        "[FORZA][ZAP]: MZOP packet has no data FLIT: Type=%s, ID=%" PRIu16 "\n",
        zNic->msgTToStr( zev->getType() ).c_str(),
        zev->getID()
      );
    }
    Mem->Write( zev->getSrcHart(), addr, static_cast<int32_t>( data ) );
    break;
  default:
    output.fatal(
      CALL_INFO,
      -1,
      "[FORZA][ZAP]: MZOP packet with erroneous opcode: Type=%s, ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
    break;
  }

  if( isLoad ) {
    // create a response packet with the data
    output.verbose( CALL_INFO, 9, 0, "[FORZA][ZAP]: Build LOAD response for MSG @ ID=%" PRIu16 "\n", zev->getID() );
    SST::Forza::zopEvent* rsp_zev = new SST::Forza::zopEvent();

    // set all the fields
    rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
    rsp_zev->setID( zev->getID() );
    rsp_zev->setOpc( SST::Forza::zopOpc::Z_RESP_LR );
    rsp_zev->setAppID( 0 );
    rsp_zev->setDestHart( zev->getSrcHart() );
    rsp_zev->setDestZCID( zev->getSrcZCID() );
    rsp_zev->setDestPCID( zev->getSrcPCID() );
    rsp_zev->setDestPrec( zev->getSrcPrec() );
    rsp_zev->setSrcHart( zev->getDestHart() );
    rsp_zev->setSrcZCID( (uint8_t) ( zNic->getEndpointType() ) );
    rsp_zev->setSrcPCID( (uint8_t) ( zNic->getPCID( zNic->getZoneID() ) ) );
    rsp_zev->setSrcPrec( (uint8_t) ( zNic->getPrecinctID() ) );

    // set the payload
    std::vector<uint64_t> payload;
    payload.push_back( 0x00ull );  // ACS
    payload.push_back( data );     // load response data
    rsp_zev->setPayload( payload );

    // inject the packet
    zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );
  } else {
    // create a response packet with the data
    output.verbose( CALL_INFO, 9, 0, "[FORZA][ZAP]: Build STORE response for MSG @ ID=%" PRIu16 "\n", zev->getID() );
    SST::Forza::zopEvent* rsp_zev = new SST::Forza::zopEvent();

    // set all the fields
    rsp_zev->setType( SST::Forza::zopMsgT::Z_RESP );
    rsp_zev->setID( zev->getID() );
    rsp_zev->setOpc( SST::Forza::zopOpc::Z_RESP_SACK );
    rsp_zev->setAppID( 0 );
    rsp_zev->setDestHart( zev->getSrcHart() );
    rsp_zev->setDestZCID( zev->getSrcZCID() );
    rsp_zev->setDestPCID( zev->getSrcPCID() );
    rsp_zev->setDestPrec( zev->getSrcPrec() );
    rsp_zev->setSrcHart( zev->getDestHart() );
    rsp_zev->setSrcZCID( (uint8_t) ( zNic->getEndpointType() ) );
    rsp_zev->setSrcPCID( (uint8_t) ( zNic->getPCID( zNic->getZoneID() ) ) );
    rsp_zev->setSrcPrec( (uint8_t) ( zNic->getPrecinctID() ) );

    // set the payload
    std::vector<uint64_t> payload;
    payload.push_back( 0x00ull );  // ACS
    rsp_zev->setPayload( payload );

    // inject the packet
    zNic->send( rsp_zev, ( SST::Forza::zopCompID )( zev->getSrcZCID() ) );
  }

  // delete the event
  delete zev;
#endif
}

void RevCPU::handleZOPThreadMigrateIntRegs( Forza::zopEvent* zev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][ZAP] Handling thread migration - INTREGS case\n" );

  const auto& pkt                              = zev->getPacket();
  // The thread-specific
  // data is formatted as follows:
  // pkt[0] = <header info>
  // pkt[1] = <header info>
  // pkt[2] = [63:32] State info [31:0] THREAD PC
  // pkt[3] = x[1] register contents
  // pkt[4] = x[2] register contents
  // pkt[5] = x[3] register contents
  // ...
  // pkt[33] = x[31] register contents
  // pkt[34] = f[0] register contents
  // pkt[35] = f[1] register contents
  // ...
  // pkt[65] = f[31] register contents

  // Create the regfile
  std::unique_ptr<RevRegFile> MigratedRegState = std::make_unique<RevRegFile>( Procs[0].get() );
  uint64_t                    pc               = pkt[2] & ( 0x0FFFFFFFFUL );
  MigratedRegState->SetPC( pc );
  for( unsigned i = 1; i < 32; i++ ) {
    MigratedRegState->SetX( i, pkt[2 + i] );  //x0 == 0
  }

  uint64_t ThreadMemTopAddr       = MigratedRegState->GetX<uint64_t>( RevReg::tp );
  uint64_t ThreadMemBaseAddr      = ThreadMemTopAddr - Mem->GetTLSSize() - _STACK_SIZE_;

  // TODO: Remove ThreadMemSeg upon ThreadCompletion (ie. ThreadState == DONE)
  std::shared_ptr<MemSegment> seg = std::make_shared<MemSegment>( ThreadMemBaseAddr, Mem->GetTLSSize() + _STACK_SIZE_ );
  Mem->GetThreadMemSegs().push_back( seg );

  std::unique_ptr<RevThread> MigratedThread = std::make_unique<RevThread>(
    GetNewThreadID(),  // TODO: Include in Payload
    _INVALID_TID_,     // TODO: Include in payload
    seg,
    std::move( MigratedRegState )
  );

  output.verbose( CALL_INFO, 1, 0, "[FORZA][ZAP] Received thread that starts at address: 0x%" PRIx64 "\n", pc );
  ReadyThreads.push_back( std::move( MigratedThread ) );
}

void RevCPU::handleZOPThreadMigrateSpawn( Forza::zopEvent* zev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][ZAP] Handling thread migration - Spawn case\n" );

  const auto& pkt                              = zev->getPacket();
  // The thread-specific data is formatted as follows:
  // pkt[0] = <header info>
  // pkt[1] = <header info>
  // pkt[2] = [63:32] State info [31:0] THREAD PC
  // pkt[3] = x[31] register contents (5-sept-24, tjd: this is my reading of zap doc...needs some clarification)

  // Create the regfile
  std::unique_ptr<RevRegFile> MigratedRegState = std::make_unique<RevRegFile>( Procs[0].get() );
  uint64_t                    pc               = pkt[2] & ( 0x0FFFFFFFFUL );
  MigratedRegState->SetPC( pc );
  for( unsigned i = 1; i < 31; i++ ) {
    MigratedRegState->SetX( i, 0 );
  }
  MigratedRegState->SetX( 31, pkt[3] );

  uint64_t ThreadMemTopAddr       = MigratedRegState->GetX<uint64_t>( RevReg::tp );
  uint64_t ThreadMemBaseAddr      = ThreadMemTopAddr - Mem->GetTLSSize() - _STACK_SIZE_;

  // TODO: Remove ThreadMemSeg upon ThreadCompletion (ie. ThreadState == DONE)
  std::shared_ptr<MemSegment> seg = std::make_shared<MemSegment>( ThreadMemBaseAddr, Mem->GetTLSSize() + _STACK_SIZE_ );
  Mem->GetThreadMemSegs().push_back( seg );

  std::unique_ptr<RevThread> MigratedThread = std::make_unique<RevThread>(
    GetNewThreadID(),  // TODO: Include in Payload
    _INVALID_TID_,     // TODO: Include in payload
    seg,
    std::move( MigratedRegState )
  );

  output.verbose( CALL_INFO, 1, 0, "[FORZA][ZAP] Received spawn that starts at address: 0x%" PRIx64 "\n", pc );
  ReadyThreads.push_back( std::move( MigratedThread ) );
}

void RevCPU::handleZOPThreadMigrate( Forza::zopEvent* zev ) {
  switch( zev->getOpc() ) {
  case SST::Forza::zopOpc::Z_TMIG_INTREGS: handleZOPThreadMigrateIntRegs( zev ); break;
  case SST::Forza::zopOpc::Z_TMIG_FPREGS:
    output.fatal( CALL_INFO, -1, "[FORZA][ZAP] TMIG_FPREGS not yet handled; message ID=%" PRIu16 "\n", zev->getID() );
    break;
  case SST::Forza::zopOpc::Z_TMIG_SPAWN: handleZOPThreadMigrateSpawn( zev ); break;
  default:
    output.fatal( CALL_INFO, -1, "[FORZA][ZAP] Invalid TMIG opcode; opcode=%" PRIu8 "\n", static_cast<uint8_t>( zev->getOpc() ) );
    break;
  }
}

void RevCPU::handleZOPMessageZAP( Forza::zopEvent* zev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][ZAP] Handling ZOP Message\n" );
  switch( zev->getType() ) {
  case Forza::zopMsgT::Z_RESP:
    if( !Mem->handleRZAResponse( zev ) ) {
      output.fatal( CALL_INFO, -1, "[FORZA][ZAP] Could not handle response for message ID=%" PRIu16 "\n", zev->getID() );
    }
    break;
  case Forza::zopMsgT::Z_EXCP:
    output.fatal(
      CALL_INFO,
      -1,
      "[FORZA][ZAP] Received exception code=%s from message ID=%" PRIu16 "\n",
      zNic->msgTToStr( zev->getType() ).c_str(),
      zev->getID()
    );
    break;
  case Forza::zopMsgT::Z_TMIG: handleZOPThreadMigrate( zev ); break;
  case Forza::zopMsgT::Z_MZOP: handleZOPMZOP( zev ); break;
  default:
    output.fatal(
      CALL_INFO, -1, "[FORZA][ZAP] ZAP's cannot handle ZOP messages of Type=%s\n", zNic->msgTToStr( zev->getType() ).c_str()
    );
    break;
  }
}

void RevCPU::handleZOPMessage( Event* ev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][%s] Received ZOP Message\n", getName().c_str() );
  Forza::zopEvent* zev = static_cast<Forza::zopEvent*>( ev );

  if( zev == nullptr ) {
    output.fatal( CALL_INFO, -1, "[FORZA][handleZOPMessage] : zopEvent is null\n" );
  }
  if( EnableRZA ) {
    // handle a FORZA ZOP Message
    handleZOPMessageRZA( zev );
  } else {
    // I am a ZAP device, handle the message accordingly
    handleZOPMessageZAP( zev );
  }
}

void RevCPU::handleRingMsg( Event* ev ) {
  output.verbose( CALL_INFO, 9, 0, "[FORZA][%s] Received Ring Message\n", getName().c_str() );
  if( ev == nullptr ) {
    output.fatal( CALL_INFO, -1, "[FORZA][handleRingMessage] : event is null\n" );
  }
  auto* ring_ev = static_cast<Forza::ringEvent*>( ev );

  if( ring_ev->getDestComp() != zoneRing->getEndpointType() ) {
    output.verbose(
      CALL_INFO,
      5,
      0,
      "[FORZA][ZAP] %s forwarding ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n",
      getName().c_str(),
      ring_ev->getCSR(),
      (uint8_t) ring_ev->getOp()
    );
    uint64_t next_addr = zoneRing->getNextAddress();
    zoneRing->send( ring_ev, next_addr );
    return;
  }

  if( ring_ev->getSrcComp() == zoneRing->getEndpointType() ) {
    output.fatal(
      CALL_INFO,
      -1,
      "[FORZA][ZAP] %s unexpected ring message; CSR=0x%" PRIx16 "; op=%" PRIu8 "\n",
      getName().c_str(),
      ring_ev->getCSR(),
      (uint8_t) ring_ev->getOp()
    );
  }

  Procs[0]->handleRingReadData( ring_ev );
}

void RevCPU::handleMessage( Event* ev ) {
  nicEvent* event = static_cast<nicEvent*>( ev );
  // -- RevNIC: This is where you can unpack and handle the data payload
  delete event;
}

uint8_t RevCPU::createTag() {
  uint8_t rtn = 0;
  if( PrivTag == 0b11111111 ) {
    rtn     = 0b00000000;
    PrivTag = 0b00000001;
    return 0b00000000;
  } else {
    rtn = PrivTag;
    PrivTag += 1;
  }
  return rtn;
}

void RevCPU::HandleCrackFault( SST::Cycle_t currentCycle ) {
  output.verbose( CALL_INFO, 4, 0, "FAULT: Crack fault injected at cycle: %" PRIu64 "\n", currentCycle );

  // select a random processor core
  unsigned Core = 0;
  if( numCores > 1 ) {
    Core = RevRand( 0, numCores - 1 );
  }

  Procs[Core]->HandleCrackFault( fault_width );
}

void RevCPU::HandleMemFault( SST::Cycle_t currentCycle ) {
  output.verbose( CALL_INFO, 4, 0, "FAULT: Memory fault injected at cycle: %" PRIu64 "\n", currentCycle );
  Mem->HandleMemFault( fault_width );
}

void RevCPU::HandleRegFault( SST::Cycle_t currentCycle ) {
  output.verbose( CALL_INFO, 4, 0, "FAULT: Register fault injected at cycle: %" PRIu64 "\n", currentCycle );

  // select a random processor core
  unsigned Core = 0;
  if( numCores > 1 ) {
    Core = RevRand( 0, numCores - 1 );
  }

  Procs[Core]->HandleRegFault( fault_width );
}

void RevCPU::HandleALUFault( SST::Cycle_t currentCycle ) {
  output.verbose( CALL_INFO, 4, 0, "FAULT: ALU fault injected at cycle: %" PRIu64 "\n", currentCycle );

  // select a random processor core
  unsigned Core = 0;
  if( numCores > 1 ) {
    Core = RevRand( 0, numCores - 1 );
  }

  Procs[Core]->HandleALUFault( fault_width );
}

void RevCPU::HandleFaultInjection( SST::Cycle_t currentCycle ) {
  // build up a vector of faults
  // then randomly determine which one to inject
  std::vector<std::string> myfaults;
  if( EnableCrackFaults ) {
    myfaults.push_back( "crack" );
  }
  if( EnableMemFaults ) {
    myfaults.push_back( "mem" );
  }
  if( EnableRegFaults ) {
    myfaults.push_back( "reg" );
  }
  if( EnableALUFaults ) {
    myfaults.push_back( "alu" );
  }

  if( myfaults.size() == 0 ) {
    output.fatal( CALL_INFO, -1, "Error: no faults enabled; add a fault vector in the 'faults' param\n" );
  }

  unsigned selector = 0;
  if( myfaults.size() != 1 ) {
    selector = RevRand( 0, int( myfaults.size() ) - 1 );
  }

  // handle the selected fault
  if( myfaults[selector] == "crack" ) {
    HandleCrackFault( currentCycle );
  } else if( myfaults[selector] == "mem" ) {
    HandleMemFault( currentCycle );
  } else if( myfaults[selector] == "reg" ) {
    HandleRegFault( currentCycle );
  } else if( myfaults[selector] == "alu" ) {
    HandleALUFault( currentCycle );
  } else {
    output.fatal( CALL_INFO, -1, "Error: erroneous fault selection\n" );
  }
}

void RevCPU::UpdateCoreStatistics( unsigned coreNum ) {
  auto [stats, memStats] = Procs[coreNum]->GetAndClearStats();
  TotalCycles[coreNum]->addData( stats.totalCycles );
  CyclesWithIssue[coreNum]->addData( stats.cyclesBusy );
  FloatsRead[coreNum]->addData( memStats.floatsRead );
  FloatsWritten[coreNum]->addData( memStats.floatsWritten );
  DoublesRead[coreNum]->addData( memStats.doublesRead );
  DoublesWritten[coreNum]->addData( memStats.doublesWritten );
  BytesRead[coreNum]->addData( memStats.bytesRead );
  BytesWritten[coreNum]->addData( memStats.bytesWritten );
  FloatsExec[coreNum]->addData( stats.floatsExec );
  TLBHitsPerCore[coreNum]->addData( memStats.TLBHits );
  TLBMissesPerCore[coreNum]->addData( memStats.TLBMisses );
}

bool RevCPU::clockTick( SST::Cycle_t currentCycle ) {
  bool rtn = true;

  output.verbose( CALL_INFO, 8, 0, "Cycle: %" PRIu64 "\n", currentCycle );

  // Process the ZOPQ
  if( EnableRZA ) {
    processZOPQ();
    for( unsigned i = 0; i < CoProcs.size(); i++ ) {
      CoProcs[i]->ClockTick( currentCycle );
    }
    return false;
  }

  // Execute each enabled core
  for( size_t i = 0; i < Procs.size(); i++ ) {
    // Check if we have more work to assign and places to put it
    UpdateThreadAssignments( i );
    if( Enabled[i] ) {
      if( !Procs[i]->ClockTick( currentCycle ) ) {
        if( EnableCoProc && !CoProcs.empty() ) {
          CoProcs[i]->Teardown();
        }
        UpdateCoreStatistics( i );
        Enabled[i] = false;
        output.verbose( CALL_INFO, 5, 0, "Closing Processor %zu at Cycle: %" PRIu64 "\n", i, currentCycle );
      }
      if( EnableCoProc && !CoProcs[i]->ClockTick( currentCycle ) && !DisableCoprocClock ) {
        output.verbose( CALL_INFO, 5, 0, "Closing Co-Processor %zu at Cycle: %" PRIu64 "\n", i, currentCycle );
      }
    }

    // See if any of the threads on this proc changes state
    HandleThreadStateChangesForProc( i );

    if( Procs[i]->HasNoBusyHarts() ) {
      Enabled[i] = false;
    }
  }

  // check to see if we need to inject a fault
  if( EnableFaults ) {
    if( FaultCntr == 0 ) {
      // inject a fault
      HandleFaultInjection( currentCycle );

      // reset the fault counter
      FaultCntr = fault_width;
    } else {
      FaultCntr--;
    }
  }

  // check to see if all the processors are completed
  for( size_t i = 0; i < Procs.size(); i++ ) {
    if( Enabled[i] ) {
      rtn = false;
    }
  }

  // If all Procs are disabled (ie. rtn == false at this point)
  // check to see if there are threads to assign
  if( ReadyThreads.size() ) {
    rtn = false;
  } else if( BlockedThreads.size() ) {
    CheckBlockedThreads();
    rtn = false;
  }

  // check to see if the network has any outstanding messages: fixme
  if( !TrackTags.empty() || !ZeroRqst.empty() ) {
    rtn = false;
  }

  if( rtn && CompletedThreads.size() ) {
    for( unsigned i = 0; i < numCores; i++ ) {
      UpdateCoreStatistics( i );
      Procs[i]->PrintStatSummary();
    }
    Mem->updatePhysHistorytoOutput();

    for( const auto& [Name, Seg] : Mem->GetDumpRanges() ) {
      // Open a file called '{Name}.init.dump'
      std::ofstream dumpFile( Name + ".dump.final", std::ios::binary );
      Mem->DumpMemSeg( Seg, 16, dumpFile );
    }
    primaryComponentOKToEndSim();
    output.verbose( CALL_INFO, 5, 0, "OK to end sim at cycle: %" PRIu64 "\n", static_cast<uint64_t>( currentCycle ) );
  } else {
    rtn = false;
  }

  return rtn;
}

// Initializes a RevThread object.
// - Moves it to the 'Threads' map
// - Adds it's ThreadID to the ReadyThreads to be scheduled
void RevCPU::InitThread( std::unique_ptr<RevThread>&& ThreadToInit ) {
  // Get a pointer to the register state for this thread
  std::unique_ptr<RevRegFile> RegState = ThreadToInit->TransferVirtRegState();

  // Set the global pointer register and the frame pointer register
  auto gp                              = Loader->GetSymbolAddr( "__global_pointer$" );
  RegState->SetX( RevReg::gp, gp );
  RegState->SetX( RevReg::fp, gp );

  // Set state to Ready
  ThreadToInit->SetState( ThreadState::READY );
  output.verbose( CALL_INFO, 4, 0, "Initializing Thread %" PRIu32 "\n", ThreadToInit->GetID() );
  output.verbose( CALL_INFO, 11, 0, "Thread Information: %s", ThreadToInit->to_string().c_str() );
  ReadyThreads.emplace_back( std::move( ThreadToInit ) );
}

// Assigns a RevThred to a specific Proc which then loads it into a RevHart
// This should not be called without first checking if the Proc has an IdleHart
void RevCPU::AssignThread( std::unique_ptr<RevThread>&& ThreadToAssign, unsigned ProcID ) {
  output.verbose( CALL_INFO, 4, 0, "Assigning Thread %" PRIu32 " to Processor %" PRIu32 "\n", ThreadToAssign->GetID(), ProcID );
  Procs[ProcID]->AssignThread( std::move( ThreadToAssign ) );
  return;
}

// Checks if a thread with a given Thread ID can proceed (used for pthread_join).
// it does this by seeing if a given thread's WaitingOnTID has completed
bool RevCPU::ThreadCanProceed( const std::unique_ptr<RevThread>& Thread ) {
  bool rtn              = false;

  // Get the thread's waiting to join TID
  uint32_t WaitingOnTID = Thread->GetWaitingToJoinTID();

  // If the thread is waiting on another thread, check if that thread has completed
  if( WaitingOnTID != _INVALID_TID_ ) {
    // Check if WaitingOnTID has completed... if so, return = true, else return false
    output.verbose( CALL_INFO, 6, 0, "Thread %" PRIu32 " is waiting on Thread %" PRIu32 "\n", Thread->GetID(), WaitingOnTID );

    // Check if the WaitingOnTID has completed, if not, thread cannot proceed
    rtn = ( CompletedThreads.find( WaitingOnTID ) != CompletedThreads.end() ) ? true : false;
  }
  // If the thread is not waiting on another thread, it can proceed
  else {
    // Thread is waiting on INVALID_TID (ie. No thread)
    // so it can proceed
    rtn = true;
  }

  return rtn;
}

// Checks if any of the blocked threads can proceed based on if the thread
// they were waiting on has completed
void RevCPU::CheckBlockedThreads() {
  // Iterate over all block threads
  for( auto it = BlockedThreads.begin(); it != BlockedThreads.end(); ) {
    if( ThreadCanProceed( *it ) ) {
      ReadyThreads.emplace_back( std::move( *it ) );
      it = BlockedThreads.erase( it );
    } else {
      ++it;
    }
  }
  return;
}

// Checks core 'i' to see if it has any available harts to assign work to
// if it does and there is work to assign (ie. ReadyThreads is not empty)
// assign it and enable the processor if not already enabled.
void RevCPU::UpdateThreadAssignments( uint32_t ProcID ) {
  // Check if we have anything to assign
  if( ReadyThreads.empty() ) {
    return;
  }

  // There is work to assign, check if this proc has room
  if( Procs[ProcID]->HasIdleHart() ) {
    // There is room, assign a thread
    // Get the next thread to assign
    Procs[ProcID]->AssignThread( std::move( ReadyThreads.front() ) );

    // Remove thread from ready threads vector
    ReadyThreads.erase( ReadyThreads.begin() );

    // Proc has a thread assigned to it, enable it
    Enabled[ProcID] = true;
  }
  return;
}

// Checks for state changes in the threads of a given processor index 'i'
// and handle appropriately
void RevCPU::HandleThreadStateChangesForProc( uint32_t ProcID ) {
  // Handle any thread state changes for this core
  // NOTE: At this point we handle EVERY thread that changed state every cycle
  for( auto& Thread : Procs[ProcID]->TransferThreadsThatChangedState() ) {
    uint32_t ThreadID = Thread->GetID();
    // Handle the thread that changed state based on the new state
    switch( Thread->GetState() ) {
    case ThreadState::MIGRATE:
      // This thread has been migrated
      output.verbose( CALL_INFO, 8, 0, "Thread %" PRIu32 " on Core %" PRIu32 " is MIGRATED\n", ThreadID, ProcID );
      CompletedThreads.emplace( ThreadID, std::move( Thread ) );
      break;

    case ThreadState::DONE:
      // This thread has completed execution
      output.verbose( CALL_INFO, 8, 0, "Thread %" PRIu32 " on Core %" PRIu32 " is DONE\n", ThreadID, ProcID );
      CompletedThreads.emplace( ThreadID, std::move( Thread ) );
      break;

    case ThreadState::BLOCKED:
      // This thread is blocked (currently only caused by a rev_pthread_join)
      output.verbose( CALL_INFO, 8, 0, "Thread %" PRIu32 "on Core %" PRIu32 " is BLOCKED\n", ThreadID, ProcID );

      // Set its state to BLOCKED
      Thread->SetState( ThreadState::BLOCKED );

      // Add it to BlockedThreads
      BlockedThreads.emplace_back( std::move( Thread ) );
      break;
    case ThreadState::START:
      // A new thread was created
      output.verbose( CALL_INFO, 99, 1, "A new thread with ID = %" PRIu32 " was found on Core %" PRIu32, Thread->GetID(), ProcID );

      // Mark it ready for execution
      Thread->SetState( ThreadState::READY );

      // Add it to the ReadyThreads so it is scheduled
      ReadyThreads.emplace_back( std::move( Thread ) );

      break;

    case ThreadState::RUNNING:
      output.verbose( CALL_INFO, 11, 0, "Thread %" PRIu32 " on Core %" PRIu32 " is RUNNING\n", ThreadID, ProcID );
      break;

    case ThreadState::READY:
      // If this happens we are not setting state when assigning thread somewhere
      output.fatal(
        CALL_INFO,
        99,
        "Error: Thread %" PRIu32 " on Core %" PRIu32 " is assigned but is in READY state... This is a bug\n",
        ThreadID,
        ProcID
      );
      break;
    default:  // Should DEFINITELY never happen
      output.fatal(
        CALL_INFO,
        99,
        "Error: Thread %" PRIu32 " on Core %" PRIu32 " is in an unknown state... This is a bug.\n%s\n",
        ThreadID,
        ProcID,
        Thread->to_string().c_str()
      );
      break;
    }
    // If the Proc does not have any threads assigned to it, there is no work to do, it is disabled
    if( Procs[ProcID]->HasNoWork() ) {
      Enabled[ProcID] = false;
    }
  }
}

void RevCPU::InitMainThread( uint32_t MainThreadID, const uint64_t StartAddr ) {
  auto MainThreadRegState = std::make_unique<RevRegFile>( Procs[0].get() );

  // The Program Counter gets set to the start address
  MainThreadRegState->SetPC( StartAddr );

  // We need to initialize the a0 register to the value of ARGC.
  // This is >= 1 (the executable name is always included).
  // We also need to initialize the a1 register to the ARGV pointer
  // of the ARGV base pointer in memory which is currently set to
  // the top of stack.
  uint64_t stackTop = Mem->GetStackTop();
  MainThreadRegState->SetX( RevReg::a0, Opts->GetArgv().size() + 1 );
  MainThreadRegState->SetX( RevReg::a1, stackTop );

  // We subtract Mem->GetTLSSize() bytes from the current stack top, and
  // round down to a multiple of 16 bytes. We set it as the new top of stack.
  stackTop = ( stackTop - Mem->GetTLSSize() ) & ~uint64_t{ 15 };
  Mem->SetStackTop( stackTop );

  // We set the stack pointer and the thread pointer to the new stack top
  // The thread local storage is accessed with a nonnegative offset from tp,
  // and the stack grows down with sp being subtracted from before storing.
  MainThreadRegState->SetX( RevReg::sp, stackTop );
  MainThreadRegState->SetX( RevReg::tp, stackTop );

  auto gp = Loader->GetSymbolAddr( "__global_pointer$" );
  MainThreadRegState->SetX( RevReg::gp, gp );
  MainThreadRegState->SetX( RevReg::fp, gp );

  std::unique_ptr<RevThread> MainThread = std::make_unique<RevThread>(
    MainThreadID,
    _INVALID_TID_,  // No Parent Thread ID
    Mem->GetThreadMemSegs().front(),
    std::move( MainThreadRegState )
  );
  MainThread->SetState( ThreadState::READY );

  output.verbose( CALL_INFO, 11, 0, "Main thread initialized %s\n", MainThread->to_string().c_str() );

  // Add to ReadyThreads so it gets scheduled
  ReadyThreads.emplace_back( std::move( MainThread ) );
}

}  // namespace SST::RevCPU

// EOF
