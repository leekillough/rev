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
# SETUP THE ZAP
# --------------------------
zap_cpu1 = sst.Component("zap0", "revcpu.RevCPU")
zap_cpu1.addParams({
        "verbose": 5,                                # Verbosity
        "numCores": 1,                               # Number of cores
        "clock": "1.0GHz",                           # Clock
        "memSize": 1024*1024*1024,                   # Memory size in bytes
        "machine": "[0:RV64GC]",                      # Core:Config; RV64I for core 0
        "startAddr": "[0:0x00000000]",               # Starting address for core 0
        "memCost": "[0:1:10]",                       # Memory loads required 1-10 cycles
        "program": os.getenv("REV_EXE", "forza_send.exe"),  # Target executable
        "zoneId": 0,                                 # [FORZA] zone ID
        "zapId": 0,                                  # [FORZA] zap ID
        "splash": 0                                  # Display the splash message
})
