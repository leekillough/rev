#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_zqm_mbox_setup_test.exe ]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py
  sst --model-options="--program=forza_zqm_mbox_setup_test.exe" ./rev-onezone.py
else
  echo "Test FORZA forza_zqm_mbox_setup_test.c: forza_zqm_mbox_setup_test.exe not Found - likely build failed"
  exit 1
fi
