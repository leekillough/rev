#!/bin/bash

#Build the test
make clean && make

program=actor_main_concurrent.exe

# Check that the exec was built...
if [[ -f $program ]]; then
  # Small 2 zone test
  sst --model-options="--hartsperzap 1 --zaps 4 --zones 2 --precincts 1 --shape 1,1:1 --program $program" ../forza-test-config-ring.py
  # Full precinct 
  #sst --model-options="--hartsperzap 1 --zaps 4 --zones 8 --precincts 1 --shape 1,1:1 --program $program" ./forza-test-config-ring.py
else
  echo "Test FORZA $program not Found - likely build failed"
  exit 1
fi
