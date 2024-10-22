import sst
import pandas as pd
import argparse
import re
import math

base_tooling_dir = "/Users/afreund/sst-tooling"
csv_topology = base_tooling_dir + "/src/fattree/R48-8Pods/SSTFMT/Ftree_topo.csv"
topology = "CSV-Fattree"

urts_dir = base_tooling_dir + "/src/fattree/"
link_latency = "100ns"
rtr_prefix = "cornelis_"
core_switch_idx = 384
max_terminal = 24
max_local = 24

topo_params = {
    "topology": csv_topology,
    "useHierarchical": True,
    "core_switch_idx": core_switch_idx,
    "csrs": base_tooling_dir+"/configs/tree_r48_g8_ep4608_t3_Kl:1_vl1_CN5000/chip.%d.csr.csv",
    "4OportSelectLut": base_tooling_dir+"/configs/tree_r48_g8_ep4608_t3_Kl:1_vl1_CN5000/chip.%d.four_oport_select.csv",
    "max_terminal": max_terminal,
    # Max local used for aggrgate switches instead of terminal number.
    "max_local": max_local,
    "ScLut":  base_tooling_dir+"/configs/tree_r48_g8_ep4608_t3_Kl:1_vl1_CN5000/chip.%d.sc_map.csv",
    "ScModLut":  base_tooling_dir+"/configs/tree_r48_g8_ep4608_t3_Kl:1_vl1_CN5000/chip.%d.sc_mod.csv",
    "verbose_routers": "",
}

router_params = {
    "link_bw": "400Gb/s",
    "flit_size": "8B",
    "xbar_bw": "400Gb/s",
    "input_latency": "0ns",
    "output_latency": "0ns",
    "input_buf_size": "14kB",
    "output_buf_size": "14kB",
    "num_vns": 1,
}

precinct_params = {
    "eg_queue_size": 0,
    "new_scale": 11,
    "old_scale": 11,
}

networkLinkControl_params = {
   "link_bw": "400Gb/s",
   "input_buf_size": "14kB",
   "output_buf_size": "14kB",
   "rc_small": 0,
   "rc_large": 4,
   "rc_threshold": 12000,
   "cc_becn_enabled": False,
   "cc_becn_down_count": "50us",
   "cc_delay_enabled": False,
   "cc_delay_init": "50us",
   "cc_delay_reload_init": "60us",
   "cc_delay_reload_inc": "10us",
   "cc_delay_reload_dec": "20us",
}


def instanciateRouter(radix, rtr_id):
    rtr = sst.Component(getRouterNameForId(rtr_id), "cornelis.opa_router")
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


def getNumNodes_csv():
    df = pd.read_csv(csv_topology, header=None, skipinitialspace=True)
    df.columns = ["guid", "port", "type", "description", "guid1", "port1", "type1", "description1"]
    return int(df[df.type1 == "FI"].count()[0])


def getNumNodes():
    if topology == "CSV-Dragonfly":
        return getNumNodes_csv()
    elif topology == "CSV-Fattree":
        return getNumNodes_csv()
    elif topology == "CSV-Megafly":
        return getNumNodes_csv()
    else:
        e = Exception("Topology Type not supported")
        raise e


def getRouterNameForId(rtr_id):
    if topology == "CSV-Fattree":
        return getRouterNameForLocation(0, rtr_id)
    elif topology == "CSV-Dragonfly":
        return getRouterNameForLocation(0, rtr_id)
    elif topology == "CSV-Megafly":
        return getRouterNameForLocation(0, rtr_id)
    else:
        raise Exception("Topology Not Supported")


def getRouterNameForLocation(group, rtr):
    if topology == "CSV-Fattree":
        return "%srtr_ % d" % (rtr_prefix, rtr)
    elif topology == "CSV-Dragonfly":
        return "%srtr_%d" % (rtr_prefix, rtr)
    elif topology == "CSV-Megafly":
        return "%srtr_%d" % (rtr_prefix, rtr)


def findRouterByLocation(location, rtr):
    return sst.findComponentByName(getRouterNameForLocation(location, rtr))


