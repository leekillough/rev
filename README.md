# REV RVV Coprocessor
       ___  ___ _   _____ 
      / _ \|_  | | / /_  |
     / , _/ __/| |/ / __/ 
    /_/|_/____/|___/____/ 
                      
# Clone and Build

    git clone git@github.com:tactcomplabs/r2v2.git
    git checkout devel
    cd build
    cmake -DRVCC=riscv64-unknown-elf-gcc -DREV_USE_SPIKE=1 ../
    make -j -s && make install
    ctest -j 10

If spike is not available an internal, less useful, instruction trace format will be used and you will be unable to run vector tests on spike.

# Current Status

- Create and run tests on spike and rev
- rev only works for single test ( vadd.cc )

# Plans

- Cool splash screen
- Vector Register File extended from RevCSR (ala RevRegFile)
- Integration Vector Instruction Decode from Dave's generated file
- Latch scalar register sources for vector instructions when instruction is issues.
- Set finite vector instruction queue and stall hart when full.
- Enable memhierarchy following scoreboard support on vector register file
- Implement vector CSR's. As long as we keep restriction of 1 Vector coprocessor per hart these can live in REV and be accessible by coprocessor.
- Evaluate vector register file for flexible VLEN, ELEN and predicate support.
- Advanced items like chaining, results forwarding, etc.. TBD

# Basic Vector Tests

## Build and Run Vector tests

    cd test/vector_basic
    
    # compile and run all tests on spike
    export USE_SPIKE=1
    make clean; make
    
    # compile and run all tests on REV
    unset USE_SPIKE
    make clean; make

    # To run a single test
    make TLIST=vadd

    # Compile only
    make compile

    # View printf output
    make printlog

## Vector Test Directory Structure

    test/vector_basic
    ├── Makefile
    ├── output
    │   ├── bin
    │   │   ├── vadd_e16_m1.64.32.dis
    │   │   ├── vadd_e16_m1.64.32.exe
    │   │   ├── vadd_e16_m1.64.32.sections
    │   │   ├── vadd_e32_m1.128.64.dis
    │   │   ├── vadd_e32_m1.128.64.exe
    │   │   ├── vadd_e32_m1.128.64.sections
    |   |
    │   ├── vadd_e16_m1.64.32
    │   │   ├── run.log
    │   │   ├── run.printlog
    │   │   └── run.status
    │   └── vadd_e32_m1.128.64
    │       ├── run.log
    │       ├── run.printlog
    │       └── run.status
    └── src
        ├── README.md
        ├── rev.h
        ├── vadd_e16_m1.64.32.cc
        └── vadd_e32_m1.128.64.cc

## Creating Vector Tests

All files with a .cc suffix under test/vector_basic/src will be in the list of tests to run. Simple copy/paste/modify an existing test to create a new test.

The suffix (e.g. .64.32.cc) is used to pass VLEN and ELEN into the simulator. These are required at configuration time and remain fixed for the entire simulation.

## Vector Test Writing Tips

- rev.h includes macros for handling printf for REV and Spike and miscellanous assembly level macros to suppor tracing and counter CSR access.

        #include "rev.h"

- Make tests self-checking and provide informative print statements to indicate failure. Use assert statements to exit tests and assist in locating point of failure in the trace files.

        if( expected != result[i] ) {
        printf( "Error: 0x%x != 0x%x\n", expected, result[i] );
        }
        assert( expected == result[i] );

- Use the counter macros to measure and print performance

        RDTIME( time0 );
        RDINSTRET( inst0 )

- Add limits to loops using new, untested functionality, to exit tests early and cleanly.

# Links
( Add link to requirements doc )
