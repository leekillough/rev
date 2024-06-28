#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_zqm_mbox_setup.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-with-two-zone.py
else
  echo "Test FORZA forza_zqm_mbox_setup.c: forza_zqm_mbox_setup.exe not Found - likely build failed"
  exit 1
fi
