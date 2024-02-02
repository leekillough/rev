# ZIP testing

## Architectural design test plan

| TestID | Test name | Test type | Test criteria for success |
| --- | --- | --- | --- |
| **ZIP_P1** | Packet stall: percentage | Performance | Packet stalls due to lack of credits |
| **ZIP_P2** | Dequeue: single sender | Performance | Packets are dequeued from ZIP within $N$ cycles of being enqueued without aggregation |
| **ZIP_P3** | Dequeue: two senders | Performance | Packets are dequeued from ZIP within $2N$ cycles of being enqueued without aggregation |
| **ZIP_P4** | Dequeue: eight senders | Performance | Packets are dequeued from ZIP within $8N$ cycles of being enqueued without aggregation |
| **ZIP_C1** | Packet stall on send | Correctness | Packets are stalled when credits are not available |
| **ZIP_C2** | Packet stall on receive | Correctness | Packets are stalled when credits are not available |
| **ZIP_C3** | Load/store to memory | Correctness | Write to slice of memory and read from owned slice |
| **ZIP_C4** | Aggregation to specification | Correctness | Packets are aggregated according to selected parameters |
| **ZIP_C5** | Disaggregation to specification | Correctness | Packets are disaggregated according to selected parameters |

## Guide to running tests

These tests require the `forza`, `forzazen`, and `forzaZIP` SST element libraries. The SST driver file `zip-test.py` can be used to generate
ZIP test output using different command-line arguments, which are explained in greater detail below.

### `sst --stop-at=40us zip-test.py -- --test=0 --precincts=4 --zones=4 --num_zops=64`
This command sets up 4 precincts that each contain 4 zones. Every zone contains a ZEN and a dummy ZOP generator. The zones in precincts 1–4
will each generate 64 ZOPs and send them via their respective ZIPs to the corresponding zones in precinct 0. The scenario is designed to test
the credit system, ensuring that aggregated packets are not sent until the destination ZIPs have available buffer space and also ensuring that
ZIPs do not send disaggregated ZOPs to zones until the ZENs can receive them.

The output of this command should show that `zip_1`, `zip_2`, and `zip_3` pass test **ZIP_C1** because aggregated packets stall once receiving ZIPs
lack credits. `zip_0` passes test **ZIP_C2** because the disaggregated ZOPs stall while waiting for the receiving ZENs to process ZOPs. `zip_1`,
`zip_2`, and `zip_3` pass tests **ZIP_C3** and **ZIP_C4** because ZOPs from the connected zones are successfully stored in ZIP memory and retreived
and then aggregated according to specifications (once the buffer is full or a maximum wait time of $10\mu\mathrm s$ has been reached). And `zip_0`
passes test **ZIP_C5** because it is able to disaggregate the packet into individual concatenated ZOPs. Also note that test **ZIP_P1** shows the
percentage of packets the were stalled before they could be sent by `zip_1`, `zip_2`, and `zip_3`. (This value, 67%, was made artificially high
due to a low allocation of credits for testing.)

### `sst --stop-at=15us zip-test.py -- --test=1 --precincts=2 --zones=1 --num_zops=1 --max_wait=0`
### `sst --stop-at=15us zip-test.py -- --test=1 --precincts=3 --zones=2 --num_zops=1 --max_wait=0`
### `sst --stop-at=15us zip-test.py -- --test=1 --precincts=9 --zones=8 --num_zops=1 --max_wait=0`
These commands set up 2, 3, or 9 precincts that each contain 1, 2, or 8 zones, respectively. Every zone contains a ZEN and a dummy ZOP generator.
The zones in precinct 0 will each generate 1 ZOP and send it to one of the other precincts. The maximum wait time is set to $0\mu\mathrm s$ to
ensure that the ZIP immediately sends out the ZOP rather than waits for aggregation. This scenario is designed to test the ability of the ZIP
in precinct 0 to quickly send packets from different sources to different destinations.

The outputs of these commands should show that `zip_0` is able to dequeue single packets from one, two, or eight senders within 102 to 106
clock cycles per packet according to tests **ZIP_P2**, **ZIP_P3**, and **ZIP_P4**. This cycle count includes the overhead of writing to and reading from memory
and shows that the peformance of the ZIP is able to scale with the number of senders.

### Rendezvous messaging
The ZIP includes functionality to employ rendezvous messaging when aggregated packet sizes are above a given threshold.
Rather than sending the entire packet once there are enough credits for the receiving ZIP, the sending ZIP will first
send a request and wait for a response to signal that there is enough space in the receiving ZIP's incoming rendezvous
buffer. Then packets are sent in MTU-sized chunks.

To test rendezvous messaging, simply add the `--rendezvous` flag to any of the above commands, which will force all ZOPs
to be sent with this method (by setting the threshold to 0 bytes). The tests should run similarly as before with some
differences. **ZIP_P1** and **ZIP_C1** now ensure that the ZIP waits for an acknowledgement rather than for credits. As
a result, **ZIP_P1** shows that 100% of packets are stalled because getting that response always takes nonzero cycles.
**ZIP_P2**, **ZIP_P3**, and **ZIP_P4** have also increased up to 4004 to 4006 clock cycles per packet.
