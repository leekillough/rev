#!/bin/bash

#Build the test
make clean && make
program=forza_simple_thread.exe

# Check that the exec was built...
if [[ -f $program ]]; then
  # 1 HART/ZAP works for rev-onezone, not fore forza-test-config-ring.py
	#sst --model-options="--hartsperzap 1 --zaps 2 --zones 1 --precincts 1 --shape 1,1:1 --program $program" ./forza-test-config-ring.py
  sst --model-options="--program=forza_simple_thread.exe --numHarts 1" ./rev-onezone.py
  # multiple HARTS/ZAP doesn't work
  #sst --model-options="--hartsperzap 2 --zaps 2 --zones 1 --precincts 1 --shape 1,1:1 --program $program" ./forza-test-config-ring.py
  #sst --model-options="--program=forza_simple_thread.exe --numHarts 2" ./rev-onezone.py
else
  echo "Test $program not Found - likely build failed"
  exit 1
fi
