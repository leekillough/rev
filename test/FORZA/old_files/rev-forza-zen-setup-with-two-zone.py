#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
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
# SETUP THE ZENS
# --------------------------
zen0 = sst.Component("zen0", "forzazen.ZEN")
zen0.addParams({
  "verbose" : 9,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 0,               # zone Id
  "numHarts" : 1,             # number of harts
  "numZaps" : 4,              # number of zaps
  "numZones" : 2,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 0,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000  # messages per cycle
})
zen1 = sst.Component("zen1", "forzazen.ZEN")
zen1.addParams({
  "verbose" : 9,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 1,               # zone Id
  "numHarts" : 1,             # number of harts
  "numZaps" : 4,              # number of zaps
  "numZones" : 2,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 0,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000  # messages per cycle
})


# --------------------------
# SETUP THE ZQMS
# --------------------------
#zqm0 = sst.Component("zqm0", "forzazqm.ZQM")
#zqm0.addParams({
#  "verbose" : 1,              # Verbosity
#  "clockFreq" : "1.0GHz",     # Clock Frequency
#  "precinctId" : 0,           # precinct Id
#  "zoneId" : 0,               # zone Id
#  "numHarts" : 1,             # number of harts
#  "numCores" : 4,             # number of cores
#})
#zqm1 = sst.Component("zqm1", "forzazqm.ZQM")
#zqm1.addParams({
#  "verbose" : 1,              # Verbosity
#  "clockFreq" : "1.0GHz",     # Clock Frequency
#  "precinctId" : 0,           # precinct Id
#  "zoneId" : 1,               # zone Id
#  "numHarts" : 1,             # number of harts
#  "numCores" : 4,             # number of cores
#})


# --------------------------
# SETUP THE ZONE0 ZAPS
# --------------------------
zap_cpu0_0 = sst.Component("zap0_0", "revcpu.RevCPU")
zap_cpu0_0.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 0,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu0_1 = sst.Component("zap0_1", "revcpu.RevCPU")
zap_cpu0_1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*200),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 1,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu0_2 = sst.Component("zap0_2", "revcpu.RevCPU")
zap_cpu0_2.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*400),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 2,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu0_3 = sst.Component("zap0_3", "revcpu.RevCPU")
zap_cpu0_3.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*600),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 3,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

# --------------------------
# SETUP THE ZONE1 ZAPS
# --------------------------
zap_cpu1_0 = sst.Component("zap1_0", "revcpu.RevCPU")
zap_cpu1_0.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 1,                                 # [FORZA] zone ID
        "zapId" : 0,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu1_1 = sst.Component("zap1_1", "revcpu.RevCPU")
zap_cpu1_1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*200),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 1,                                 # [FORZA] zone ID
        "zapId" : 1,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu1_2 = sst.Component("zap1_2", "revcpu.RevCPU")
zap_cpu1_2.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*400),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 1,                                 # [FORZA] zone ID
        "zapId" : 2,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu1_3 = sst.Component("zap1_3", "revcpu.RevCPU")
zap_cpu1_3.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*600),                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 1,                                 # [FORZA] zone ID
        "zapId" : 3,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})



# --------------------------
# SETUP THE ZONE0 RZA
# --------------------------
rza0 = sst.Component("rza0", "revcpu.RevCPU")
rza0.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 2,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*1200),                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "enableRZA" : 1,                              # [FORZA] Enable RZA functionality
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "enable_memH" : 1,                            # Enable memHierarchy support
        "splash" : 0,                                 # Display the splash message
})

rza0_lspipe = rza0.setSubComponent("rza_ls","revcpu.RZALSCoProc")
rza0_lspipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza0_amopipe = rza0.setSubComponent("rza_amo","revcpu.RZAAMOCoProc")
rza0_amopipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza0_lsq = rza0.setSubComponent("memory", "revcpu.RevBasicMemCtrl");
rza0_lsq.addParams({
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

iface0 = rza0_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
iface0.addParams({
      "verbose" : VERBOSE
})

memctrl0 = sst.Component("memory0", "memHierarchy.MemController")
memctrl0.addParams({
    "debug" : DEBUG_MEM,
    "debug_level" : DEBUG_LEVEL,
    "clock" : "2GHz",
    "verbose" : VERBOSE,
    "addr_range_start" : 0,
    "addr_range_end" : MEM_SIZE+(1024*1024*1200),
    "backing" : "malloc"
})

memory0 = memctrl0.setSubComponent("backend", "memHierarchy.simpleMem")
memory0.addParams({
    "access_time" : "100ns",
    "mem_size" : "8GB"
})

# --------------------------
# SETUP THE ZONE1 RZA
# --------------------------
rza1 = sst.Component("rza1", "revcpu.RevCPU")
rza1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 2,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024+(1024*1024*1200),                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "program" : os.getenv("REV_EXE", "forza_zen_setup.exe"),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "enableRZA" : 1,                              # [FORZA] Enable RZA functionality
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 1,                                 # [FORZA] zone ID
        "enable_memH" : 1,                            # Enable memHierarchy support
        "splash" : 0,                                 # Display the splash message
})

