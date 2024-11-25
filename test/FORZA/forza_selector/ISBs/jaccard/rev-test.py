#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#
# rev-test.py
#

# import os
import sst

# Define SST core options
sst.setProgramOption("timebase", "1ps")

# Tell SST what statistics handling we want
sst.setStatisticLoadLevel(8)

max_addr_gb = 1

# Define the simulation components
comp_cpu = sst.Component("cpu", "revcpu.RevCPU")
comp_cpu.addParams({
        "verbose": 1,                                # Verbosity
        "numCores": 1,                               # Number of cores
        "numHarts": 16,
        "clock": "4.0GHz",                           # Clock
        "memSize": 2*1024*1024*1024,                   # Memory size in bytes
        "machine": "[0:RV64IMAFDC]",                      # Core:Config; RV64I for core 0
        "startAddr": "[0:0x00000000]",               # Starting address for core 0
        "memCost": "[0:1:10]",                       # Memory loads required 1-10 cycles
        "args": "2 2",
        "program": "jaccard.exe",  # Target executable
        "splash": 1                                  # Display the splash message
})

# sst.setStatisticOutput("sst.statOutputCSV")
sst.enableAllStatisticsForAllComponents()

# EOF