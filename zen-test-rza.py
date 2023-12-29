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
router = sst.Component("router", "merlin.hr_router")

router.addGlobalParamSet("router_params")
router.addParams({"id": 0, "num_ports": 4})
topo = router.setSubComponent("topology", "merlin.singlerouter", 0)
topo.addGlobalParamSet("topology_params")

# --------------------------
# SETUP THE ZOPGen, ZEN
# --------------------------
zap0 = sst.Component("zap0", "Forza.ZOPGen")
#zen0.addGlobalParamSet("zen_params")
zap0.addParams({"int_id" : 0})

zap0_lc = zap0.setSubComponent("m_zop_iface", "Forza.zenZopNIC", 0)
zap0_lc.addGlobalParamSet("networkLinkControl_params")
zap0_zopapi_lc = zap0_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap0_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap0_lc.addParams({"verbose": 10})


#zen0_rtrlink = sst.Link("link_zen0_rtr0")
#router.addLink(zen0_rtrlink, "port0", "5 ns")
#zen0_lc.addLink(zen0_rtrlink, "network", "5 ns")
zap0_link0 = sst.Link("zap0_link")
zap0_link0.connect( (zap0_zopapi_lc, "rtr_port", "1us"), (router, "port2", "1us") )
zap1 = sst.Component("zap1", "Forza.ZOPGen")
#zen0.addGlobalParamSet("zen_params")
zap1.addParams({"int_id" : 1})



zap1_lc = zap1.setSubComponent("m_zop_iface", "Forza.zenZopNIC", 0)
zap1_lc.addGlobalParamSet("networkLinkControl_params")
zap1_zopapi_lc = zap1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zap1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zap1_lc.addParams({"verbose": 10})


#zen0_rtrlink = sst.Link("link_zen0_rtr0")
#router.addLink(zen0_rtrlink, "port0", "5 ns")
#zen0_lc.addLink(zen0_rtrlink, "network", "5 ns")
zap1_link0 = sst.Link("zap1_link")
zap1_link0.connect( (zap1_zopapi_lc, "rtr_port", "1us"), (router, "port3", "1us") )
zen1 = sst.Component("zen1", "Forza.ZEN")
#zen0.addGlobalParamSet("zen_params")
zen1.addParams({"int_id" : 1, "dma_enabled": 1})

zen1_lc = zen1.setSubComponent("m_zop_iface", "Forza.zenZopNIC", 0)
zen1_lc.addGlobalParamSet("networkLinkControl_params")
zen1_zopapi_lc = zen1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zen1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
zen1_link = sst.Link("zen1_link")
zen1_link.connect( (zen1_zopapi_lc, "rtr_port", "1us"), (router, "port1", "1us") )
zen1_lc.addParams({"verbose": 10})

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
        "program" : os.getenv("REV_EXE", "ex2.exe"),  # Target executable
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
rza_nic = rza.setSubComponent("zone_nic", "Forza.zopNIC")
rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")

rza_iface.addGlobalParamSet("networkLinkControl_params")
rza_nic.addParams(nic_params)
#rza_iface.addParams(net_params)

# --------------------------
# LINK THE VARIOUS COMPONENTS
# --------------------------
# -- RZA TO MEMORY
link_iface_mem = sst.Link("link_iface_mem")
link_iface_mem.connect( (iface, "port", "50ps"), (memctrl, "direct_link", "50ps") )

rza_link0 = sst.Link("rza_link")
rza_link0.connect( (rza_iface, "rtr_port", "1us"), (router, "port0", "1us") )

# EOF
