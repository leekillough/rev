#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_send_test.exe ]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py
  sst --model-options="--program=forza_send_test.exe --numHarts=16" ./rev-onezone.py
else
  echo "Test FORZA forza_send_test.c: forza_send_test.exe not Found - likely build failed"
  exit 1
fi
