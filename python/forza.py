#!/usr/bin/env python
#
# FORZA PYTHON DRIVER
#

import sys
import sst
import argparse
from sst.merlin.base import *

class FORZA:
  def __init__(self, name = "FORZA", zones = 4, precincts = 1,
               zapsPerZone = 4, hartsPerZap = 1, clock = "1GHz",
               program = "test.exe", memSize = 1073741823,
               memAccessTime = "100ns", reqPerCycle = 1,
               inputBufSize = "2048B", outputBufSize = "2048B",
               linkBW = "100GB/s", flitSize = "8B", linkLatency = "1us",
               xbarBW = "800GB/s", verbose = "5"):
    print("Initializing FORZA")
    self.name           = name
    self.zones          = zones
    self.precincts      = precincts
    self.zapsPerZone    = zapsPerZone
    self.hartsPerZap    = hartsPerZap
    self.clock          = clock
    self.program        = program
    self.memSize        = memSize
    self.memAccessTime  = memAccessTime
    self.reqPerCycle    = reqPerCycle
    self.inputBufSize   = inputBufSize
    self.outputBufSize  = outputBufSize
    self.linkBW         = linkBW
    self.flitSize       = flitSize
    self.linkLatency    = linkLatency
    self.xbarBW         = xbarBW
    self.verbose        = verbose

  @property
  def name(self):
    return self._name

  @name.setter
  def name(self, value):
    self._name = value

  @property
  def zones(self):
    return self._zones

  @zones.setter
  def zones(self, value):
    self._zones = value

  @property
  def precincts(self):
    return self._precincts

  @precincts.setter
  def precincts(self, value):
    self._precincts = value

  @property
  def zapsPerZone(self):
    return self._zapsPerZone

  @zapsPerZone.setter
  def zapsPerZone(self, value):
    self._zapsPerZone = value

  @property
  def hartsPerZap(self):
    return self._hartsPerZap

  @hartsPerZap.setter
  def hartsPerZap(self, value):
    self._hartsPerZap = value

  @property
  def clock(self):
    return self._clock

  @clock.setter
  def clock(self, value):
    self._clock = value

  @property
  def program(self):
    return self._program

  @program.setter
  def program(self, value):
    self._program = value

  @property
  def memSize(self):
    return self._memSize

  @memSize.setter
  def memSize(self, value):
    self._memSize = value

  @property
  def memAccessTime(self):
    return self._memAccessTime

  @memAccessTime.setter
  def memAccessTime(self, value):
    self._memAccessTime = value

  @property
  def reqPerCycle(self):
    return self._reqPerCycle

  @reqPerCycle.setter
  def reqPerCycle(self, value):
    self._reqPerCycle = value

  @property
  def inputBufSize(self):
    return self._inputBufSize

  @inputBufSize.setter
  def inputBufSize(self, value):
    self._inputBufSize = value

  @property
  def outputBufSize(self):
    return self._outputBufSize

  @outputBufSize.setter
  def outputBufSize(self, value):
    self._outputBufSize = value

  @property
  def linkBW(self):
    return self._linkBW

  @linkBW.setter
  def linkBW(self, value):
    self._linkBW = value

  @property
  def flitSize(self):
    return self._flitSize

  @flitSize.setter
  def flitSize(self, value):
    self._flitSize = value

  @property
  def linkLatency(self):
    return self._linkLatency

  @linkLatency.setter
  def linkLatency(self, value):
    self._linkLatency = value

  @property
  def xbarBW(self):
    return self._xbarBW

  @xbarBW.setter
  def xbarBW(self, value):
    self._xbarBW = value

  @property
  def verbose(self):
    return self._verbose

  @verbose.setter
  def verbose(self, value):
    self._verbose = value

  def enableStatistics(self, level):
    print("Enabling Statistics")
    sst.setStatisticOutput("sst.statOutputCSV")
    sst.enableAllStatisticsForComponentType("revcpu.RevCPU")
    sst.enableAllStatisticsForComponentType("revcpu.RZAAMOCoProc")
    sst.enableAllStatisticsForComponentType("revcpu.RZALSCoProc")
    sst.enableAllStatisticsForComponentType("revcpu.RevBasicMemCtrl")
    sst.enableAllStatisticsForComponentType("memHierarchy.MemController")
    sst.enableAllStatisticsForComponentType("memHierarchy.simpleMem")
    sst.enableAllStatisticsForComponentType("forzazen.ZEN")
    sst.enableAllStatisticsForComponentType("forzazqm.ZQM")
    sst.enableAllStatisticsForComponentType("forza.zopNIC")

  def build(self):

    for i in range(self.precincts):
      #-- create the precinct router
      prec_router = sst.Component("prec_router_"+str(i), "merlin.hr_router")
      prec_router.setSubComponent("topology", "merlin.singlerouter")
      prec_router.addParams({
        "input_buf_size" : self.inputBufSize,
        "output_buf_size" : self.outputBufSize,
        "link_bw" : self.linkBW,
        "xbar_bw" : self.xbarBW,
        "flit_size" : self.flitSize,
        "num_ports" : self.zones,
        "id" : 0
      })

      for j in range(self.zones):
        #-- create the zone router
        zone_router = sst.Component("zone_router_"+str(i)+"_"+str(j), "merlin.hr_router")
        zone_router.setSubComponent("topology", "merlin.singlerouter")
        zone_router.addParams({
          "input_buf_size" : self.inputBufSize,
          "output_buf_size" : self.outputBufSize,
          "link_bw" : self.linkBW,
          "xbar_bw" : self.xbarBW,
          "flit_size" : self.flitSize,
          "num_ports" : self.zapsPerZone+3,
          "id" : 0
        })

        #-- create the ZQM
        zqm = sst.Component("zqm_"+str(i)+"_"+str(j), "forzazqm.ZQM")
        zqm.addParams({
          "verbose" : self.verbose,
          "clockFreq" : self.clock,
          "precinctId" : i,
          "zoneId" : j,
          "numHarts" : 1,
          "numCores" : 1
        })
        zqm_nic = zqm.setSubComponent("zone_nic", "forza.zopNIC")
        zqm_iface = zqm_nic.setSubComponent("iface", "merlin.linkcontrol")
        zqm_nic.addParams({
          "verbose" : self.verbose,
          "clock" : self.clock,
          "req_per_cycle" : self.reqPerCycle
        })
        zqm_iface.addParams({
          "input_buf_size" : self.inputBufSize,
          "output_buf_size" : self.outputBufSize,
          "link_bw" : self.linkBW,
        })

        zqm_link = sst.Link("zqm_link_"+str(i)+"_"+str(j))
        zqm_link.connect( (zqm_iface, "rtr_port", "1us"),
                          (zone_router, "port"+str(self.zapsPerZone), "1us") )

        #-- create the ZEN
        zen = sst.Component("zen_"+str(i)+"_"+str(j), "forzazen.ZEN")
        zen.addParams({
          "verbose" : self.verbose,
          "clockFreq" : self.clock,
          "precinctId" : i,
          "zoneId" : j,
          "numHarts" : self.hartsPerZap,
          "numZap" : self.zapsPerZone,
          "numZones" : self.zones,
          "numPrecincts" : self.precincts,
          "enableDMA" : 0,
          "zenQSizeLimit" : 100000,
          "processPerCycle" : 100000
        })
        zen_nic = zen.setSubComponent("zone_nic", "forza.zopNIC")
        zen_iface = zen_nic.setSubComponent("iface", "merlin.linkcontrol")
        zen_nic.addParams({
          "verbose" : self.verbose,
          "clock" : self.clock,
          "req_per_cycle" : self.reqPerCycle
        })
        zen_iface.addParams({
          "input_buf_size" : self.inputBufSize,
          "output_buf_size" : self.outputBufSize,
          "link_bw" : self.linkBW,
        })

        zen_link = sst.Link("zen_link_"+str(i)+"_"+str(j))
        zen_link.connect( (zen_iface, "rtr_port", "1us"),
                          (zone_router, "port"+str(self.zapsPerZone+1), "1us") )

        zen_prec_nic = zen.setSubComponent("precinct_nic", "forza.zopNIC")
        zen_prec_iface = zen_prec_nic.setSubComponent("iface", "merlin.linkcontrol")
        zen_prec_nic.addParams({
          "verbose" : self.verbose,
          "clock" : self.clock,
          "req_per_cycle" : self.reqPerCycle
        })
        zen_prec_iface.addParams({
          "input_buf_size" : self.inputBufSize,
          "output_buf_size" : self.outputBufSize,
          "link_bw" : self.linkBW,
        })

        zen_prec_link = sst.Link("zen_prec_link_"+str(i)+"_"+str(j))
        zen_prec_link.connect( (zen_prec_iface, "rtr_port", "1us"),
                               (prec_router, "port"+str(j), "1us") )

        #-- create the RZA
        rza = sst.Component("rza_"+str(i)+"_"+str(j), "revcpu.RevCPU")
        rza.addParams({
          "verbose" : self.verbose,
          "numCores" : 2,
          "clock" : self.clock,
          "memSize" : self.memSize,
          "machine" : "[CORES:RV64G]",
          "startAddr" : "[CORES:0x00000000]",
          "program" : self.program,
          "enableZoneNIC" : 1,
          "enableRZA" : 1,
          "precinctId" : i,
          "zoneId" : j,
          "enable_memH" : 1,
          "splash" : 0
        })
        rza_lspipe = rza.setSubComponent("rza_ls","revcpu.RZALSCoProc")
        rza_lspipe.addParams({
          "clock" : self.clock,
          "verbose" : self.verbose
        })
        rza_amopipe = rza.setSubComponent("rza_amo","revcpu.RZAAMOCoProc")
        rza_amopipe.addParams({
          "clock" : self.clock,
          "verbose" : self.verbose
        })
        rza_lsq = rza.setSubComponent("memory", "revcpu.RevBasicMemCtrl")
        rza_lsq.addParams({
          "verbose" : self.verbose,
          "clock" : self.clock,
          "max_loads"       : 16,
          "max_stores"      : 16,
          "max_flush"       : 16,
          "max_llsc"        : 16,
          "max_readlock"    : 16,
          "max_writeunlock" : 16,
          "max_custom"      : 16,
          "ops_per_cycle"   : 16
        })
        rza_lsq_iface = rza_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
        rza_lsq_iface.addParams({
          "verbose" : self.verbose
        })
        memctrl = sst.Component("memory_"+str(i)+"_"+str(j), "memHierarchy.MemController")
        memctrl.addParams({
          "debug" : 0,
          "debug_level" : 0,
          "clock" : self.clock,
          "verbose" : self.verbose,
          "addr_range_start" : 0,
          "addr_range_end" : self.memSize,
          "backing" : "malloc"
        })
        backing_memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
        backing_memory.addParams({
          "access_time" : self.memAccessTime,
          "mem_size" : str(self.memSize)+"B"
        })
        rza_mem_link = sst.Link("rza_mem_link_"+str(i)+"_"+str(j))
        rza_mem_link.connect( (rza_lsq_iface, "port", "50ps"), (memctrl, "direct_link", "50ps") )

        rza_nic = rza.setSubComponent("zone_nic", "forza.zopNIC")
        rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")
        rza_nic.addParams({
          "verbose" : self.verbose,
          "clock" : self.clock,
          "req_per_cycle" : self.reqPerCycle
        })
        rza_iface.addParams({
          "input_buf_size" : self.inputBufSize,
          "output_buf_size" : self.outputBufSize,
          "link_bw" : self.linkBW,
        })

        rza_link = sst.Link("rza_link_"+str(i)+"_"+str(j))
        rza_link.connect( (rza_iface, "rtr_port", "1us"),
                          (zone_router, "port"+str(self.zapsPerZone+2), "1us") )

        #-- create the ZAPS
        for k in range(self.zapsPerZone):
          zap = sst.Component("zap_"+str(i)+"_"+str(j)+"_"+str(k), "revcpu.RevCPU")
          zap.addParams({
            "verbose" : self.verbose,
            "numCores" : 1,
            "clock" : self.clock,
            "memSize" : self.memSize,
            "machine" : "[CORES:RV64GC]",
            "startAddr" : "[CORES:0x00000000]",
            "program" : self.program,
            "enableZoneNIC" : 1,
            "enableRZA" : 0,
            "precinctId" : i,
            "zoneId" : j,
            "zapId" : k,
            "splash" : 0
          })
          zap_nic = zap.setSubComponent("zone_nic", "forza.zopNIC")
          zap_iface = zap_nic.setSubComponent("iface", "merlin.linkcontrol")
          zap_nic.addParams({
            "verbose" : self.verbose,
            "clock" : self.clock,
            "req_per_cycle" : self.reqPerCycle
          })
          zap_iface.addParams({
            "input_buf_size" : self.inputBufSize,
            "output_buf_size" : self.outputBufSize,
            "link_bw" : self.linkBW,
          })
          zap_link = sst.Link("zap_link_"+str(i)+"_"+str(j)+"_"+str(k))
          zap_link.connect( (zap_iface, "rtr_port", "1us"),
                            (zone_router, "port"+str(k), "1us") )


if __name__ == "__main__":
  #-- parse the args
  ap = argparse.ArgumentParser()
  ap.add_argument("-a", "--hartsperzap", required=True, help="sets the number of harts per zap")
  ap.add_argument("-z", "--zaps", required=True, help="sets the number of zaps per zone")
  ap.add_argument("-o", "--zones", required=True, help="sets the number of zones per precinct")
  ap.add_argument("-p", "--precincts", required=True, help="sets the number of precincts")
  ap.add_argument("-r", "--program", required=True, help="sets the target program exe")
  args = vars(ap.parse_args())

  f = FORZA(zones=int(args['zones']), precincts=int(args['precincts']),
            zapsPerZone=int(args['zaps']), hartsPerZap=int(args['hartsperzap']),
            program=args['program'])

  f.build()

#-- EOF

