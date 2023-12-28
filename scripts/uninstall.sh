#!/bin/bash
#
# Unregisters Rev from the SST infrastructure
#

#-- unregister it
sst-register -u revcpu
sst-register -u forza
sst-register -u forzazen

#-- forcible remove it from the local script
CONFIG=~/.sst/sstsimulator.conf
if test -f "$CONFIG"; then
  echo "Removing configuration from local config=$CONFIG"
  sed -i.bak '/revcpu/d' $CONFIG
  sed -i.bak '/forza/d' $CONFIG
  sed -i.bak '/forzazen/d' $CONFIG
fi