def buildTopo():
    if topology == "CSV-Dragonfly":
        df = pd.read_csv(csv_topology, header=None, skipinitialspace=True)
        df.columns = ["guid", "port", "type", "description", "guid1", "port1", "type1", "description1"]

        if host_link_latency is None:  # noqa: F821
            host_link_latency = link_latency

        max_port = int(df.port.max())
        links = dict()

        def getLink(name, name2):
            if name not in links and name2 not in links:
                links[name] = sst.Link(name)
                # links[name] = name
            if name in links:
                return links[name]
            if name2 in links:
                ret = links[name2]
                del links[name2]
                return ret
        nic_num = 0

        for switch in df[df.type == "SW"].description.unique():
            router_num = int(re.findall(r'\d+', switch)[0])
            rtr = instanciateRouter(max_port+1, router_num)
            sub = rtr.setSubComponent(router.getTopologySlotName(), "cornelis.topoOpa400", 0)   # noqa: F821
            sub.addParams(topoOpa400)  # noqa: F821
            sub.addParam("urt0", urts_dir+"/Dfly_"+switch+"_urt0.csv")
            sub.addParam("urt1", urts_dir+"/Dfly_"+switch+"_urt1.csv")
            sub.addParam("urt2", urts_dir+"/Dfly_"+switch+"_urt2.csv")
            sub.addParam("urt3", urts_dir+"/Dfly_"+switch+"_urt3.csv")
            unconnected = list()
            links_list = list()
            ep_list = list()
            switch_df = df[df.description == switch]
            for port in range(0, max_port+1):
                portDf = switch_df[(switch_df.port == port)]
                # Check if port is connected
                if not portDf.empty:
                    if str(portDf[portDf.port == port].head().type1.values[0]) == "FI":
                        node_id = nic_num
                        ep_lc = HFIEndpoints[node_id]
                        plink = sst.Link("precinctlink_%d" % node_id)
                        ep_lc.addLink(plink, "rtr_port", host_link_latency)
                        host_links.append(plink)  # noqa: F821
                        ep_list.append(port)
                        nic_num = nic_num + 1
                    else:
                        linkName = "link_{guid}_{port}_{guid1}_{port1}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        linkName2 = "link_{guid1}_{port1}_{guid}_{port}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        # print(getLink(linkName, linkName2))
                        link = getLink(linkName, linkName2)
                        links_list.append(port)
                        rtr.addLink(link, "port%d" % port, self.link_latency)  # noqa: F821
                else:
                    unconnected.append(port)
            sub.addParam("ep_map", ep_list)
            sub.addParam("links_map", links_list)
            sub.addParam("unconnected_map", unconnected)
            # router_num = router_num + 1
    elif topology == "CSV-Fattree":
        df = pd.read_csv(csv_topology, header=None, skipinitialspace=True)
        # df.columns = df.columns.str.strip().str.lower().str.replace(' ', '_')
        # .str.replace('(', '').str.replace(')','').str.replace('.', '')
        df.columns = ["guid", "port", "type", "description", "guid1", "port1", "type1", "description1"]

        # if host_link_latency is None:
        host_link_latency = link_latency

        max_port = int(df.port.max())
        print("Max Port is: " + str(max_port))
        links = dict()

        def getLink(name, name2):
            if name not in links and name2 not in links:
                links[name] = sst.Link(name)
                # links[name] = name
            if name in links:
                return links[name]
            if name2 in links:
                ret = links[name2]
                del links[name2]
                return ret

        nic_num = 0

        for switch in df[df.type == "SW"].description.unique():
            router_num = int(re.findall(r'\d+', switch)[0])
            rtr = instanciateRouter(max_port+1, router_num)
            sub = rtr.setSubComponent("topology", "cornelis.topoOpa400", 0)
            sub.addParams(topo_params)
            sub.addParam("urt0", urts_dir+"/Ftree_"+switch+"_urt0.csv")
            sub.addParam("urt1", urts_dir+"/Ftree_"+switch+"_urt1.csv")
            sub.addParam("urt2", urts_dir+"/Ftree_"+switch+"_urt2.csv")
            sub.addParam("urt3", urts_dir+"/Ftree_"+switch+"_urt3.csv")

            if (router_num < int(core_switch_idx)):
                sub.addParam("switch_personality", (math.floor(router_num/8) % 2))
            else:
                sub.addParam("switch_personality", 2)

            unconnected = list()
            links_list = list()
            ep_list = list()
            switch_df = df[df.description == switch]
            for port in range(0, max_port+1):
                portDf = switch_df[(switch_df.port == port)]
                # Check if port is connected
                if not portDf.empty:
                    if str(portDf[portDf.port == port].head().type1.values[0]) == "FI":
                        node_id = nic_num
                        ep_lc = HFIEndpoints[node_id]
                        plink = sst.Link("precinctlink_%d" % node_id)
                        ep_lc.addLink(plink, "rtr_port", host_link_latency)
                        rtr.addLink(plink, "port%d" % port, host_link_latency)
                        # host_links.append(plink)
                        ep_list.append(port)
                        nic_num = nic_num + 1
                    else:
                        linkName = "link_{guid}_{port}_{guid1}_{port1}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        linkName2 = "link_{guid1}_{port1}_{guid}_{port}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        # print(getLink(linkName, linkName2))
                        link = getLink(linkName, linkName2)
                        links_list.append(port)
                        rtr.addLink(link, "port%d" % port, link_latency)
                else:
                    unconnected.append(port)

            sub.addParam("ep_map", ep_list)
            sub.addParam("links_map", links_list)
            sub.addParam("unconnected_map", unconnected)
    elif topology == "CSV-Megafly":
        df = pd.read_csv(csv_topology, header=None, skipinitialspace=True)
        # df.columns = df.columns.str.strip().str.lower().str.replace(' ', '_').
        # str.replace('(', '').str.replace(')','').str.replace('.', '')
        df.columns = ["guid", "port", "type", "description", "guid1", "port1", "type1", "description1"]

        if host_link_latency is None:
            host_link_latency = link_latency

        max_port = int(df.port.max())
        print("Max Port is: " + str(max_port))
        links = dict()

        def getLink(name, name2):
            if name not in links and name2 not in links:
                links[name] = sst.Link(name)
                # links[name] = name
            if name in links:
                return links[name]
            if name2 in links:
                ret = links[name2]
                del links[name2]
                return ret
        nic_num = 0

        for switch in df[df.type == "SW"].description.unique():
            router_num = int(re.findall(r'\d+', switch)[0])
            rtr = instanciateRouter(max_port+1, router_num)
            sub = rtr.setSubComponent(router.getTopologySlotName(), "cornelis.topoOpa400", 0)  # noqa: F821
            sub.addParams(topoOpa400)  # noqa: F821
            sub.addParam("urt0", urts_dir+"/Mfly_"+switch+"_urt0.csv")
            sub.addParam("urt1", urts_dir+"/Mfly_"+switch+"_urt1.csv")
            sub.addParam("urt2", urts_dir+"/Mfly_"+switch+"_urt2.csv")
            sub.addParam("urt3", urts_dir+"/Mfly_"+switch+"_urt3.csv")

            if ((router_num % aggregate_start) < (aggregate_start/2)):  # noqa: F821
                sub.addParam("switch_personality", 0)
            else:
                sub.addParam("switch_personality", 1)

            unconnected = list()
            links_list = list()
            ep_list = list()
            switch_df = df[df.description == switch]
            for port in range(0, max_port+1):
                portDf = switch_df[(switch_df.port == port)]
                # Check if port is connected
                if not portDf.empty:
                    if str(portDf[portDf.port == port].head().type1.values[0]) == "FI":
                        node_id = nic_num
                        ep_lc = HFIEndpoints[node_id]
                        plink = sst.Link("precinctlink_%d" % node_id)
                        ep_lc.addLink(plink, "rtr_port", host_link_latency)
                        host_links.append(plink)  # noqa: F821
                        ep_list.append(port)
                        nic_num = nic_num + 1
                    else:
                        linkName = "link_{guid}_{port}_{guid1}_{port1}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        linkName2 = "link_{guid1}_{port1}_{guid}_{port}".format(
                            guid=portDf[portDf.port == port].head().guid.values[0],
                            port=portDf[portDf.port == port].head().port.values[0],
                            guid1=portDf[portDf.port == port].head().guid1.values[0],
                            port1=portDf[portDf.port == port].head().port1.values[0])
                        # print(getLink(linkName, linkName2))
                        link = getLink(linkName, linkName2)
                        links_list.append(port)
                        rtr.addLink(link, "port%d" % port, link_latency)
                else:
                    unconnected.append(port)

            sub.addParam("ep_map", ep_list)
            sub.addParam("links_map", links_list)
            sub.addParam("unconnected_map", unconnected)


