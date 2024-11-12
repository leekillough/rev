#!/usr/bin/env python
#
# FORZA PYTHON DRIVER
#

# import sys
import sst
import argparse
# import array as arr
# from sst.merlin.base import *
# from sst.merlin.endpoint import *
# from sst.merlin.topology import *
# from sst.merlin.interface import *
# from sst.merlin.router import *

# Configurable Simulation Parameters
# Full Scale FORZA - 4608 Precincts
# shape="24,24:24,24:8"
# Small system
# shape = "2,2:4"
HFIEndpoints = []
shape = ""
host_link_latency = None
link_latency = "100ns"
rtr_prefix = "cornelis_"

topo_params = {
    "routing_alg": "deterministic",
    "adaptive_threshold": 0.5
}

router_params = {
    "flit_size": "8B",
    "input_buf_size": "14kB",
    "input_latency": "47ns",
    "link_bw": "400Gb/s",
    "num_vns": "1",
    "output_buf_size": "14kB",
    "output_latency": "47ns",
    "xbar_bw": "400Gb/s",
}

precinct_params = {
    "eg_queue_size": 0,
    "new_scale": 11,
    "old_scale": 11,
}

networkLinkControl_params = {
    "input_buf_size": "14kB",
    "job_id": "0",
    "job_size": "2",
    "link_bw": "400Gb/s",
    "output_buf_size": "14kB",
    "use_nid_remap": "False",
}

zone_id = 0
zap_id = 0
pe_id = 0
ups = []
downs = []
routers_per_level = []
groups_per_level = []
start_ids = []
total_hosts = 1


def getRouterNameForId(rtr_id):
    num_levels = len(start_ids)

    # Check to make sure the index is in range
    level = num_levels - 1
    if rtr_id >= (start_ids[level] + routers_per_level[level]) or rtr_id < 0:
        print("ERROR: topoFattree.getRouterNameForId: rtr_id not found: %d" % rtr_id)
        sst.exit()

    # Find the level
    for x in range(num_levels-1, 0, -1):
        if rtr_id >= start_ids[x]:
            break
        level = level - 1

    # Find the group
    remainder = rtr_id - start_ids[level]
    routers_per_group = routers_per_level[level] // groups_per_level[level]
    group = remainder // routers_per_group
    router = remainder % routers_per_group
    return getRouterNameForLocation((level, group, router))


def getRouterNameForLocation(location):
    return "%srtr_l%s_g%d_r%d" % (rtr_prefix, location[0], location[1], location[2])


# Construct HR_Router SST Object
def instanciateRouter(radix,  rtr_id):
    rtr = sst.Component(getRouterNameForId(rtr_id), "merlin.hr_router")
    rtr.addGlobalParamSet("router_params")
    rtr.addParam("num_ports", radix)
    rtr.addParam("id", rtr_id)
    return rtr


# Construct Endpoints
def instanciateEndpoint(id, additionalParams={}):
    zipComp = sst.Component("zip"+str(id), "ForzaZIP.ZIP")
    zipComp.addParams({"precID": id, "maxWait": "1us"})

    zipMem = zipComp.setSubComponent("memory", "ForzaZIP.ZIPBasicMemCtrl", 0)
    zipMemIface = zipMem.setSubComponent("memIface", "memHierarchy.standardInterface", 0)

    zipMemCtrl = sst.Component("zipMemCtrl"+str(id), "memHierarchy.MemController")
    zipMemCtrl.addParams({"clock": "2GHz", "addr_range_start": 0, "backing": "malloc"})
    zipMemBackend = zipMemCtrl.setSubComponent("backend", "memHierarchy.simpleMem")
    zipMemBackend.addParams({"access_time": "100ns", "mem_size": "8GB"})

    zipMemLink = sst.Link("zipMemLink"+str(id))
    zipMemLink.connect((zipMemIface, "port", "50ps"), (zipMemCtrl, "direct_link", "50ps"))

    zipNIC = zipComp.setSubComponent("zopLink", "forza.zopNIC", 1)
    zipNIC_lc = zipNIC.setSubComponent("iface", "merlin.linkcontrol", 0)
    zipNIC_lc.addGlobalParamSet("networkLinkControl_params")
    zipNIC.addParams({"precinctID": id})

    noc = sst.Component("precinctNOC"+str(id), "merlin.hr_router")
    noc.addGlobalParamSet("router_params")
    noc.addParams({"id": 4608+id, "num_ports": 2})
    noc.setSubComponent("topology", "merlin.singlerouter", 0)

    zipNICLink = sst.Link("zipNICLink"+str(id))
    zipNICLink.connect((zipNIC_lc, "rtr_port", "1us"), (noc, "port0", "1us"))

    zipHFINIC = zipComp.setSubComponent("hfiLink", "ForzaZIP.ZIPHFINIC", 2)
    zipComp_lc = zipHFINIC.setSubComponent("iface", "merlin.linkcontrol", 0)
    zipComp_lc.addGlobalParamSet("networkLinkControl_params")
    zipHFINIC.addParams({"precinctID": id})

    return zipComp, zipComp_lc


