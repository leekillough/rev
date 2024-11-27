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

# Build and Run Vector tests

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

    # Test Directory structure

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

The suffix (e.g. .64.32.cc) is used to pass VLEN and ELEN into the simulators. These are required at configuration time and remain fixed for the entire simulation.

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

# Current Status and Plans (Updated 11/26/2024)

## Vector ISA Support
- Initial Vector Register File (including Vector CSRs), Instruction Decoder, and Instruction Function Table in place
- 80 Instructions (out of a lot!) coded with vector mask support. Many tested.
- TODO [H] touch remaining categories of instructions so we understand all special cases.
- TODO [H] non-zero starting elements, AVLs that are non multiple of vlen/sew
- TODO [L] tail/mask agnostioc/unagnostic 
- TODO [L] memory alignment and consistency checks

## Timing
- Basic infrasture for the  CPU core to issue instructions to the coprocessor instruction queue is in place. However, instruction execution occurs in 0 cycles with the hand of the almighty reaching back into the core register file.
- TODO [M] Capture instruction input scalars with instruction in queue and remove pointer to core register file in execution functions.
- TODO [M] MemHierarchy Support
    - Provide vector register scoreboard (whole register) and completion function to clear scoreboard
    - Check for register dependency before executing vector instruction
- TODO [M] Instruction Latency and Cost Model
    - Need to develop a simple instruction cost model that will create back pressure on the instruction queue and stall the core.
    - Would like a simple timing model initially but should consider a more accurate pipeline model that includes chaining support.

## Design Encapsulation
- Created independent Vector Register file in RevVRegFile.h and Instruction table support in RVVec.h.
- Vector CSRs reside in the Vector Register file. Core CSR reads are handled has Coprocessor instructions.
- TODO [L] Eventually, we want to refactor and relocate all vector code outside of the REV Source tree and provide an independent build flow.

## Current Test Summary
### CSR access
    Core reads of coprocessor CSRs and set*vl* funtionality.
    csr.128.64
    csr.64.32
### Vector Mask
    Vector Mask usage across full VLEN
    mask1_e8_m8.128.64
### Vector Register File access
    Basic instruction permuted across SEW, LMUL, VLEN, ELEN
    vadd.vi_e8_m1.128.64
    vadd.vx_e8_m1.128.64
    vadd_e16_m1.128.64
    vadd_e16_m1.32.16
    vadd_e16_m1.64.32
    vadd_e16_m4.32.16
    vadd_e16_m8.128.64
    vadd_e16_m8.32.16
    vadd_e16_mf4.128.64
    vadd_e32_m1.128.64
    vadd_e32_m2.128.64
    vadd_e32_m4.128.64
    vadd_e32_m8.128.64
    vadd_e32_m8_unaligned.128.64
    vadd_e32_mf2.128.64
    vadd_e64.m1.128.64
    vadd_e8_m1.128.64
    vadd_e8_mf2.128.64
    vadd_e8_mf4.128.64
    vadd_e8_mf8.128.64
### Benchmarks
    Ported from RV Unpriveleged ISA Appendix C and https://github.com/riscv-software-src/riscv-tests
    vec-cond.128.64
    vec-daxpy.128.64
    vec-mixed.128.64
    vec-sgemm.128.64
    vec-strcmp.128.64
## Load / Store Tests
    Unit-stride, Mask Load, Strided, and Indexed
    vmem1_e16_m8.128.64
    vmem1_e8_m8.128.64
    vmem_indexed_e16_m8.128.64
    vmem_strided_e16_m8.128.64

TODO more representative code

https://github.com/flame/blis/tree/master/kernels/rviv/3
https://raw.githubusercontent.com/flame/blis/master/kernels/rviv/3/bli_sdgemm_rviv_asm_4vx4.h

# Instruction Support

This sections lists the instructions that have implementations (and limited testing) and which ones do not.

## Configuration-Setting Instructions
    vsetvli %rd, %rs1, %zimm11
    vsetivli %rd, %zimm, %zimm10
    vsetvl %rd, %rs1, %rs2
## Vector Loads and Stores
### Vector Unit-Stride Instructions
    vle8.v %vd, (%rs1), %vm
    vle32.v %vd, (%rs1), %vm
    vle64.v %vd, (%rs1), %vm
    vse8.v %vs3, (%rs1), %vm
    vse16.v %vs3, (%rs1), %vm
    vse32.v %vs3, (%rs1), %vm
    vse64.v %vs3, (%rs1), %vm
