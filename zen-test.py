import argparse
import sst

sst.setProgramOption("verbose", "1")
# sst.setProgramOption("stop-at", "0 ns")
# sst.setProgramOption("print-timing-info", "false")
# sst.setProgramOption("heartbeat-period", "")
# sst.setProgramOption("timebase", "1 ps")
# sst.setProgramOption("partitioner", "sst.single")
# sst.setProgramOption("timeVortex", "sst.timevortex.priority_queue")
# sst.setProgramOption("interthread-links", "false")
# sst.setProgramOption("output-prefix-core", "@x SST Core: ")
DEBUG_L1 = 0
DEBUG_MEM = 0
DEBUG_LEVEL = 10
VERBOSE = 2
MEM_SIZE = 1024*1024*1024-1
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

zen_params = {

}



def main():
  print('\nStarting SST forzasim_macro')

  router = sst.Component("router", "merlin.hr_router")
  router.addGlobalParamSet("router_params")
  router.addParams({"id": 0, "num_ports": 4})

  topo = router.setSubComponent("topology", "merlin.singlerouter", 0)
  topo.addGlobalParamSet("topology_params")

  rza = sst.Component("rza", "Forza.ZOPGen")
  #zen0.addGlobalParamSet("zen_params")
  rza.addParams({"int_id" : 8})

  rza_lc = rza.setSubComponent("m_zop_iface", "Forza.zopNIC", 0)
  rza_lc.addGlobalParamSet("networkLinkControl_params")
  rza_zopapi_lc = rza_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
  rza_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
  rza_lc.addParams({"verbose": 10})


  #zen0_rtrlink = sst.Link("link_zen0_rtr0")
  #router.addLink(zen0_rtrlink, "port0", "5 ns")
  #zen0_lc.addLink(zen0_rtrlink, "network", "5 ns")
  rza_link0 = sst.Link("rza_link")
  rza_link0.connect( (rza_zopapi_lc, "rtr_port", "1us"), (router, "port0", "1us") )
  zap0 = sst.Component("zap0", "Forza.ZOPGen")
  #zen0.addGlobalParamSet("zen_params")
  zap0.addParams({"int_id" : 0})

  zap0_lc = zap0.setSubComponent("m_zop_iface", "Forza.zopNIC", 0)
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



  zap1_lc = zap1.setSubComponent("m_zop_iface", "Forza.zopNIC", 0)
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
  zen1.addParams({"int_id" : 1})

  zen1_lc = zen1.setSubComponent("m_zop_iface", "Forza.zopNIC", 0)
  zen1_lc.addGlobalParamSet("networkLinkControl_params")
  zen1_zopapi_lc = zen1_lc.setSubComponent("iface", "merlin.linkcontrol", 0)
  zen1_zopapi_lc.addGlobalParamSet("networkLinkControl_params")
  zen_link1 = sst.Link("zen1_link")
  zen_link1.connect( (zen1_zopapi_lc, "rtr_port", "1us"), (router, "port1", "1us") )
  zen1_lc.addParams({"verbose": 10})


  #zen1_rtrlink = sst.Link("link_zen1_rtr0")
  #router.addLink(zen1_rtrlink, "port1", "5 ns")
  #zen1_lc.addLink(zen1_rtrlink, "network", "5 ns")

if __name__ == '__main__':
  main()