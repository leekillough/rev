For the ZQM to function properly for an application, a ZQM Setup Packet is required to configure some state information.  Once the setup packet has been processed, the ZQM can be used in normal operation.

## ZQM Setup Packet
The ZQM::configMTApp function contains an example of how to construct a ZQM Setup packet for a migrating thread application, an actor-based application is similar.

The source and dest information of the packet isn't particularly relevant (it's assumed the zopNIC delivers it properly).  However, the application ID (appID) is extremely relevant and will be used by the ZQM logic to find the state information for the application.

The payload of the setup packet is defined in the ZOP Packet Spec and here:
| Payload word | Contents |
| --- | --- |
| 0 | Lowest ZAP HART for this application |
| 1 | Highest ZAP HART for this application (inclusive) |
| 2 | Memory buffer low address (cannot be 0) |
| 3 | Memory buffer high address (cannot be 0) |
| 4 | Fill HARTs sequentially flag (non-zero/true for actor programs, 0 else) |

For an actor program, the memory buffer addresses can be the same (e.g, no buffer).  Note that the memory buffer address should be an even multiple of the thread length (currently assumed to be 34, 8 byte words) - 1.  Change ZqmAidStateTableRow::ThreadLengthDblWords if the thread is a differnt size.

As an example, if we have an actor program that will use 16 HARTs per ZAP, the payload could be something like {16, 31, 0x1000, 0x1000, 1}.  A migrating thread program using 8 HARTs per ZAP and a storage buffer of 100 threads could have a payload along the lines of {0, 7, 0x1000, 0x1000+((100*34*8)-1), 0}.

## Actor Programs
Originally, it was anticipated that spawned actor threads would carry their destination HART information with them, hence the Z_TMIG_FIXED opcode for these threads (Z_TMIG_SELECT has/is expected to be used for migrating threads).  More recently, it was decided that the ZQM state will maintain a flag saying whether to load threads into HARTs sequentially or not.  If the flag is set, the Z_TMIG_ opcode is essentially ignored; for simplicity, any thread going to the ZQM should just use the Z_TMIG_SELECT opcode.