class FORZA:
    def __init__(self, name="FORZA", zones=4, precincts=1,
                 zapsPerZone=4, hartsPerZap=1, clock="1GHz",
                 program="test.exe", memSize=1073741823,
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
              "req_per_cycle": self.reqPerCycle
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
            zipComp_lc = zipHFINIC.setSubComponent("iface", "cornelis.opalinkcontrol", 0)
            zipComp_lc.addParams({
              "link_bw": "400Gb/s",
              "input_buf_size": "14kB",
              "output_buf_size": "14kB",
              "rc_small": 0,
              "rc_large": 4,
              "rc_threshold": 12000,
              "cc_becn_enabled": False,
              "cc_becn_down_count": "50us",
              "cc_delay_enabled": False,
              "cc_delay_init": "50us",
              "cc_delay_reload_init": "60us",
              "cc_delay_reload_inc": "10us",
              "cc_delay_reload_dec": "20us",
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

                # -- create the ZQM
                zqm = sst.Component("zqm_"+str(i)+"_"+str(j), "forzazqm.ZQM")
                zqm.addParams({
                  "verbose": self.verbose,
                  "clockFreq": self.clock,
                  "precinctId": i,
                  "zoneId": j,
                  "numHarts": 1,
                  "numCores": 1
                })
                zqm_nic = zqm.setSubComponent("zone_nic", "forza.zopNIC")
                zqm_iface = zqm_nic.setSubComponent("iface", "merlin.linkcontrol")
                zqm_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle
                })
                zqm_iface.addParams({
                  "input_buf_size": self.inputBufSize,
                  "output_buf_size": self.outputBufSize,
                  "link_bw": self.linkBW,
                })

                zqm_link = sst.Link("zqm_link_"+str(i)+"_"+str(j))
                zqm_link.connect((zqm_iface, "rtr_port", "1us"),
                                 (zone_router, "port"+str(self.zapsPerZone), "1us"))

                # -- create the ZEN
                zen = sst.Component("zen_"+str(i)+"_"+str(j), "forzazen.ZEN")
                zen.addParams({
                  "verbose": self.verbose,
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
                  "req_per_cycle": self.reqPerCycle
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

                # -- create the RZA
                rza = sst.Component("rza_"+str(i)+"_"+str(j), "revcpu.RevCPU")
                rza.addParams({
                  "verbose": self.verbose,
                  "numCores": 2,
                  "clock": self.clock,
                  "memSize": self.memSize,
                  "machine": "[CORES:RV64G]",
                  "startAddr": "[CORES:0x00000000]",
                  "program": self.program,
                  "enableZoneNIC": 1,
                  "enableRZA": 1,
                  "precinctId": i,
                  "zoneId": j,
                  "enable_memH": 1,
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
                  "addr_range_end": self.memSize,
                  "backing": "malloc"
                })
                backing_memory = memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
                backing_memory.addParams({
                  "access_time": self.memAccessTime,
                  "mem_size": str(self.memSize)+"B"
                })
                rza_mem_link = sst.Link("rza_mem_link_"+str(i)+"_"+str(j))
                rza_mem_link.connect((rza_lsq_iface, "port", "50ps"), (memctrl, "direct_link", "50ps"))

                rza_nic = rza.setSubComponent("zone_nic", "forza.zopNIC")
                rza_iface = rza_nic.setSubComponent("iface", "merlin.linkcontrol")
                rza_nic.addParams({
                  "verbose": self.verbose,
                  "clock": self.clock,
                  "req_per_cycle": self.reqPerCycle
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
                    zap.addParams({
                      "verbose": self.verbose,
                      "numCores": 1,
                      "clock": self.clock,
                      "memSize": self.memSize,
                      "machine": "[CORES:RV64GC]",
                      "startAddr": "[CORES:0x00000000]",
                      "program": self.program,
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
                      "req_per_cycle": self.reqPerCycle
                    })
                    zap_iface.addParams({
                      "input_buf_size": self.inputBufSize,
                      "output_buf_size": self.outputBufSize,
                      "link_bw": self.linkBW,
                    })
                    zap_link = sst.Link("zap_link_"+str(i)+"_"+str(j)+"_"+str(k))
                    zap_link.connect((zap_iface, "rtr_port", "1us"),
                                     (zone_router, "port"+str(k), "1us"))

        return ZipArray


if __name__ == "__main__":
    # -- parse the args
    ap = argparse.ArgumentParser()
    ap.add_argument("-a", "--hartsperzap", required=True, help="sets the number of harts per zap")
    ap.add_argument("-z", "--zaps", required=True, help="sets the number of zaps per zone")
    ap.add_argument("-o", "--zones", required=True, help="sets the number of zones per precinct")
    ap.add_argument("-p", "--precincts", required=True, help="sets the number of precincts")
    ap.add_argument("-r", "--program", required=True, help="sets the target program exe")
    ap.add_argument("-b", "--baseToolingDir", required=True, help="Sets the base path for the router configurations")
    ap.add_argument("-c", "--coreSwitchIdx", required=True, help="Sets the core switch idx for the router configurations")
    ap.add_argument("-t", "--maxTerminal", required=True,
                    help="Sets the Max terminal for the switch for the router configurations")
    ap.add_argument("-l", "--maxLocal", required=True, help="Sets the Max Local for the switch for the router configurations")
    args = vars(ap.parse_args())

    # -- this argument is only used for the merlin network
    base_tooling_dir = args['baseToolingDir']
    maxTerminal = args['maxTerminal']
    maxLocal = args['maxLocal']

    # -- build all the inner-precinct logic
    f = FORZA(zones=int(args['zones']), precincts=int(args['precincts']),
              zapsPerZone=int(args['zaps']), hartsPerZap=int(args['hartsperzap']),
              program=args['program'])
    HFIEndpoints = f.build()

    # -- build the system network
    sst.addGlobalParams("router_params", router_params)
    sst.addGlobalParams("networkLinkControl_params", networkLinkControl_params)
    buildTopo()

# -- EOF
