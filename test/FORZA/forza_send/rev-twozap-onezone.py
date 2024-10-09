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
sst.setProgramOption("stop-at", "1ms")

DEBUG_L1 = 0
DEBUG_MEM = 0
DEBUG_LEVEL = 10
VERBOSE = 2
MEM_SIZE = 1024*1024*1024
EXE="forza_send.exe"

# --------------------------
# SETUP THE ZEN
# --------------------------
zen = sst.Component("zen0", "forzazen.ZEN")
zen.addParams({
  "verbose" : 9,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 0,               # zone Id
  "numHarts" : 32,             # number of harts
  "numZaps" : 2,
  "numZones" : 1,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 0,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000, # messages per cycle
  "enablePrecinctNIC": False  # enable the precinct NIC?
})

# --------------------------
# SETUP THE ZQM
# --------------------------
zqm = sst.Component("zqm0", "forzazqm.ZQM")
zqm.addParams({
  "verbose" : 9,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 0,               # zone Id
  "numHarts" : 32,             # number of harts
  "numCores" : 2,             # number of cores
})

# --------------------------
# SETUP THE ZAPS
# --------------------------
zap_cpu0 = sst.Component("zap0", "revcpu.RevCPU")
zap_cpu0.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "numHarts" : 32,
        "clock" : "1.0GHz",                           # Clock
        "memSize" : MEM_SIZE,                   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "args"    : "0",
        "program" : os.getenv("REV_EXE", EXE),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 0,                                  # [FORZA] zap ID
        "splash" : 0                                  # Display the splash message
})

zap_cpu1 = sst.Component("zap1", "revcpu.RevCPU")
zap_cpu1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 1,                               # Number of cores
        "numHarts" : 32,
        "clock" : "1.0GHz",                           # Clock
        "memSize" : MEM_SIZE+(1024*1024*200),   # Memory size in bytes
        "machine" : "[0:RV64GC]",                     # Core:Config; RV64I for core 0
        "startAddr" : "[0:0x00000000]",               # Starting address for core 0
        "memCost" : "[0:1:10]",                       # Memory loads required 1-10 cycles
        "args"    : "0",
        "program" : os.getenv("REV_EXE", EXE),  # Target executable
        "enableZoneNIC" : 1,                          # [FORZA] Enable the zone NIC
        "precinctId" : 0,                             # [FORZA] precinct ID
        "zoneId" : 0,                                 # [FORZA] zone ID
        "zapId" : 1,                                  # [FORZA] zap ID
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
        "memSize" : MEM_SIZE+(1024*1024*1200),                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "args"    : "7",
        "program" : os.getenv("REV_EXE", EXE),  # Target executable
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

rza_lsq = rza.setSubComponent("memory", "revcpu.RevBasicMemCtrl")
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
    "addr_range_end" : MEM_SIZE+(1024*1024*1200),
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
  "num_ports" : "5",
  "id" : 0
}
ring_rtr_params = {
  "xbar_bw" : "100GB/s",
  "flit_size" : "8B",
  "num_ports" : "4",
  "id" : 0
}

zap_nic0 = zap_cpu0.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface0 = zap_nic0.setSubComponent("iface", "merlin.linkcontrol")

zap0_ring_nic = zap_cpu0.setSubComponent("ring_nic", "forza.RingNetNIC")
zap0_ring_iface = zap0_ring_nic.setSubComponent("iface", "merlin.linkcontrol")

zap_nic1 = zap_cpu1.setSubComponent("zone_nic", "forza.zopNIC")
zap_iface1 = zap_nic1.setSubComponent("iface", "merlin.linkcontrol")

zap1_ring_nic = zap_cpu1.setSubComponent("ring_nic", "forza.RingNetNIC")
zap1_ring_iface = zap1_ring_nic.setSubComponent("iface", "merlin.linkcontrol")

rza_nic = rza.setSubComponent("zone_nic", "forza.zopNIC")
rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")

zen_nic = zen.setSubComponent("zone_nic", "forza.zopNIC")
zen_iface = zen_nic.setSubComponent("iface", "merlin.linkcontrol")

zen_ring_nic = zen.setSubComponent("ring_nic", "forza.RingNetNIC")
zen_ring_iface = zen_ring_nic.setSubComponent("iface", "merlin.linkcontrol")

zqm_nic = zqm.setSubComponent("zone_nic", "forza.zopNIC")
zqm_iface = zqm_nic.setSubComponent("iface", "merlin.linkcontrol")

zqm_ring_nic = zqm.setSubComponent("ring_nic", "forza.RingNetNIC")
zqm_ring_iface = zqm_ring_nic.setSubComponent("iface", "merlin.linkcontrol")

router = sst.Component("router", "merlin.hr_router")
router.setSubComponent("topology", "merlin.singlerouter")

ring_router = sst.Component("ring_router", "merlin.hr_router")
ring_router.setSubComponent("topology", "merlin.singlerouter")

zap_nic0.addParams(nic_params)
zap_iface0.addParams(net_params)
zap_nic1.addParams(nic_params)
zap_iface1.addParams(net_params)
rza_nic.addParams(nic_params)
rza_iface.addParams(net_params)
zen_nic.addParams(nic_params)
zen_iface.addParams(net_params)
zqm_nic.addParams(nic_params)
zqm_iface.addParams(net_params)
router.addParams(net_params)
router.addParams(rtr_params)

zap0_ring_nic.addParams(nic_params)
zap0_ring_iface.addParams(net_params)
zap1_ring_nic.addParams(nic_params)
zap1_ring_iface.addParams(net_params)
zen_ring_nic.addParams(nic_params)
zen_ring_iface.addParams(net_params)
zqm_ring_nic.addParams(nic_params)
zqm_ring_iface.addParams(net_params)
ring_router.addParams(net_params)
ring_router.addParams(ring_rtr_params)

# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA TO MEMORY
link_iface_mem = sst.Link("link_iface_mem")
link_iface_mem.connect( (iface, "port", "50ps"), (memctrl, "direct_link", "50ps") )

#-- NOC
zap_link0 = sst.Link("zap_link0")
zap_link0.connect( (zap_iface0, "rtr_port", "1us"), (router, "port0", "1us") )

zap_link1 = sst.Link("zap_link1")
zap_link1.connect( (zap_iface1, "rtr_port", "1us"), (router, "port1", "1us") )

rza_link0 = sst.Link("rza_link")
rza_link0.connect( (rza_iface, "rtr_port", "1us"), (router, "port2", "1us") )

zen_link0 = sst.Link("zen_link")
zen_link0.connect( (zen_iface, "rtr_port", "1us"), (router, "port3", "1us") )

zqm_link0 = sst.Link("zqm_link")
zqm_link0.connect( (zqm_iface, "rtr_port", "1us"), (router, "port4", "1us") )

#-- RING NOC
zap_ring_link0 = sst.Link("zap_ring_link0")
zap_ring_link0.connect( (zap0_ring_iface, "rtr_port", "1us"), (ring_router, "port0", "1us") )

zap_ring_link1 = sst.Link("zap_ring_link1")
zap_ring_link1.connect( (zap1_ring_iface, "rtr_port", "1us"), (ring_router, "port1", "1us") )

zen_ring_link0 = sst.Link("zen_ring_link")
zen_ring_link0.connect( (zen_ring_iface, "rtr_port", "1us"), (ring_router, "port2", "1us") )

zqm_ring_link0 = sst.Link("zqm_ring_link")
zqm_ring_link0.connect( (zqm_ring_iface, "rtr_port", "1us"), (ring_router, "port3", "1us") )

# EOF
