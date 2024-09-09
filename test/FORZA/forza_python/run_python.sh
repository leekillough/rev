#!/bin/bash

#Build the test
make clean && make

# Look for the ZEN and ZQM models
ZEN=`sst-info forzazen.ZEN | grep "ELI version"`
ZQM=`sst-info forzazqm.ZQM | grep "ELI version"`

# Check that the exec was built...
if [ -f ex2.exe ]; then
  if [ -n "$ZEN" ]; then
    if [ -n "$ZQM" ]; then
      sst --add-lib-path=../../build/src/ --run-mode=init --model-options="--hartsperzap 1 --zaps 1 --zones 1 --precincts 8 --shape 2,2:4 --program ex2.exe" ../../../python/forza.py
      sst --add-lib-path=../../build/src/ --run-mode=init --model-options="--hartsperzap 512 --zaps 1 --zones 1 --precincts 8 --shape 2,2:4 --program ex2.exe" ../../../python/forza.py
      sst --add-lib-path=../../build/src/ --run-mode=init --model-options="--hartsperzap 512 --zaps 2 --zones 1 --precincts 8 --shape 2,2:4 --program ex2.exe" ../../../python/forza.py
      sst --add-lib-path=../../build/src/ --run-mode=init --model-options="--hartsperzap 512 --zaps 2 --zones 2 --precincts 8 --shape 2,2:4 --program ex2.exe" ../../../python/forza.py
      sst --add-lib-path=../../build/src/ --run-mode=init --model-options="--hartsperzap 512 --zaps 4 --zones 4 --precincts 8 --shape 2,2:4 --program ex2.exe" ../../../python/forza.py
    fi
  fi

else
  echo "Test EX2: ex2.exe not Found - likely build failed"
  exit 1
fi
