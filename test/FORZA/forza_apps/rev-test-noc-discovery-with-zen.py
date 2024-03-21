#
# Copyright (C) 2017-2023 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#
# rev-test-ex1.py
#

import os
import sst

# Define SST core options
sst.setProgramOption("timebase", "1ps")

DEBUG_L1 = 0
DEBUG_MEM = 0
DEBUG_LEVEL = 10
VERBOSE = 2
MEM_SIZE = 1024*1024*1024-1

# --------------------------
# SETUP THE ZEN
# --------------------------
zen = sst.Component("zen0", "forzazen.ZEN")
zen.addParams({
  "verbose" : 7,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 0,               # zone Id
  "numHarts" : 0,             # number of harts
  "numZaps" : 1,              # number of zaps
  "numZones" : 1,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 0,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000, # messages per cycle
  "enablePrecinctNIC": False  # enable the precinct NIC?
})

# --------------------------
# SETUP THE ZAP
# --------------------------
zap_cpu1 = sst.Component("zap0", "revcpu.RevCPU")
zap_cpu1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "numHarts" : 16,
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_main.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 0,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})


# --------------------------
# SETUP THE RZA
# --------------------------
rza = sst.Component("rza", "revcpu.RevCPU")
rza.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 2,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "program" : os.getenv("REV_EXE", "forza_main.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "enableRZA" : 1,                              # [FORZA] Enable RZA functionality
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "enable_memH" : 1,                            # Enable memHierarchy support
        "splash" : 0,                                 # Display the splash message
})

rza_lspipe = rza.setSubComponent("rza_ls","revcpu.RZALSCoProc")
rza_lspipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza_amopipe = rza.setSubComponent("rza_amo","revcpu.RZAAMOCoProc")
rza_amopipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza_lsq = rza.setSubComponent("memory", "revcpu.RevBasicMemCtrl");
rza_lsq.addParams({
      "verbose"         : "9",
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

iface = rza_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
iface.addParams({
      "verbose" : VERBOSE
})

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

# --------------------------
# SETUP THE NOC
# --------------------------
nic_params = {
  "verbose" : 9,
  "clock" : "1GHz",
  "req_per_cycle" : 1
}
net_params = {
  "input_buf_size" : "2048B",
  "output_buf_size" : "2048B",
  "link_bw" : "100GB/s"
}
rtr_params = {
  "xbar_bw" : "100GB/s",
  "flit_size" : "8B",
  "num_ports" : "3",
  "id" : 0
}
zap_nic = zap_cpu1.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface = zap_nic.setSubComponent("iface", "merlin.linkcontrol")

rza_nic = rza.setSubComponent("zone_nic", "forza.zopNIC")
rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")

zen_nic = zen.setSubComponent("zone_nic", "forza.zopNIC")
zen_iface = zen_nic.setSubComponent("iface", "merlin.linkcontrol")

router = sst.Component("router", "merlin.hr_router")
router.setSubComponent("topology", "merlin.singlerouter")

zap_nic.addParams(nic_params)
zap_iface.addParams(net_params)
rza_nic.addParams(nic_params)
rza_iface.addParams(net_params)
zen_nic.addParams(nic_params)
zen_iface.addParams(net_params)
router.addParams(net_params)
router.addParams(rtr_params)

# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA TO MEMORY
link_iface_mem = sst.Link("link_iface_mem")
link_iface_mem.connect( (iface, "port", "50ps"), (memctrl, "direct_link", "50ps") )

#-- NOC
zap_link0 = sst.Link("zap_link")
zap_link0.connect( (zap_iface, "rtr_port", "1us"), (router, "port0", "1us") )

rza_link0 = sst.Link("rza_link")
rza_link0.connect( (rza_iface, "rtr_port", "1us"), (router, "port1", "1us") )

zen_link0 = sst.Link("zen_link")
zen_link0.connect( (zen_iface, "rtr_port", "1us"), (router, "port2", "1us") )

# EOF
