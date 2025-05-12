import sst
import argparse

parser = argparse.ArgumentParser()

parser.add_argument("-p", "--precincts",  help="Number of precincts",                               default=2, type=int)
parser.add_argument("-z", "--zones",      help="Number of zones per precinct",                      default=1, type=int)
parser.add_argument("-w", "--max_wait",   help="Maximum number of microseconds before aggregation", default=10, type=float)
parser.add_argument("-n", "--num_zops",   help="Number of ZOPs to send per ZOPgen per cycle",       default=1, type=int)
parser.add_argument("-c", "--num_cycles", help="Number of cycles to send ZOPs from ZOPgens",        default=1, type=int)
parser.add_argument("-i", "--interval",   help="Number of cycles to wait between sending ZOPs",     default=1, type=int)
parser.add_argument("-t", "--test",       help="Which test setup to run",                           default=0, type=int)
parser.add_argument("-r", "--rendezvous", help="Switch to rendezvous messaging between ZIPs",       action='store_true')

args = parser.parse_args()

NUM_PRECINCTS = args.precincts
NUM_ZONES = args.zones

sst.addGlobalParams("zopNIC_params", {
    "link_bw" : "100Gb/s",
})
sst.addGlobalParams("router_params", {
    "flit_size" : "8B",
    "input_buf_size" : "14kB",
    "link_bw" : "100Gb/s",
    "num_vns" : "1",
    "output_buf_size" : "14kB",
    "xbar_bw" : "100Gb/s",
})

class FORZA:
    def __init__(self):
        self.fabric = sst.Component("fabric", "merlin.hr_router")
        self.fabric.addGlobalParamSet("router_params")
        self.fabric.addParams({"id": 0, "num_ports": NUM_PRECINCTS})
        self.fabric.setSubComponent("topology", "merlin.singlerouter", 0)

        self.num_precincts = 0
        self.precincts = []

    def addPrecinct(self):
        self.precincts.append(Precinct(self.fabric, self.num_precincts))
        self.num_precincts += 1

class Precinct:
    def __init__(self, fabric, precinct_id):
        self.precinct_id = precinct_id

        self.zip = ZIP(precinct_id)

        self.link_zip_hfi = sst.Link("link_zip_hfi_{}".format(precinct_id))
        self.link_zip_hfi.connect((self.zip.hfilinkcontrol, "rtr_port", "1us"), (fabric, "port{}".format(precinct_id), "1us"))

        self.noc = sst.Component(str(self), "merlin.hr_router")
        self.noc.addGlobalParamSet("router_params")
        self.noc.addParams({"id": 1+precinct_id, "num_ports": 1+NUM_ZONES})
        self.noc.setSubComponent("topology", "merlin.singlerouter", 0)

        self.link_zip_noc = sst.Link("link_zip_noc_{}".format(precinct_id))
        self.link_zip_noc.connect((self.zip.linkcontrol, "rtr_port", "1us"), (self.noc, "port0", "1us"))

        self.num_zones = 0
        self.zones = []

    def addZone(self):
        self.zones.append(Zone(self.noc, self.precinct_id, self.num_zones))
        self.num_zones += 1

    def __str__(self):
        return "noc_{}".format(self.precinct_id)

class ZIP:
    def __init__(self, precinct_id):
        self.precinct_id = precinct_id

        self.zip = sst.Component(str(self), "ForzaZIP.ZIP")

        self.zipmem = self.zip.setSubComponent("memory", "ForzaZIP.ZIPBasicMemCtrl", 0)
        self.memiface = self.zipmem.setSubComponent("memIface", "memHierarchy.standardInterface", 0)

        self.memctrl = sst.Component("zip_memctrl_{}".format(precinct_id), "memHierarchy.MemController")
        self.memctrl.addParams({"clock" : "2GHz", "addr_range_start" : 0, "backing" : "malloc"})
        self.memory = self.memctrl.setSubComponent("backend", "memHierarchy.simpleMem")
        self.memory.addParams({"access_time" : "100ns", "mem_size" : "8GB"})

        if args.rendezvous:
            self.zip.addParams({"maxRVBuff" : 20, "MTU" : 5, "RVThresh" : 0, "tests" : 1, "verbose" : 9, "precID" : precinct_id, "maxWait" : "{}us".format(args.max_wait)})
        else:
            self.zip.addParams({"tests" : 1, "verbose" : 9, "precID" : precinct_id, "maxWait" : "{}us".format(args.max_wait)})

        self.nic = self.zip.setSubComponent("zopLink", "forza.zopNIC", 1)
        self.linkcontrol = self.nic.setSubComponent("iface", "merlin.linkcontrol", 0)
        self.linkcontrol.addGlobalParamSet("zopNIC_params")
        self.nic.addParams({"verbose" : 0, "precinctID" : precinct_id})

        self.hfinic = self.zip.setSubComponent("hfiLink", "ForzaZIP.ZIPHFINIC", 2)
        self.hfilinkcontrol = self.hfinic.setSubComponent("iface", "merlin.linkcontrol", 0)
        self.hfilinkcontrol.addGlobalParamSet("zopNIC_params")
        self.hfinic.addParams({"verbose" : 0, "precinctID" : precinct_id})

        self.memlink = sst.Link("link_zip_mem_{}".format(precinct_id))
        self.memlink.connect((self.memiface, "port", "50ps"), (self.memctrl, "direct_link", "50ps"))

    def __str__(self):
        return "zip_{}".format(self.precinct_id)