def findRouterByLocation(location):
    return sst.findComponentByName(getRouterNameForLocation(location))


# Process the shape
def calculateTopo():
    global shape
    global ups
    global downs
    global routers_per_level
    global groups_per_level
    global start_ids
    global total_hosts
    levels = shape.split(":")

    for lvl in levels:
        links = lvl.split(",")
        downs.append(int(links[0]))
        if len(links) > 1:
            ups.append(int(links[1]))

    for i in downs:
        total_hosts *= i

    routers_per_level = [0] * len(downs)
    routers_per_level[0] = total_hosts // downs[0]
    for i in range(1, len(downs)):
        routers_per_level[i] = routers_per_level[i-1] * ups[i-1] // downs[i]

    start_ids = [0] * len(downs)
    for i in range(1, len(downs)):
        start_ids[i] = start_ids[i-1] + routers_per_level[i-1]

    groups_per_level = [1] * len(downs)
    if ups:  # if ups is empty, then this is a single level and the following line will fail
        groups_per_level[0] = total_hosts // downs[0]

    for i in range(1, len(downs)-1):
        groups_per_level[i] = groups_per_level[i-1] // downs[i]

    print("Total hosts: " + str(total_hosts))
    print("Routers per level: " + str(routers_per_level))
    print("Downs: " + str(downs))
    print("Ups: " + str(ups))


