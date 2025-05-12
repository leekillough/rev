import os
import sst

if len(sys.argv) < 3:
  print("ERROR : Simulation model requires --model-options=\"INT STR (STR)\"" )
  print(sys.argv)
  sst.exit()

MEMCHUNK_SIZE = 512*1024*1024
MEMSIZE = MEMCHUNK_SIZE * 16
CPUS = int(sys.argv[1])
# CPUS = 1
CPUTYPE = sys.argv[2] # "rev" "standardCPU"
if CPUTYPE == "rev":
    if len(sys.argv) < 4:
        print("ERROR : Simulation model requires --model-options=\"INT STR (STR)\" (second string is the .exe file if the first string is rev)" )
        print(sys.argv)
        sst.exit()
    EXE = sys.argv[3] # 'program1.exe, program2.exe'

revlsqparams = {
      "verbose"         : "0",
      "clock"           : "2.0Ghz",
      "max_loads"       : 64,
      "max_stores"      : 64,
      "max_flush"       : 64,
      "max_llsc"        : 64,
      "max_readlock"    : 64,
      "max_writeunlock" : 64,
      "max_custom"      : 64,
      "ops_per_cycle"   : 64
}

l1cacheparams = {
    "access_latency_cycles" : "4",
    "cache_frequency" : "2 Ghz",
    "replacement_policy" : "lru",
    "coherence_protocal" : "MSI",
    "associativity" : "4",
    "cache_line_size" : "64",
    "L1" : "1",
    "cache_size" : "2KiB"
}

memctrlparams = {
    "clock" : "2GHz",
    "addr_range_start" : 0,
    "addr_range_end" : MEMSIZE-1,
    "backing" : "malloc"
}

memoryparams = {
    "access_time" : "100ns",
    "mem_size" : "8GB"
}

busparams = {
      "bus_frequency" : "2 Ghz",
}

bus = sst.Component("bus", "memHierarchy.Bus")
bus.addParams(busparams)

l1cache = sst.Component("l1cache", "memHierarchy.Cache")
l1cache.addParams(l1cacheparams)

memctrl = sst.Component("memory", "memHierarchy.MemController")
memctrl.addParams(memctrlparams)

memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
memory.addParams(memoryparams)

link_bus_l1cache = sst.Link("link_bus_l1cache")
link_bus_l1cache.connect( (bus, "low_network_0", "1ns"),(l1cache, "high_network_0", "1ns") )

link_l1cache_mem = sst.Link("link_l1cache_mem")
link_l1cache_mem.connect( (l1cache, "low_network_0", "1ns"), (memctrl, "direct_link", "1ns") )

for i in range(CPUS):
    if CPUTYPE == "rev":
        # rev
        revcpuparams = {
            "verbose"  : "1",
            "numCores" : 1,                               # Number of cores
            "clock" : "2.0GHz",                           # Clock
            "memSize" : (i+1) * MEMCHUNK_SIZE,                         # Memory size in bytes
            "machine" : "[CORES:RV64G]",                      # Core:Config; RV32I for core 0
            "startAddr" : "[0:0x00000000]",               # Starting address for core 0
            "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
            "program" : os.getenv("REV_EXE", EXE),  # Target executable
            "enable_memH" : 1,                            # Enable memHierarchy support
            "splash" : 1                                  # Display the splash message
        }

        cpu = sst.Component("cpu" + str(i), "revcpu.RevCPU")
        cpu.addParams(revcpuparams)
        lsq = cpu.setSubComponent("memory", "revcpu.RevBasicMemCtrl");
        lsq.addParams(revlsqparams)
        iface = lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
        link = sst.Link("link" + str(i))
        link.connect( (iface, "port", "1ns"),(bus, "high_network_" + str(i), "1ns") )
    elif CPUTYPE == "standardCPU":
        # standardCPU
        standardcpuparams = {
            "verbose" : 1,
            "memFreq" : 100,
            "memSize" : "512MiB",
            "clock" : "1GHz",
            "maxOutstanding" : 10,
            "opCount" : 1000,
            "write_freq" : 25,
            "read_freq" : 75,
        }
        cpu = sst.Component("core" + str(i), "memHierarchy.standardCPU")
        cpu.addParams(standardcpuparams)
        iface = cpu.setSubComponent("memory", "memHierarchy.standardInterface")
        link = sst.Link("link" + str(i))
        link.connect( (iface, "port", "1ns"),(bus, "high_network_" + str(i), "1ns") )
    else:
        continue

# EOF