class Zone:
    def __init__(self, noc, precinct_id, zone_id):
        self.precinct_id = precinct_id
        self.zone_id = zone_id

        self.xbar = sst.Component("xbar_{}".format(self), "merlin.hr_router")
        self.xbar.addGlobalParamSet("router_params")
        self.xbar.addParams({"id": 1+NUM_PRECINCTS+precinct_id*NUM_ZONES+zone_id, "num_ports": 2})
        self.xbar.setSubComponent("topology", "merlin.singlerouter", 0)

        self.zen = sst.Component("zen_{}".format(self), "forzazen.ZEN");
        self.zen.addParams({"precinctId" : precinct_id, "zoneId" : zone_id, "verbose" : 0, "enablePrecinctNIC": True})

        self.zone_nic = self.zen.setSubComponent("zone_nic", "forza.zopNIC", 0)
        self.zone_linkcontrol = self.zone_nic.setSubComponent("iface", "merlin.linkcontrol", 0)
        self.zone_linkcontrol.addGlobalParamSet("zopNIC_params")
        self.zone_nic.addParams({"verbose" : 0})

        self.prec_nic = self.zen.setSubComponent("precinct_nic", "forza.zopNIC", 0)
        self.prec_linkcontrol = self.prec_nic.setSubComponent("iface", "merlin.linkcontrol", 0)
        self.prec_linkcontrol.addGlobalParamSet("zopNIC_params")
        self.prec_nic.addParams({"verbose" : 0})

        self.noc_link = sst.Link("noc_zen_link_{}".format(self))
        self.noc_link.connect((self.prec_linkcontrol, "rtr_port", "1us"), (noc, "port{}".format(zone_id+1), "1us"))

        self.xbar_link = sst.Link("xbar_zen_link_{}".format(self))
        self.xbar_link.connect((self.zone_linkcontrol, "rtr_port", "1us"), (self.xbar, "port0", "1us"))

        self.zopgen = sst.Component("zopgen_{}".format(self), "forzazen.ZOPGen_prec")
        self.zopgen.addParams({"zone_id" : zone_id, "int_id" : precinct_id, "num_ZOPs" : args.num_zops, "num_cycles" : args.num_cycles, "interval" : args.interval, "test" : args.test})

        self.zopgen_nic = self.zopgen.setSubComponent("m_zop_iface", "forza.zopNIC", 0)
        self.zopgen_linkcontrol = self.zopgen_nic.setSubComponent("iface", "merlin.linkcontrol", 0)
        self.zopgen_linkcontrol.addGlobalParamSet("zopNIC_params")
        self.zopgen_nic.addParams({"verbose" : 0})

        self.zopgen_link = sst.Link("zopgen_link_{}".format(self))
        self.zopgen_link.connect((self.zopgen_linkcontrol, "rtr_port", "1us"), (self.xbar, "port1", "1us"))

    def __str__(self):
        return "zone_{}_{}".format(self.precinct_id, self.zone_id)

F = FORZA()
for i in range(NUM_PRECINCTS):
    F.addPrecinct()
    for j in range(NUM_ZONES):
        F.precincts[-1].addZone()

sst.setStatisticLoadLevel(1)
sst.enableAllStatisticsForComponentType("ForzaZIP.ZIP")
sst.setStatisticOutput("sst.statOutputConsole")
