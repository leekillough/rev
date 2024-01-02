#
# Copyright 2022-2023 Tactical Computing Laboratories LLC
#   Modified by Lucata Corp.
#

import os
import sst

# argv[0] = 'sstsim.x'
# argv[1] = '--debug'
# argv[2] = 'INTEGER'
# argv[3] = '--verbose'
# argv[4] = 'INTEGER'

# Ignore these requirements for now
#if len(sys.argv) < 5:
#    print("ERROR : Simulation model requires --model-options=\"--debug INT --verbose INT\"")
#    print(sys.argv)
#    sst.exit()

#DEBUG = int(sys.argv[2])
#VERBOSE = int(sys.argv[4])

# Note: Copied from zen-test.py (and matches zen-test-rza.py also)
sst.addGlobalParams("networkLinkControl_params",{
    "input_buf_size" : "14kB",
    "job_id" : "0",
    "job_size" : "2",
    "link_bw" : "100Gb/s",
    "output_buf_size" : "14kB",
    "use_nid_remap" : "False",
})

sst.addGlobalParams("router_params", {
    "flit_size" : "8B",
    "input_buf_size" : "14kB",
    "input_latency" : "47ns",
    "link_bw" : "100Gb/s",
    "num_vns" : "1",
    "output_buf_size" : "14kB",
    "output_latency" : "47ns",
    "xbar_bw" : "100Gb/s",
})

sst.addGlobalParams("topology_params", {
    "num_ports" : "3"
})

## DEFINE ZQM ##
# sst.Component(name here, type - (sub)component name from ELI)
zqm_module = sst.Component("zqm_module", "forzazqm.ZQM")
zqm_module.addParams({
    "clockFreq" : "2GHz",
    "clockTicks" : 1000,
    "numCores" : 1,
    "numHarts" : 16,
    "precinctId" : 0,
    "zoneId" : 1
    #    "debug" : DEBUG,
    #    "debug_level" : DEBUG,
    #    "verbose" : VERBOSE,
})

# sst.setSubComponent(slot_name (ELI), type (ELI), slot_index=0)
# ZQM.zone_nic
m_zop_iface = zqm_module.setSubComponent("zone_nic", "Forza.zenZopNIC", 0)
m_zop_iface.addGlobalParamSet("networkLinkControl_params")
# ZQM.zone_nic seems to have a "hidden" SubComponent
mzopiface_lc = m_zop_iface.setSubComponent("iface", "merlin.linkcontrol", 0)
mzopiface_lc.addGlobalParamSet("networkLinkControl_params")

## DEFINE ZOPGEN ##
zopgen = sst.Component("zopgen", "Forza.ZOPGen")
zopgen.addParams({"int_id" : 15})
zopgen_lc = zopgen.setSubComponent("m_zop_iface", "Forza.zenZopNIC", 0)
zopgen_lc.addGlobalParamSet("networkLinkControl_params")
zopgen_lc.addParams({"verbose": 10})
zopgeniface_lc = zopgen_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
zopgeniface_lc.addGlobalParamSet("networkLinkControl_params")

## DEFINE ZONE ROUTER ##
router = sst.Component("router", "merlin.hr_router")
router.addGlobalParamSet("router_params") # These params were set above
router.addParams({"id": 0, "num_ports": 2}) # Ports is 4 in zen-test - 2 zaps, rza (mem), zop gen
merlin_topo = router.setSubComponent("topology", "merlin.singlerouter", 0)
merlin_topo.addGlobalParamSet("topology_params")

## DEFINE PRECINCT ROUTER (Necessary?) ##
#prec_router = sst.Component("prec_router", "merlin.hr_router")
#prec_router.addGlobalParamSet("router_params")
#prec_router.addParams({"id": 1, "num_ports": 1})

#prec_topo = prec_router.setSubComponent("topology", "merlin.singlerouter", 0)
#prec_topo.addGlobalParamSet("topology_params")


# Enable statistics - do later
#sst.setStatisticLoadLevel(10)
#sst.setStatisticOutput("sst.statOutputConsole")
#sst.enableAllStatisticsForComponentType("memHierarchy.standardCPU")

## TODO: DEFINE LINKS BETWEEN MODULES ##
# Link between zone router and mzopiface_lc (part of the zone_nic in the zqm)
zqm_router_link = sst.Link("zqm_router_link")
zqm_router_link.connect( (mzopiface_lc, "router_port", "1us"), (router, "port1", "1us") )

# Link between zone router and zopgeniface_lc (part of the zopgen_lc inside of ZOPGen)
zopgen_router_link = sst.Link("zopgen_router_link")
zopgen_router_link.connect( (zopgeniface_lc, "router_port", "1us"), (router, "port0", "1us") )