#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_multisend.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-zen-multizap.py
else
  echo "Test FORZA forza_multisend.c: forza_multisend.exe not Found - likely build failed"
  exit 1
fi
