#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f actor_main_concurrent.exe ]]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py
  sst --model-options="--program=actor_main_concurrent.exe" ./rev-onezone.py
else
  echo "Test FORZA actor_main_concurrent.c: actor_main_concurrent.exe not Found - likely build failed"
  exit 1
fi
