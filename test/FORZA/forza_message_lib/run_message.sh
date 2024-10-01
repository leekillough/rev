#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_message_test.exe ]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py
  sst --model-options="--program=forza_message_test.exe --numHarts=32" ./rev-onezone.py
else
  echo "Test FORZA forza_message_test.c: forza_message_test.exe not Found - likely build failed"
  exit 1
fi
