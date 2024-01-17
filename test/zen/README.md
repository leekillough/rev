# ZEN testing

## Architectural design test plan

| TestID | Test name | Test type | Test criteria for success |
| --- | --- | --- | --- |
| **ZEN_P1** | HART to HART within a zone: bandwidth | Performance | Bandwidth matches 80% of connected network link bandwidth
| **ZEN_P2** | HART to HART across zones: bandwidth | Performance | Bandwidth matches 80% of connected network link bandwidth
| **ZEN_P3** | Peak message rate: IOPS | Performance | Bandwidth matches 80% of connected network link bandwidth
| **ZEN_P4** | Send with payload in scratchpad: latency | Performance | Latency does not exceed $N$ cycles
| **ZEN_P5** | Send with payload in memory: latency | Performance | Latency does not exceed $N$ cycles
| **ZEN_C1** | HART to HART within a zone | Correctness | Message pulled from ZAP scratchpad matches at destination
| **ZEN_C2** | HART to HART within a zone | Correctness | Message pulled from main memory matches at destination
| **ZEN_C3** | HART to HART across zones | Correctness | Message pulled from ZAP scratchpad matches at destination
| **ZEN_C4** | HART to HART across zones | Correctness | Message pulled from main memory matches at destination
| **ZEN_C5** | Incast: many HARTs target same HART | Correctness | Senders stall and no message is lost

## Guide to running tests

These tests require the `revcpu`, `forza`, and `forzazen` SST element libraries. The SST driver file `zen-test-rza.py` can be used to generate ZEN test output.
This file uses the executable `ex2.exe`, which can be found in `../../../test/ex2` and compiled with `RVCC=riscv64-unknown-elf-g++ make all` (assuming that the
RISCV compiler `riscv64-unknown-elf-g++` can be found in the path).

### `REV_EXE=../../../test/ex2/ex2.exe sst --stop-at=1ms zen-test-rza.py`
This command sets up 1 precinct with 2 zones, each of which contains a ZEN, 2 ZAPs (which are really just ZOP generators), and an RZA. This scenario is designed
to test HART-to-HART communication by sending payloads between ZAPs in the same zone and in different zones.

The output should show that tests **ZEN_C1** and **ZEN_C3** pass because `zap0_0` is able to successfully send a payload to `zap0_1` (same zone) and `zap1_1`
(different zone).
