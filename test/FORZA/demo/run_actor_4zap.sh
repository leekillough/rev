#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f actor_4zap.exe ]]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py

  #sst --model-options="--program=actor_conc_4zap.exe --numZaps=4" ./rev-4zap-onezone.py
  #sst --model-options="--program=actor_conc_4zap.exe --numZaps=2" ./rev-onezone.py
  sst --model-options="--hartsperzap 1 --zaps 4 --zones 1 --precincts 1 --shape 1,1:1 --program actor_4zap.exe" ./forza-test-config-ring.py | grep -a debug:P
else
  echo "Test FORZA actor_4zap.c: actor_4zap.exe not Found - likely build failed"
  exit 1
fi
