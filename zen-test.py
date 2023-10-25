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
    "num_ports" : "5"
})

zen_params = {

}



def main():
  print('\nStarting SST forzasim_macro')

  router = sst.Component("router", "merlin.hr_router")
  router.addGlobalParamSet("router_params")
  router.addParams({"id": 0, "num_ports": 3})

  topo = router.setSubComponent("topology", "merlin.singlerouter", 0)
  topo.addGlobalParamSet("topology_params")

  zen0 = sst.Component("zen0", "ForzaZEN.ZEN")
  #zen0.addGlobalParamSet("zen_params")
  zen0.addParams({"int_id" : 0})
  zen0_lc = zen0.setSubComponent("rtrLink", "merlin.linkcontrol", 0)
  zen0_lc.addGlobalParamSet("networkLinkControl_params")


  zen0_rtrlink = sst.Link("link_zen0_rtr0")
  router.addLink(zen0_rtrlink, "port0", "5 ns")
  zen0_lc.addLink(zen0_rtrlink, "rtr_port", "5 ns")

  zen1 = sst.Component("zen1", "ForzaZEN.ZEN")
  zen1.addGlobalParamSet("zen_params")
  zen1.addParams({"int_id" : 1})
  zen1_lc = zen1.setSubComponent("rtrLink", "merlin.linkcontrol", 0)
  zen1_lc.addGlobalParamSet("networkLinkControl_params")


  zen1_rtrlink = sst.Link("link_zen1_rtr1")
  router.addLink(zen1_rtrlink, "port1", "5 ns")
  zen1_lc.addLink(zen1_rtrlink, "rtr_port", "5 ns")


  zen2 = sst.Component("zen2", "ForzaZEN.ZEN")
  zen2.addGlobalParamSet("zen_params")
  zen2.addParams({"int_id" : 2})
  zen2_lc = zen2.setSubComponent("rtrLink", "merlin.linkcontrol", 0)
  zen2_lc.addGlobalParamSet("networkLinkControl_params")


  zen2_rtrlink = sst.Link("link_zen2_rtr2")
  router.addLink(zen2_rtrlink, "port2", "5 ns")
  zen2_lc.addLink(zen2_rtrlink, "rtr_port", "5 ns")

  forzamemctrl = sst.Component("forzamemctrl", "ForzaMem.ForzaMemController")
  forzamemctrl.addParams({
      "clock" : "2GHz"
  })

  forzanic = forzamemctrl.setSubComponent("forzaNIC", "ForzaElement.forzaNIC")
  forzanic.addParams({
        "id" : 0,
        "num_peers" : 0,
        "cacheLine" : "64",
        "message_size" : "8B",
        "send_untimed_broadcast" : 0
  })
  forzaiface = forzanic.setSubComponent("networkIF", "merlin.linkcontrol", 0)
  forzaiface.addGlobalParamSet("networkLinkControl_params")
  forzaiface.addParams({
      "input_buf_size" : "1kB",
      "output_buf_size" : "1kB",
      "link_bw" : "1GB/s"
  })


  forzamem = forzamemctrl.setSubComponent("forzaMEM", "memHierarchy.standardInterface")
  forzamem.addParams({
      "verbose" : 2
  })


  l1cache = sst.Component("l1cache", "memHierarchy.Cache")
  l1cache.addParams({
      "access_latency_cycles" : "4",
      "cache_frequency" : "2 Ghz",
      "replacement_policy" : "lru",
      "coherence_protocol" : "MSI",
      "associativity" : "4",
      "cache_line_size" : "64",
      "debug" : DEBUG_L1,
      "debug_level" : DEBUG_LEVEL,
      "verbose" : VERBOSE,
      "L1" : "1",
      "cache_size" : "16KiB"
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
  link_cpu_cache_link = sst.Link("link_cpu_cache_link")
  link_cpu_cache_link.connect( (forzamem, "port", "1000ps"), (l1cache, "high_network_0", "1000ps") )
  link_mem_bus_link = sst.Link("link_mem_bus_link")
  link_mem_bus_link.connect( (l1cache, "low_network_0", "50ps"), (memctrl, "direct_link", "50ps") )

  forzaiface_rtrlink = sst.Link("link_rza_rtr3")
  router.addLink(forzaiface_rtrlink, "port3", "5 ns")
  forzaiface.addLink(forzaiface_rtrlink, "rtr_port", "5 ns")




if __name__ == '__main__':
  main()