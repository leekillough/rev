#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f forza_argc.exe ]]; then
  sst --add-lib-path=../../build/src/ ./rev-forza-argc.py
else
  echo "Test FORZA forza_argc.c: forza_argc.exe not Found - likely build failed"
  exit 1
fi
