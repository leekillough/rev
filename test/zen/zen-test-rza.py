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

sst.addGlobalParams("topology_params", {
    "num_ports" : "3"
})
net_params = {
  "input_buf_size" : "2048B",
  "output_buf_size" : "2048B",
  "link_bw" : "100GB/s"
}
nic_params = {
  "verbose" : 9,
  "clock" : "1GHz",
  "req_per_cycle" : 1
}
sst.addGlobalParams("networkLinkControl_params",{
    "input_buf_size" : "14kB",
    "job_id" : "0",
    "job_size" : "2",
    #"link_bw": "1B/s",
    "link_bw" : "100Gb/s",
    "output_buf_size" : "14kB",
    "use_nid_remap" : "False",
})

sst.addGlobalParams("router_params", {
    "flit_size" : "8B",
    "input_buf_size" : "14kB",
    "input_latency" : "47ns",
    #"link_bw": "1B/s",
    "link_bw" : "100Gb/s",
    "num_vns" : "1",
    "output_buf_size" : "14kB",
    "output_latency" : "47ns",
    "xbar_bw" : "100Gb/s",
})

prec_router = sst.Component("prec_router", "merlin.hr_router")

prec_router.addGlobalParamSet("router_params")
prec_router.addParams({"id": 0, "num_ports": 2})
prec_topo = prec_router.setSubComponent("topology", "merlin.singlerouter", 0)
prec_topo.addGlobalParamSet("topology_params")

##########
# ZONE 0 #
##########

router0 = sst.Component("router0", "merlin.hr_router")

router0.addGlobalParamSet("router_params")
router0.addParams({"id": 1, "num_ports": 4})
topo0 = router0.setSubComponent("topology", "merlin.singlerouter", 0)
topo0.addGlobalParamSet("topology_params")

# --------------------------
# SETUP THE ZOPGen, ZEN
# --------------------------
zap0_0 = sst.Component("zap0_0", "forzazen.ZOPGen")
zap0_0.addParams({"int_id" : 0, "zoneId" : 0})
zap0_0_lc = zap0_0.setSubComponent("m_zop_iface", "forza.zopNIC", 0)
zap0_0_lc.addGlobalParamSet("networkLinkControl_params")
zap0_0_zopapi_lc = zap0_0_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap0_0_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap0_0_lc.addParams({"verbose": 10})
zap0_0_link0 = sst.Link("zap0_0_link")
zap0_0_link0.connect( (zap0_0_zopapi_lc, "rtr_port", "1us"), (router0, "port2", "1us") )

zap0_1 = sst.Component("zap0_1", "forzazen.ZOPGen")
zap0_1.addParams({"int_id" : 1, "tests" : 1, "zoneId" : 0})
zap0_1_lc = zap0_1.setSubComponent("m_zop_iface", "forza.zopNIC", 0)
zap0_1_lc.addGlobalParamSet("networkLinkControl_params")
zap0_1_zopapi_lc = zap0_1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap0_1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap0_1_lc.addParams({"verbose": 10})
zap0_1_link0 = sst.Link("zap0_1_link")
zap0_1_link0.connect( (zap0_1_zopapi_lc, "rtr_port", "1us"), (router0, "port3", "1us") )

zen0 = sst.Component("zen0", "forzazen.ZEN")
zen0.addParams({
  "verbose" : 10,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 0,               # zone Id
  "numHarts" : 2,             # number of harts
  "numZaps" : 2,              # number of zaps
  "numZones" : 2,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 1,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000, # messages per cycle
  "enablePrecinctNIC" : True
})
zen0_lc = zen0.setSubComponent("zone_nic", "forza.zopNIC", 0)
zen0_lc.addGlobalParamSet("networkLinkControl_params")
zen0_zopapi_lc = zen0_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zen0_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zen0_link = sst.Link("zen0_link")
zen0_link.connect( (zen0_zopapi_lc, "rtr_port", "1us"), (router0, "port1", "1us") )
zen0_lc.addParams({"verbose": 10})

zen0_prec_nic = zen0.setSubComponent("precinct_nic", "forza.zopNIC", 0)
zen0_prec_nic.addGlobalParamSet("networkLinkControl_params")
zen0_prec_linkcontrol = zen0_prec_nic.setSubComponent("iface", "merlin.linkcontrol", 0)
zen0_prec_linkcontrol.addGlobalParamSet("networkLinkControl_params")
zen0_prec_link = sst.Link("zen0_prec_link")
zen0_prec_link.connect((zen0_prec_linkcontrol, "rtr_port", "1us"), (prec_router, "port0", "1us"))

# --------------------------
# SETUP THE RZA
# --------------------------
rza0 = sst.Component("rza0", "revcpu.RevCPU")
rza0.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 2,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "program" : os.getenv("REV_EXE", "ex2.exe"),  # Target executable
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
    "addr_range_end" : MEM_SIZE,
    "backing" : "malloc"
})

memory0 = memctrl0.setSubComponent("backend", "memHierarchy.simpleMem")
memory0.addParams({
    "access_time" : "100ns",
    "mem_size" : "8GB"
})

# --------------------------
# SETUP THE NOC
# --------------------------
rza0_nic = rza0.setSubComponent("zone_nic", "forza.zopNIC")
rza0_iface = rza0_nic.setSubComponent("iface", "merlin.linkcontrol")

rza0_iface.addGlobalParamSet("networkLinkControl_params")
rza0_nic.addParams(nic_params)
rza0_iface.addParams(net_params)

# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA TO MEMORY
link0_iface_mem = sst.Link("link0_iface_mem")
link0_iface_mem.connect( (iface0, "port", "50ps"), (memctrl0, "direct_link", "50ps") )

rza0_link = sst.Link("rza0_link")
rza0_link.connect( (rza0_iface, "rtr_port", "1us"), (router0, "port0", "1us") )

##########
# ZONE 1 #
##########

router1 = sst.Component("router1", "merlin.hr_router")

router1.addGlobalParamSet("router_params")
router1.addParams({"id": 2, "num_ports": 4})
topo1 = router1.setSubComponent("topology", "merlin.singlerouter", 0)
topo1.addGlobalParamSet("topology_params")

# --------------------------
# SETUP THE ZOPGen, ZEN
# --------------------------
zap1_0 = sst.Component("zap1_0", "forzazen.ZOPGen")
zap1_0.addParams({"int_id" : 0, "zoneId" : 1})
zap1_0_lc = zap1_0.setSubComponent("m_zop_iface", "forza.zopNIC", 0)
zap1_0_lc.addGlobalParamSet("networkLinkControl_params")
zap1_0_zopapi_lc = zap1_0_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap1_0_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap1_0_lc.addParams({"verbose": 10})
zap1_0_link0 = sst.Link("zap1_0_link")
zap1_0_link0.connect( (zap1_0_zopapi_lc, "rtr_port", "1us"), (router1, "port2", "1us") )

zap1_1 = sst.Component("zap1_1", "forzazen.ZOPGen")
zap1_1.addParams({"int_id" : 1, "tests" : 1, "zoneId" : 1})
zap1_1_lc = zap1_1.setSubComponent("m_zop_iface", "forza.zopNIC", 0)
zap1_1_lc.addGlobalParamSet("networkLinkControl_params")
zap1_1_zopapi_lc = zap1_1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap1_1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap1_1_lc.addParams({"verbose": 10})
zap1_1_link0 = sst.Link("zap1_1_link")
zap1_1_link0.connect( (zap1_1_zopapi_lc, "rtr_port", "1us"), (router1, "port3", "1us") )

zen1 = sst.Component("zen1", "forzazen.ZEN")
zen1.addParams({
  "verbose" : 10,              # Verbosity
  "clockFreq" : "1.0GHz",     # Clock Frequency
  "precinctId" : 0,           # precinct Id
  "zoneId" : 1,               # zone Id
  "numHarts" : 2,             # number of harts
  "numZaps" : 2,              # number of zaps
  "numZones" : 2,             # number of zones
  "numPrecincts" : 1,         # number of precincts
  "enableDMA" : 1,            # enable the DMA?
  "zenQSizeLimit" : 100000,   # zenQ size limit
  "processPerCycle" : 100000, # messages per cycle
  "enablePrecinctNIC" : True
})
zen1_lc = zen1.setSubComponent("zone_nic", "forza.zopNIC", 0)
zen1_lc.addGlobalParamSet("networkLinkControl_params")
zen1_zopapi_lc = zen1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zen1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zen1_link = sst.Link("zen1_link")
zen1_link.connect( (zen1_zopapi_lc, "rtr_port", "1us"), (router1, "port1", "1us") )
zen1_lc.addParams({"verbose": 10})

zen1_prec_nic = zen1.setSubComponent("precinct_nic", "forza.zopNIC", 0)
zen1_prec_nic.addGlobalParamSet("networkLinkControl_params")
zen1_prec_linkcontrol = zen1_prec_nic.setSubComponent("iface", "merlin.linkcontrol", 0)
zen1_prec_linkcontrol.addGlobalParamSet("networkLinkControl_params")
zen1_prec_link = sst.Link("zen1_prec_link")
zen1_prec_link.connect((zen1_prec_linkcontrol, "rtr_port", "1us"), (prec_router, "port1", "1us"))

# --------------------------
# SETUP THE RZA
# --------------------------
rza1 = sst.Component("rza1", "revcpu.RevCPU")
rza1.addParams({
        "verbose" : 5,                                # Verbosity
        "numCores" : 2,                               # Number of cores
        "clock" : "1.0GHz",                           # Clock
        "memSize" : 1024*1024*1024,                   # Memory size in bytes
        "machine" : "[CORES:RV64G]",                  # Core:Config; RV64I for core 0
        "startAddr" : "[CORES:0x00000000]",           # Starting address for core 0
        "program" : os.getenv("REV_EXE", "ex2.exe"),  # Target executable
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
    "addr_range_end" : MEM_SIZE,
    "backing" : "malloc"
})

memory1 = memctrl1.setSubComponent("backend", "memHierarchy.simpleMem")
memory1.addParams({
    "access_time" : "100ns",
    "mem_size" : "8GB"
})

# --------------------------
# SETUP THE NOC
# --------------------------
rza1_nic = rza1.setSubComponent("zone_nic", "forza.zopNIC")
rza1_iface = rza1_nic.setSubComponent("iface", "merlin.linkcontrol")

rza1_iface.addGlobalParamSet("networkLinkControl_params")
rza1_nic.addParams(nic_params)
rza1_iface.addParams(net_params)

# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA TO MEMORY
link1_iface_mem = sst.Link("link1_iface_mem")
link1_iface_mem.connect( (iface1, "port", "50ps"), (memctrl1, "direct_link", "50ps") )

rza1_link = sst.Link("rza1_link")
rza1_link.connect( (rza1_iface, "rtr_port", "1us"), (router1, "port0", "1us") )


# EOF