rza1_lspipe = rza1.setSubComponent("rza_ls","revcpu.RZALSCoProc")
rza1_lspipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza1_amopipe = rza1.setSubComponent("rza_amo","revcpu.RZAAMOCoProc")
rza1_amopipe.addParams({
  "clock" : "1.0GHz",
  "verbose" : 9,
})

rza1_lsq = rza1.setSubComponent("memory", "revcpu.RevBasicMemCtrl");
rza1_lsq.addParams({
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

iface1 = rza1_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
iface1.addParams({
      "verbose" : VERBOSE
})

memctrl1 = sst.Component("memory1", "memHierarchy.MemController")
memctrl1.addParams({
    "debug" : DEBUG_MEM,
    "debug_level" : DEBUG_LEVEL,
    "clock" : "2GHz",
    "verbose" : VERBOSE,
    "addr_range_start" : 0,
    "addr_range_end" : MEM_SIZE+(1024*1024*1200),
    "backing" : "malloc"
})

memory1 = memctrl1.setSubComponent("backend", "memHierarchy.simpleMem")
memory1.addParams({
    "access_time" : "100ns",
    "mem_size" : "8GB"
})



# --------------------------
# SETUP THE ZONE0 NOC
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
  "num_ports" : "6",
  "id" : 0
}

zap_nic0_0 = zap_cpu0_0.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface0_0 = zap_nic0_0.setSubComponent("iface", "merlin.linkcontrol")

zap_nic0_1 = zap_cpu0_1.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface0_1 = zap_nic0_1.setSubComponent("iface", "merlin.linkcontrol")

zap_nic0_2 = zap_cpu0_2.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface0_2 = zap_nic0_2.setSubComponent("iface", "merlin.linkcontrol")

zap_nic0_3 = zap_cpu0_3.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface0_3 = zap_nic0_3.setSubComponent("iface", "merlin.linkcontrol")

rza_nic0 = rza0.setSubComponent("zone_nic", "forza.zopNIC")
rza_iface0 = rza_nic0.setSubComponent("iface", "merlin.linkcontrol")

zen_nic0 = zen0.setSubComponent("zone_nic", "forza.zopNIC")
zen_iface0 = zen_nic0.setSubComponent("iface", "merlin.linkcontrol")

#zqm_nic0 = zqm0.setSubComponent("zone_nic", "forza.zopNIC")
#zqm_iface0 = zqm_nic0.setSubComponent("iface", "merlin.linkcontrol")

router0 = sst.Component("router0", "merlin.hr_router")
router0.setSubComponent("topology", "merlin.singlerouter")

zap_nic0_0.addParams(nic_params)
zap_iface0_0.addParams(net_params)
zap_nic0_1.addParams(nic_params)
zap_iface0_1.addParams(net_params)
zap_nic0_2.addParams(nic_params)
zap_iface0_2.addParams(net_params)
zap_nic0_3.addParams(nic_params)
zap_iface0_3.addParams(net_params)

rza_nic0.addParams(nic_params)
rza_iface0.addParams(net_params)
zen_nic0.addParams(nic_params)
zen_iface0.addParams(net_params)
#zqm_nic0.addParams(nic_params)
#zqm_iface0.addParams(net_params)
router0.addParams(net_params)
router0.addParams(rtr_params)

# --------------------------
# SETUP THE ZONE1 NOC
# --------------------------
zap_nic1_0 = zap_cpu1_0.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface1_0 = zap_nic1_0.setSubComponent("iface", "merlin.linkcontrol")

zap_nic1_1 = zap_cpu1_1.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface1_1 = zap_nic1_1.setSubComponent("iface", "merlin.linkcontrol")

zap_nic1_2 = zap_cpu1_2.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface1_2 = zap_nic1_2.setSubComponent("iface", "merlin.linkcontrol")

zap_nic1_3 = zap_cpu1_3.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface1_3 = zap_nic1_3.setSubComponent("iface", "merlin.linkcontrol")

rza_nic1 = rza1.setSubComponent("zone_nic", "forza.zopNIC")
rza_iface1 = rza_nic1.setSubComponent("iface", "merlin.linkcontrol")

zen_nic1 = zen1.setSubComponent("zone_nic", "forza.zopNIC")
zen_iface1 = zen_nic1.setSubComponent("iface", "merlin.linkcontrol")

#zqm_nic1 = zqm1.setSubComponent("zone_nic", "forza.zopNIC")
#zqm_iface1 = zqm_nic1.setSubComponent("iface", "merlin.linkcontrol")

router1 = sst.Component("router1", "merlin.hr_router")
router1.setSubComponent("topology", "merlin.singlerouter")

zap_nic1_0.addParams(nic_params)
zap_iface1_0.addParams(net_params)
zap_nic1_1.addParams(nic_params)
zap_iface1_1.addParams(net_params)
zap_nic1_2.addParams(nic_params)
zap_iface1_2.addParams(net_params)
zap_nic1_3.addParams(nic_params)
zap_iface1_3.addParams(net_params)

rza_nic1.addParams(nic_params)
rza_iface1.addParams(net_params)
zen_nic1.addParams(nic_params)
zen_iface1.addParams(net_params)
#zqm_nic1.addParams(nic_params)
#zqm_iface1.addParams(net_params)
router1.addParams(net_params)
router1.addParams(rtr_params)

# --------------------------
# SETUP THE PRECINCT NOC
# --------------------------
prec_rtr_params = {
  "xbar_bw" : "100GB/s",
  "flit_size" : "8B",
  "num_ports" : "2",
  "id" : 1
}

prec_nic0 = zen0.setSubComponent("precinct_nic", "forza.zopNIC")
prec_iface0 = prec_nic0.setSubComponent("iface", "merlin.linkcontrol")
prec_nic1 = zen1.setSubComponent("precinct_nic", "forza.zopNIC")
prec_iface1 = prec_nic1.setSubComponent("iface", "merlin.linkcontrol")
prec0_router = sst.Component("prec0_router", "merlin.hr_router")
prec0_router.setSubComponent("topology", "merlin.singlerouter")

prec_nic0.addParams(nic_params)
prec_iface0.addParams(net_params)
prec_nic1.addParams(nic_params)
prec_iface1.addParams(net_params)
prec0_router.addParams(net_params)
prec0_router.addParams(prec_rtr_params)


# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA0 TO MEMORY
link_iface_mem0 = sst.Link("link_iface_mem0")
link_iface_mem0.connect( (iface0, "port", "50ps"), (memctrl0, "direct_link", "50ps") )

# -- RZA1 TO MEMORY
link_iface_mem1 = sst.Link("link_iface_mem1")
link_iface_mem1.connect( (iface1, "port", "50ps"), (memctrl1, "direct_link", "50ps") )

#-- ZONE0 NOC
zap_link0_0 = sst.Link("zap_link0_0")
zap_link0_0.connect( (zap_iface0_0, "rtr_port", "1us"), (router0, "port0", "1us") )

zap_link0_1 = sst.Link("zap_link0_1")
zap_link0_1.connect( (zap_iface0_1, "rtr_port", "1us"), (router0, "port1", "1us") )

zap_link0_2 = sst.Link("zap_link0_2")
zap_link0_2.connect( (zap_iface0_2, "rtr_port", "1us"), (router0, "port2", "1us") )

zap_link0_3 = sst.Link("zap_link0_3")
zap_link0_3.connect( (zap_iface0_3, "rtr_port", "1us"), (router0, "port3", "1us") )

rza_link0 = sst.Link("rza_link0")
rza_link0.connect( (rza_iface0, "rtr_port", "1us"), (router0, "port4", "1us") )

zen_link0 = sst.Link("zen_link0")
zen_link0.connect( (zen_iface0, "rtr_port", "1us"), (router0, "port5", "1us") )

#zqm_link0 = sst.Link("zqm_link0")
#zqm_link0.connect( (zqm_iface0, "rtr_port", "1us"), (router0, "port6", "1us") )

#-- ZONE1 NOC
zap_link1_0 = sst.Link("zap_link1_0")
zap_link1_0.connect( (zap_iface1_0, "rtr_port", "1us"), (router1, "port0", "1us") )

zap_link1_1 = sst.Link("zap_link1_1")
zap_link1_1.connect( (zap_iface1_1, "rtr_port", "1us"), (router1, "port1", "1us") )

zap_link1_2 = sst.Link("zap_link1_2")
zap_link1_2.connect( (zap_iface1_2, "rtr_port", "1us"), (router1, "port2", "1us") )

zap_link1_3 = sst.Link("zap_link1_3")
zap_link1_3.connect( (zap_iface1_3, "rtr_port", "1us"), (router1, "port3", "1us") )

rza_link1 = sst.Link("rza_link1")
rza_link1.connect( (rza_iface1, "rtr_port", "1us"), (router1, "port4", "1us") )

zen_link1 = sst.Link("zen_link1")
zen_link1.connect( (zen_iface1, "rtr_port", "1us"), (router1, "port5", "1us") )

#zqm_link1 = sst.Link("zqm_link1")
#zqm_link1.connect( (zqm_iface1, "rtr_port", "1us"), (router1, "port6", "1us") )

#-- PRECINCT NOC
zen_prec_link0 = sst.Link("zen_prec_link0")
zen_prec_link0.connect( (prec_iface0, "rtr_port", "1us"), (prec0_router, "port0", "1us") )
zen_prec_link1 = sst.Link("zen_prec_link1")
zen_prec_link1.connect( (prec_iface1, "rtr_port", "1us"), (prec0_router, "port1", "1us") )

# EOF
