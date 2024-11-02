#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f forza_simple_thread.exe ]]; then
	sst --model-options="--program=forza_simple_thread.exe --numHarts 1" ./rev-onezone.py
else
  echo "Test FORZA forza_simple_thread.c: forza_simple_thread.exe not Found - likely build failed"
  exit 1
fi
