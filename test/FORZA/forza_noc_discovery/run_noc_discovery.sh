#!/bin/bash

#Build the test
make clean && make

# Look for the ZEN and ZQM models
ZEN=`sst-info forzazen.ZEN | grep "ELI version"`
ZQM=`sst-info forzazqm.ZQM | grep "ELI version"`

# Check that the exec was built...
if [ -f ex2.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery.py
  sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery-with-multi-zap.py

  if [ -n "$ZEN" ]; then
    sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery-with-zen.py
  fi

  if [ -n "$ZQM" ]; then
    sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery-with-zqm.py
  fi

  if [ -n "$ZEN" ]; then
    if [ -n "$ZQM" ]; then
      sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery-with-zen_zqm.py
      sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery-with-two-zone.py
    fi
  fi

else
  echo "Test EX2: ex2.exe not Found - likely build failed"
  exit 1
fi