def buildTopo():
    global host_link_latency
    if not host_link_latency:
        host_link_latency = link_latency

    # Recursive function to build levels
    def fattree_rb(level, group, links):
        id = start_ids[level] + group * (routers_per_level[level]//groups_per_level[level])

        host_links = []
        if level == 0:
            # create all the nodes
            for i in range(downs[0]):
                node_id = id * downs[0] + i
                print("Building endpoint " + str(node_id))
                ep_lc = HFIEndpoints[node_id]
                plink = sst.Link("precinctlink_%d" % node_id)
                ep_lc.addLink(plink, "rtr_port", host_link_latency)
                host_links.append(plink)

            # Create the edge router
            rtr_id = id
            rtr = instanciateRouter(ups[0] + downs[0], rtr_id)
            topology = rtr.setSubComponent("topology", "merlin.fattree")
            topology.addParams(topo_params)
            topology.addParams({"shape": shape})
            # Add links
            for lvl in range(len(host_links)):
                rtr.addLink(host_links[lvl], "port%d" % lvl, link_latency)
            for lvl in range(len(links)):
                rtr.addLink(links[lvl], "port%d" % (lvl+downs[0]), link_latency)
            return

        rtrs_in_group = routers_per_level[level] // groups_per_level[level]
        # Create the down links for the routers
        rtr_links = [[] for index in range(rtrs_in_group)]
        for i in range(rtrs_in_group):
            for j in range(downs[level]):
                rtr_links[i].append(sst.Link("link_l%d_g%d_r%d_p%d" % (level, group, i, j)))

        # Now create group links to pass to lower level groups from router down links
        group_links = [[] for index in range(downs[level])]
        for i in range(downs[level]):
            for j in range(rtrs_in_group):
                group_links[i].append(rtr_links[j][i])

        for i in range(downs[level]):
            fattree_rb(level-1, group*downs[level]+i, group_links[i])

        # Create the routers in this level.
        # Start by adding up links to rtr_links
        for i in range(len(links)):
            rtr_links[i % rtrs_in_group].append(links[i])

        for i in range(rtrs_in_group):
            rtr_id = id + i
            rtr = instanciateRouter(ups[level] + downs[level], rtr_id)
            topology = rtr.setSubComponent("topology", "merlin.fattree")
            topology.addParams(topo_params)
            topology.addParams({"shape": shape})
            # Add links
            for lvl in range(len(rtr_links[i])):
                rtr.addLink(rtr_links[i][lvl], "port%d" % lvl, link_latency)
    #  End recursive function

    level = len(ups)
    if ups:  # True for all cases except for single level
        #  Create the router links
        rtrs_in_group = routers_per_level[level] // groups_per_level[level]

        # Create the down links for the routers
        rtr_links = [[] for index in range(rtrs_in_group)]
        for i in range(rtrs_in_group):
            for j in range(downs[level]):
                rtr_links[i].append(sst.Link("link_l%d_g0_r%d_p%d" % (level, i, j)))

        # Now create group links to pass to lower level groups from router down links
        group_links = [[] for index in range(downs[level])]
        for i in range(downs[level]):
            for j in range(rtrs_in_group):
                group_links[i].append(rtr_links[j][i])

        for i in range(downs[len(ups)]):
            fattree_rb(level-1, i, group_links[i])

        # Create the routers in this level
        radix = downs[level]
        for i in range(routers_per_level[level]):
            rtr_id = start_ids[len(ups)] + i
            rtr = instanciateRouter(radix, rtr_id)

            topology = rtr.setSubComponent("topology", "merlin.fattree")
            topology.addParams(topo_params)
            topology.addParams({"shape": shape})

            for lvl in range(len(rtr_links[i])):
                rtr.addLink(rtr_links[i][lvl], "port%d" % lvl, link_latency)
#    else:  # Single level case
        # Commented out since node_id was defined as unused
        # create all the nodes
#        for i in range(downs[0]):
#            node_id = i

    rtr_id = 0


class FORZA:
    def __init__(self, name="FORZA", zones=4, precincts=1,
                 zapsPerZone=4, hartsPerZap=1, clock="1GHz",
                 program="test.exe", progArgs="", memSize=1073741823,
                 memAccessTime="100ns", reqPerCycle=1,
                 inputBufSize="2048B", outputBufSize="2048B",
                 linkBW="100GB/s", flitSize="8B", linkLatency="100ns",
                 xbarBW="800GB/s", verbose="5"):
        print("Initializing FORZA")
        self.name = name
        self.zones = zones
        self.precincts = precincts
        self.zapsPerZone = zapsPerZone
        self.hartsPerZap = hartsPerZap
        self.clock = clock
        self.program = program
        self.progArgs = progArgs
        self.memSize = memSize
        self.memAccessTime = memAccessTime
        self.reqPerCycle = reqPerCycle
        self.inputBufSize = inputBufSize
        self.outputBufSize = outputBufSize
        self.linkBW = linkBW
        self.flitSize = flitSize
        self.linkLatency = linkLatency
        self.xbarBW = xbarBW
        self.verbose = verbose
        # avoid multiple zaps conflicting address spaces
        if self.zapsPerZone > 1:
            self.memSizeChange = True
        else:
            self.memSizeChange = False

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
        sst.enableAllStatisticsForComponentType("ForzaZIP.ZIP")

    def build(self):

        ZipArray = []

        for i in range(self.precincts):
            # -- create the precinct router
            prec_router = sst.Component("prec_router_"+str(i), "merlin.hr_router")
            prec_router.setSubComponent("topology", "merlin.singlerouter")
            prec_router.addParams({
              "input_buf_size": self.inputBufSize,
              "output_buf_size": self.outputBufSize,
              "link_bw": self.linkBW,
              "xbar_bw": self.xbarBW,
              "flit_size": self.flitSize,
              "num_ports": self.zones+1,
              "id": 0
            })

            # -- create the zip
            zipComp = sst.Component("zip"+str(i), "ForzaZIP.ZIP")
            zipComp.addParams({"precID": i, "maxWait": "1us"})

            zipMem = zipComp.setSubComponent("memory", "ForzaZIP.ZIPBasicMemCtrl", 0)
            zipMemIface = zipMem.setSubComponent("memIface", "memHierarchy.standardInterface", 0)

            zipMemCtrl = sst.Component("zipMemCtrl_"+str(i), "memHierarchy.MemController")
            zipMemCtrl.addParams({"clock": "2GHz", "addr_range_start": 0, "backing": "malloc"})
            zipMemBackend = zipMemCtrl.setSubComponent("backend", "memHierarchy.simpleMem")
            zipMemBackend.addParams({"access_time": "100ns", "mem_size": "8GB"})

            zipMemLink = sst.Link("zipMemLink_"+str(i))
            zipMemLink.connect((zipMemIface, "port", "50ps"), (zipMemCtrl, "direct_link", "50ps"))

            zipNIC = zipComp.setSubComponent("zopLink", "forza.zopNIC", 1)
            zipNIC.addParams({"precinctID": i})
            zipNIC_lc = zipNIC.setSubComponent("iface", "merlin.linkcontrol", 0)

            zipNIC.addParams({
              "verbose": self.verbose,
              "clock": self.clock,
              "req_per_cycle": self.reqPerCycle,
              "numZones": self.zones,
              "numPrecincts": self.precincts
            })
            zipNIC_lc.addParams({
              "input_buf_size": self.inputBufSize,
              "output_buf_size": self.outputBufSize,
              "link_bw": self.linkBW,
            })

            zipNICLink = sst.Link("zipNICLink_"+str(i))
            zipNICLink.connect((zipNIC_lc, "rtr_port", "1us"), (prec_router, "port"+str(self.zones), "1us"))

            zipHFINIC = zipComp.setSubComponent("hfiLink", "ForzaZIP.ZIPHFINIC", 2)
            zipHFINIC.addParams({"precinctID": i})
            zipComp_lc = zipHFINIC.setSubComponent("iface", "merlin.linkcontrol", 0)
            zipComp_lc.addParams({
              "input_buf_size": "14kB",
              "job_id": "0",
              "job_size": "2",
              "link_bw": "400Gb/s",
              "output_buf_size": "14kB",
              "use_nid_remap": "False",
            })

            # -- store off the ZIP linkcontrol object in an array for future topology connectivity
            ZipArray.append(zipComp_lc)

            for j in range(self.zones):
                # -- create the zone router
                zone_router = sst.Component("zone_router_"+str(i)+"_"+str(j), "merlin.hr_router")
                zone_router.setSubComponent("topology", "merlin.singlerouter")
                zone_router.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                  "xbar_bw": self.xbarBW,
                  "flit_size": self.flitSize,
                  "num_ports": self.zapsPerZone+3,
                  "id": 0
                })
                # -- create the zone ring router
                zring_router = sst.Component("zring_router_"+str(i)+"_"+str(j), "merlin.hr_router")
                zring_router.setSubComponent("topology", "merlin.singlerouter")
                zring_router.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                  "xbar_bw": self.xbarBW,
                  "flit_size": self.flitSize,
                  "num_ports": self.zapsPerZone+2,
                  "id": 0
                })
                # -- create the ZQM
                zqm = sst.Component("zqm_"+str(i)+"_"+str(j), "forzazqm.ZQM")
                zqm.addParams({
                  "verbose": self.verbose,
                  "clockFreq": self.clock,
                  "precinctId": i,
                  "zoneId": j,
                  "numHarts": self.hartsPerZap,
                  "numCores": self.zapsPerZone
                })
                #  zone noc
                zqm_nic = zqm.setSubComponent("zone_nic", "forza.zopNIC")
                zqm_iface = zqm_nic.setSubComponent("iface", "merlin.linkcontrol")
                zqm_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle,
                  "numZones": self.zones,
                  "numPrecincts": self.precincts
                })
                zqm_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zqm_link = sst.Link("zqm_link_"+str(i)+"_"+str(j))
                zqm_link.connect((zqm_iface, "rtr_port", "1us"),
                                 (zone_router, "port"+str(self.zapsPerZone), "1us"))
                #  zring
                zqm_ring_nic = zqm.setSubComponent("ring_nic", "forza.RingNetNIC")
                zqm_ring_iface = zqm_ring_nic.setSubComponent("iface", "merlin.linkcontrol")
                zqm_ring_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle
                })
                zqm_ring_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zqm_ring_link = sst.Link("zqm_ring_link_"+str(i)+"_"+str(j))
                zqm_ring_link.connect((zqm_ring_iface, "rtr_port", "1us"),
                                      (zring_router, "port"+str(self.zapsPerZone), "1us"))

                # -- create the ZEN
                zen = sst.Component("zen_"+str(i)+"_"+str(j), "forzazen.ZEN")
                zen.addParams({
                  "verbose": 7,  # self.verbose,
                  "clockFreq": self.clock,
                  "precinctId": i,
                  "zoneId": j,
                  "numHarts": self.hartsPerZap,
                  "numZap": self.zapsPerZone,
                  "numZones": self.zones,
                  "numPrecincts": self.precincts,
                  "enableDMA": 0,
                  "zenQSizeLimit": 100000,
                  "processPerCycle": 100000
                })
                zen_nic = zen.setSubComponent("zone_nic", "forza.zopNIC")
                zen_iface = zen_nic.setSubComponent("iface", "merlin.linkcontrol")
                zen_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle,
                  "numZones": self.zones,
                  "numPrecincts": self.precincts
                })
                zen_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zen_link = sst.Link("zen_link_"+str(i)+"_"+str(j))
                zen_link.connect((zen_iface, "rtr_port", "1us"),
                                 (zone_router, "port"+str(self.zapsPerZone+1), "1us"))

                zen_prec_nic = zen.setSubComponent("precinct_nic", "forza.zopNIC")
                zen_prec_iface = zen_prec_nic.setSubComponent("iface", "merlin.linkcontrol")
                zen_prec_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle
                })
                zen_prec_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zen_prec_link = sst.Link("zen_prec_link_"+str(i)+"_"+str(j))
                zen_prec_link.connect((zen_prec_iface, "rtr_port", "1us"),
                                      (prec_router, "port"+str(j), "1us"))

                #  zring
                zen_ring_nic = zen.setSubComponent("ring_nic", "forza.RingNetNIC")
                zen_ring_iface = zen_ring_nic.setSubComponent("iface", "merlin.linkcontrol")
                zen_ring_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle
                })
                zen_ring_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zen_ring_link = sst.Link("zen_ring_link_"+str(i)+"_"+str(j))
                zen_ring_link.connect((zen_ring_iface, "rtr_port", "1us"),
                                      (zring_router, "port"+str(self.zapsPerZone+1), "1us"))

                # -- create the RZA
                rza = sst.Component("rza_"+str(i)+"_"+str(j), "revcpu.RevCPU")
                if self.memSizeChange:
                    memSizeRza = self.memSize + 1024*1024*200*self.zapsPerZone
                else:
                    memSizeRza = self.memSize
                rza.addParams({
                  "verbose": self.verbose,
                  "numCores": 2,
                  "clock": self.clock,
                  "memSize": memSizeRza,
                  "machine": "[CORES:RV64G]",
                  "program": self.program,
                  "args": self.progArgs,
                  "enableZoneNIC": 1,
                  "enableRZA": 1,
                  "precinctId": i,
                  "zoneId": j,
                  "enableMemH": 1,
                  "splash": 0
                })
                rza_lspipe = rza.setSubComponent("rza_ls", "revcpu.RZALSCoProc")
                rza_lspipe.addParams({
                  "clock": self.clock,
                  "verbose": self.verbose
                })
                rza_amopipe = rza.setSubComponent("rza_amo", "revcpu.RZAAMOCoProc")
                rza_amopipe.addParams({
                  "clock": self.clock,
                  "verbose": self.verbose
                })
                rza_lsq = rza.setSubComponent("memory", "revcpu.RevBasicMemCtrl")
                rza_lsq.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "max_loads": 16,
                  "max_stores": 16,
                  "max_flush": 16,
                  "max_llsc": 16,
                  "max_readlock": 16,
                  "max_writeunlock": 16,
                  "max_custom": 16,
                  "ops_per_cycle": 16
                })
                rza_lsq_iface = rza_lsq.setSubComponent("memIface", "memHierarchy.standardInterface")
                rza_lsq_iface.addParams({
                  "verbose": self.verbose
                })
                memctrl = sst.Component("memory_"+str(i)+"_"+str(j), "memHierarchy.MemController")
                memctrl.addParams({
                  "debug": 0,
                  "debug_level": 0,
                  "clock": self.clock,
                  "verbose": self.verbose,
                  "addr_range_start": 0,
                  "addr_range_end": memSizeRza,
                  "backing": "malloc"
                })
                backing_memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
                backing_memory.addParams({
                  "access_time": self.memAccessTime,
                  "mem_size": str(memSizeRza)+"B"
                })
                rza_mem_link = sst.Link("rza_mem_link_"+str(i)+"_"+str(j))
                rza_mem_link.connect((rza_lsq_iface, "port", "50ps"), (memctrl, "direct_link", "50ps"))
                rza_nic = rza.setSubComponent("zone_nic", "forza.zopNIC")
                rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")
                rza_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle,
                  "numZones": self.zones,
                  "numPrecincts": self.precincts
                })
                rza_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })
                rza_link = sst.Link("rza_link_"+str(i)+"_"+str(j))
                rza_link.connect((rza_iface, "rtr_port", "1us"),
                                 (zone_router, "port"+str(self.zapsPerZone+2), "1us"))
                # -- create the ZAPS
                for k in range(self.zapsPerZone):
                    zap = sst.Component("zap_"+str(i)+"_"+str(j)+"_"+str(k), "revcpu.RevCPU")
                    if self.memSizeChange:
                        memSizeZap = self.memSize + 1024*1024*200*k
                    else:
                        memSizeZap = self.memSize
                    zap.addParams({
                      "verbose": self.verbose,
                      # "verbose": 16,
                      "numCores": 1,
                      "numHarts": self.hartsPerZap,
                      "clock": self.clock,
                      # "memSize": self.memSize,
                      "memSize": memSizeZap,
                      # if this works will need to change it for
                      # multi zone/precinct designs
                      # "memSize": (self.memSize + 1024*1024*200*k),
                      "machine": "[CORES:RV64GC]",
                      # "startAddr": "[CORES:0x00000000]",
                      "program": self.program,
                      "args": self.progArgs,
                      "enableZoneNIC": 1,
                      "enableRZA": 0,
                      "precinctId": i,
                      "zoneId": j,
                      "zapId": k,
                      "splash": 0
                    })
                    zap_nic = zap.setSubComponent("zone_nic", "forza.zopNIC")
                    zap_iface = zap_nic.setSubComponent("iface", "merlin.linkcontrol")
                    zap_nic.addParams({
                      "verbose": self.verbose,
                      "clock": self.clock,
                      "req_per_cycle": self.reqPerCycle,
                      "numZones": self.zones,
                      "numPrecincts": self.precincts
                    })
                    zap_iface.addParams({
                      "input_buf_size": self.inputBufSize,
                      "output_buf_size": self.outputBufSize,
                      "link_bw": self.linkBW,
                    })
                    zap_link = sst.Link("zap_link_"+str(i)+"_"+str(j)+"_"+str(k))
                    zap_link.connect((zap_iface, "rtr_port", "1us"),
                                     (zone_router, "port"+str(k), "1us"))
                    # zring
                    zap_ring_nic = zap.setSubComponent("ring_nic", "forza.RingNetNIC")
                    zap_ring_iface = zap_ring_nic.setSubComponent("iface", "merlin.linkcontrol")
                    zap_ring_nic.addParams({
                        "verbose": self.verbose,
                        "clock": self.clock,
                        "req_per_cycle": self.reqPerCycle
                    })
                    zap_ring_iface.addParams({
                        "input_buf_size": self.inputBufSize,
                        "output_buf_size": self.outputBufSize,
                        "link_bw": self.linkBW,
                    })

                    zap_ring_link = sst.Link("zap_ring_link_"+str(i)+"_"+str(j)+"_"+str(k))
                    zap_ring_link.connect((zap_ring_iface, "rtr_port", "1us"),
                                          (zring_router, "port"+str(k), "1us"))

        return ZipArray


