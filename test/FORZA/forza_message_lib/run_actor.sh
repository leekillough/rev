#!/bin/bash

#Build the test
make clean && make
program=actor_main.exe


# Check that the exec was built...
if [[ -f $program ]]; then
  sst --model-options="--hartsperzap 1 --zaps 2 --zones 1 --precincts 1 --shape 1,1:1 --program $program" ../forza-test-config-ring.py
else
  echo "Test FORZA $program not Found - likely build failed"
  exit 1
fi