### Vector Unit-Stride Mask Load/Start
    vlm.v %vd, (%rs1)
    vsm.v %vs3, (%rs1) 
### Vector Strided Instructions
    vlse8.v %vd, (%rs1), %rs2, %vm
    vlse16.v %vd, (%rs1), %rs2, %vm
    vlse32.v %vd, (%rs1), %rs2, %vm
    vlse64.v %vd, (%rs1), %rs2, %vm
    vsse8.v %vs3, (%rs1), %rs2, %vm
    vsse16.v %vs3, (%rs1), %rs2, %vm
    vsse32.v %vs3, (%rs1), %rs2, %vm
    vsse64.v %vs3, (%rs1), %rs2, %vm
### Vector Indexed Instructions
    note: Currently no difference between ordered and unordered versions
    vluxei8.v vd, (rs1), vs2, vm
    vluxei16.v vd, (rs1), vs2, vm
    vluxei32.v vd, (rs1), vs2, vm
    vluxei64.v vd, (rs1), vs2, vm
    vloxei8.v vd, (rs1), vs2, vm
    vloxei16.v vd, (rs1), vs2, vm
    vloxei32.v vd, (rs1), vs2, vm
    vloxei64.v vd, (rs1), vs2, vm
    vsuxei8.v vs3, (rs1), vs2, vm
    vsuxei16.v vs3, (rs1), vs2, vm
    vsuxei32.v vs3, (rs1), vs2, vm
    vsuxei64.v vs3, (rs1), vs2, vm
    vsoxei8.v vs3, (rs1), vs2, vm
    vsoxei16.v vs3, (rs1), vs2, vm
    vsoxei32.v vs3, (rs1), vs2, vm
    vsoxei64.v vs3, (rs1), vs2, vm
### Unit Stride Load Fault-only first
    vle8ff.v %vd, (%rs1), %vm
    vle16ff.v %vd, (%rs1), %vm
    vle32ff.v %vd, (%rs1), %vm
    vle64ff.v %vd, (%rs1), %vm
### Vector Load/Store Segment Instructions
    [H] TODO
### Vector Load/Store Whole Register Instructions
    [H] TODO
## Vector Arithemetic Instructions 
### Vector Single-Width Integer Add and Subtract
    vadd.vv %vd, %vs2, %vs1, %vm
    vadd.vx %vd, %vs2, %rs1, %vm
    vadd.vi %vd, %vs2, %zimm5, %vm
    vsub.vv %vd, %vs2, %vs1, %vm
    vsub.vx %vd, %vs2, %rs1, %vm
    vrsub.vx %vd, %vs2, %vs1, %vm
    vrsub.vi %vd, %vs2, %rs1, %vm
### Vector Widening Integer Add/Subtract
    TODO
### Vector Integer Extension
    TODO
### Vector Integer Add-with-Carry / Subtract-with-Borrow Instructions
    TODO
### Vector Bitwise logical operations
    vand.vv %vd, %vs2, %vs1, %vm
    vand.vx %vd, %vs2, %rs1, %vm
    vand.vi %vd, %vs2, %zimm5, %vm
    vor.vv %vd, %vs2, %vs1, %vm
    vor.vx %vd, %vs2, %rs1, %vm
    vor.vi %vd, %vs2, %zimm5, %vm
    vxor.vv %vd, %vs2, %vs1, %vm
    vxor.vx %vd, %vs2, %rs1, %vm
    vxor.vi %vd, %vs2, %zimm5, %vm
### Vector Single-Width Shift Instructions
    TODO
### Vector Narrowing Integer Right Shift Instructions
    TODO
### Vector Integer Compare Instructions
    vmseq.vv %vd, %vs2, %vs1, %vm
    vmseq.vx %vd, %vs2, %rs1, %vm
    vmseq.vi %vd, %vs2, %zimm5, %vm
    vmsne.vv %vd, %vs2, %vs1, %vm
    vmsne.vx %vd, %vs2, %rs1, %vm
    vmsne.vi %vd, %vs2, %zimm5, %vm
    vmsle.vv %vd, %vs2, %vs1, %vm
    vmsle.vx %vd, %vs2, %rs1, %vm
    vmsle.vi %vd, %vs2, %zimm5, %vm
    (TODO rest of them)
