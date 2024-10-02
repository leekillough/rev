import os
import sst

DEBUG_L1 = 0
DEBUG_MEM = 0
DEBUG_LEVEL = 10
VERBOSE = 2
MEM_SIZE = 1024*1024*1024-1

# Define the simulation components
comp_cpu = sst.Component("cpu", "revcpu.RevCPU")
comp_cpu.addParams({
        "verbose" : 2,                                # Verbosity
    "numCores" : 1,                               # Number of cores
    "numHarts" : 4,                               # Number of cores
        "clock" : "2.0GHz",                           # Clock
    "memSize" : MEM_SIZE,                         # Memory size in bytes
    "machine" : "[CORES:RV64G]",                      # Core:Config; RV64I for core 0
    "startAddr" : "[0:0x00000000]",               # Starting address for core 0
    "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
    "program" : "scratchpad-overflow.exe",              # Target executable
    "enable_memH" : 1,                            # Enable memHierarchy support
    "enableVerScratchpad" : 1,                    # Enable verilator-simulated scratchpad module
    "splash" : 1                                  # Display the splash message
})
comp_cpu.enableAllStatistics()

scratch = comp_cpu.setSubComponent("scratch", "revcpu.VerilatorScratchpadCtrl") #Added verilator-backed scratchpad controller
scratch.addParams({
    "clock" : "2.0GHz",
    "verbose" : 2
})
verilatorsst = sst.Component("vsst", "verilatorcomponent.VerilatorComponent") # and verilatorsst component
model = verilatorsst.setSubComponent("model", "verilatorsstScratchpad.VerilatorSSTScratchpad") # and verilatorsst subcomponent
model.addParams({
    "useVPI" : 0,
    "clockFreq" : "2.0GHz",
    "clockPort" : "clk"
})
clk_link = sst.Link("clkLink")
clk_link.connect( (model, "clk", "0ps"), (scratch, "clk", "0ps") )
en_link = sst.Link("enLink")
en_link.connect( (model, "en", "0ps"), (scratch, "en", "0ps") )
addr_link = sst.Link("addrLink")
addr_link.connect( (model, "addr", "0ps"), (scratch, "addr", "0ps") )
write_link = sst.Link("writeLink")
write_link.connect( (model, "write", "0ps"), (scratch, "write", "0ps") )
wdata_link = sst.Link("wdataLink")
wdata_link.connect( (model, "wdata", "0ps"), (scratch, "wdata", "0ps") )
len_link = sst.Link("lenLink")
len_link.connect( (model, "len", "0ps"), (scratch, "len", "0ps") )
rdata_link = sst.Link("rdataLink")
rdata_link.connect( (model, "rdata", "0ps"), (scratch, "rdata", "0ps") )

# Create the RevMemCtrl subcomponent
comp_lsq = comp_cpu.setSubComponent("memory", "revcpu.RevBasicMemCtrl");
comp_lsq.addParams({
      "verbose"         : "5",
      "clock"           : "2.0Ghz",
      "max_loads"       : 16,
      "max_stores"      : 16,
      "max_flush"       : 16,
      "max_llsc"        : 16,
      "max_readlock"    : 16,
      "max_writeunlock" : 16,
      "max_custom"      : 16,
      "ops_per_cycle"   : 16
})
comp_lsq.enableAllStatistics({"type":"sst.AccumulatorStatistic"})

iface = comp_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
iface.addParams({
      "verbose" : VERBOSE
})


#l1cache = sst.Component("l1cache", "memHierarchy.Cache")
#l1cache.addParams({
#    "access_latency_cycles" : "4",
#    "cache_frequency" : "2 Ghz",
#    "replacement_policy" : "lru",
#    "coherence_protocol" : "MSI",
#    "associativity" : "4",
#    "cache_line_size" : "64",
#    "debug" : DEBUG_L1,
#    "debug_level" : DEBUG_LEVEL,
#    "verbose" : VERBOSE,
#    "L1" : "1",
#    "cache_size" : "16KiB"
#})

memctrl = sst.Component("memory", "memHierarchy.MemController")
memctrl.addParams({
    "debug" : DEBUG_MEM,
    "debug_level" : DEBUG_LEVEL,
    "clock" : "2GHz",
    "verbose" : VERBOSE,
    "addr_range_start" : 0,
    "addr_range_end" : MEM_SIZE,
    "backing" : "malloc"
})

memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
memory.addParams({
    "access_time" : "100ns",
    "mem_size" : "8GB"
})

#sst.setStatisticLoadLevel(7)
#sst.setStatisticOutput("sst.statOutputConsole")
#sst.enableAllStatisticsForAllComponents()

#link_cpu_cache_link = sst.Link("link_cpu_cache_link")
#link_cpu_cache_link.connect( (iface, "port", "1000ps"), (l1cache, "high_network_0", "1000ps") )
#link_mem_bus_link = sst.Link("link_mem_bus_link")
#link_mem_bus_link.connect( (l1cache, "low_network_0", "50ps"), (memctrl, "direct_link", "50ps") )

link_iface_mem = sst.Link("link_iface_mem")
link_iface_mem.connect( (iface, "port", "50ps"), (memctrl, "direct_link", "50ps") )

# EOF