if __name__ == "__main__":
    # -- parse the args
    ap = argparse.ArgumentParser()
    ap.add_argument("-a", "--hartsperzap", required=True, help="sets the number of harts per zap")
    ap.add_argument("-z", "--zaps", required=True, help="sets the number of zaps per zone")
    ap.add_argument("-o", "--zones", required=True, help="sets the number of zones per precinct")
    ap.add_argument("-p", "--precincts", required=True, help="sets the number of precincts")
    ap.add_argument("-r", "--program", required=True, help="sets the target program exe")
    ap.add_argument("-s", "--shape", required=True, help="sets the shape of the system network")
    ap.add_argument("-g", "--progargs", default="", required=False, help="sets the arguments for the program ran by the zaps")
    args = vars(ap.parse_args())
    # -- this argument is only used for the merlin network
    shape = args['shape']
    print("Running with program "+str(args['program'])+" with args: "+args['progargs'])
    # -- build all the inner-precinct logic
    f = FORZA(zones=int(args['zones']), precincts=int(args['precincts']),
              zapsPerZone=int(args['zaps']), hartsPerZap=int(args['hartsperzap']),
              program=args['program'], progArgs=args['progargs'])
    HFIEndpoints = f.build()
    # -- build the system network
    sst.addGlobalParams("router_params", router_params)
    sst.addGlobalParams("networkLinkControl_params", networkLinkControl_params)
    calculateTopo()
    buildTopo()

# -- EOF