### Vector Integer Min/Max Instructions
    TODO
### Vector Single-Width Integer Multiply Instructions
    TODO
### Vector Integer Divide Instructions
    TODO
### Vector Widening Integer Multiply Instructions
    TODO
### Vector Single-Width Integer Multiply-Add Instructions
    TODO
### Vector Widening Integer Multiply-Add Instructions
    TODO
### Vector Integer Merge Instructions
    TODO
### Vector Integer Move Instructions
    vmv.v.v %vd, %vs1
    vmv.v.x %vd, %rs1
    vmv.v.i %vd, zimm5
## Vector Fixed-Point Arithmetic Instructions
    TODO
### Vector Single-Width Saturating Add and Subtract
    TODO
### Vector Single-Width Averaging Add and Subtract
    TODO
### Vector Single-Width Fractional Multiply with Rounding and Saturation
    TODO
### Vector Single-Width Scaling Shift Instructions
    TODO
### Vector Narrowing Fixed-Point Clip Instructions
    TODO
## Vector Floating-Point Instructions
    TODO
### Vector Single-Width Floating-Point Add/Subtract Instructions
    TODO
### Vector Widening Floating-Point Add/Subtract Instructions
    TODO
### Vector Single-Width Floating-Point Multiply/Divide Instructions
    TODO
### Vector Widening Floating-Point Multiply
    TODO
### Vector Single-Width Floating-Point Fused Multiply-Add Instructions
    vfmacc.vf %vd, %rs1, %vs2, %vm
    (TODO the rest of them)
### Vector Widening Floating-Point Fused Multiply-Add Instructions
    TODO
### Vector Floating-Point Square-Root Instruction
    TODO
### Vector Floating-Point Reciprocal Square-Root Estimate Instruction
    TODO
### Vector Floating-Point Reciprocal Estimate Instruction
    TODO
### Vector Floating-Point MIN/MAX Instructions
    TODO
### Vector Floating-Point Sign-Injection Instructions
    TODO
### Vector Floating-Point Compare Instructions
    TODO
### Vector Floating-Point Classify Instruction
    TODO
### Vector Floating-Point Merge Instruction
    TODO
### Vector Floating-Point Move Instruction
    TODO
### Single-Width Floating-Point/Integer Type-Convert Instructions
    TODO
### Widening Floating-Point/Integer Type-Convert Instructions
    TODO
### Narrowing Floating-Point/Integer Type-Convert Instructions
    TODO
## Vector Reduction Operations
    [H] TODO
### Vector Single-Width Integer Reduction Instructions
    TODO
### Vector Widening Integer Reduction Instructions
    TODO
### Vector Single-Width Floating-Point Reduction Instructions
    TODO
### Vector Widening Floating-Point Reduction Instructions
    TODO
## Vector Mask Instructions
    TODO
### Vector Mask-Register Logical Instructions
    vmand.mm %vd, %vs2, %vs1
    vmnand.mm %vd, %vs2, %vs1
    vmandn.mm %vd, %vs2, %vs1
    vmxor.mm %vd, %vs2, %vs1
    vmor.mm %vd, %vs2, %vs1
    vmnor.mm %vd, %vs2, %vs1
    vmorn.mm %vd, %vs2, %vs1
    vmxnor.mm %vd, %vs2, %vs1
### Vector count population in mask vcpop.m
    TODO
### first find-first-set mask bit
    vfirst.m %rd, %vs2, %vm
### vmsbf.m set-before-first mask bit
    TODO
### vmsif.m set-including-first mask bit
    TODO
### vmsof.m set-only-first mask bit
    TODO
### Vector Iota Instruction
    TODO
### Vector Element Index Instruction
    TODO
## Vector Permutation Instructions
    TODO
### Integer Scalar Move Instructions
    TODO
### Floating-Point Scalar Move Instructions
    TODO
### Vector Slideup Instructions
    TODO
### Vector Slidedown Instructions
    TODO
### Vector Slide1up
    TODO
### Vector Floating-Point Slide1up Instruction
    TODO
### Vector Slide1down Instruction
    TODO
### Vector Floating-Point Slide1down Instruction
    TODO
### Vector Register Gather Instructions
    TODO
### Vector Compress Instruction
    TODO
### Whole Vector Register Move 
    TODO

# Links
( Add link to requirements doc )
