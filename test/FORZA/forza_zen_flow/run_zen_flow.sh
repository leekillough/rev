#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_zen_flow.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-forza-zen-setup-with-two-zone.py
else
  echo "Test FORZA forza_zen_flow.c: forza_zen_flow.exe not Found - likely build failed"
  exit 1
fi
