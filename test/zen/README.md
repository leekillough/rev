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
(different zone). **ZEN_C5** passes because `zap0_0` receives a `NACK` when the destination HART is out of memory and the message stalls. In terms of performance,
**ZEN_P1** and **ZEN_P2** show that the ZEN achieves bandwidth of 111 Mb/s and 95 Mb/s when sending payloads in scratchpad
between HARTS within and across zones, respectively (compare to the configured network link bandwidth of 100 Gb/s). **ZEN_P3** shows that
the ZEN can achieve between 135 and 158 KIOps (thousands of input/output operations per second). **ZEN_P4** shows that sending a payload in scratchpad between
HARTs within one zone takes about 12700 ZAP clock cycles.

### Future tests
The opcode `Z_MSG_SENDAS` for sending with address and size (rather than with payload) is not yet implemented, so we cannot currently perform tests
**ZEN_P5**, **ZEN_C2**, or **ZEN_C4**.
