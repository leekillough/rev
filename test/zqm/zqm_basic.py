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

# TODO: Place holder driver for sending commands into the CxlMemElement
#cpu = sst.Component("core", "memHierarchy.standardCPU")
#cpu.addParams({
#    "memFreq" : 100,
#    "memSize" : "512MiB",
#    "clock" : "1GHz",
#    "maxOutstanding" : 10,
#    "opCount" : 1000,
#    "write_freq" : 25,
#    "read_freq" : 75,
#})
#iface = cpu.setSubComponent("memory", "memHierarchy.standardInterface")

# sst.Component(name here, type - (sub)component name from ELI)
zqm_module = sst.Component("zqm_module", "zqm.ZQM")
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
# This should be what's handling the link...I think...
#memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
#memory.addParams({
#    "access_time" : "1000ns",
#    "mem_size" : "512MiB"
#})

# TODO: Eventually add a memory backend

# Enable statistics - do later
#sst.setStatisticLoadLevel(10)
#sst.setStatisticOutput("sst.statOutputConsole")
#sst.enableAllStatisticsForComponentType("memHierarchy.standardCPU")
#sst.enableAllStatisticsForComponentType("memHierarchy.standardInterface")
#sst.enableAllStatisticsForComponentType("memHierarchy.Cache")
#sst.enableAllStatisticsForComponentType("memHierarchy.MemController")
#sst.enableAllStatisticsForComponentType("memHierarchy.simpleMem")


# Define the simulation links
#link_driver_cxl_link = sst.Link("link_driver_cxl_link")
#link_driver_cxl_link.connect( (iface, "port", "1000ps"), (cxlmem_module, "bus", "1000ps") )