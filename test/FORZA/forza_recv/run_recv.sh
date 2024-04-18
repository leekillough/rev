#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_recv.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev_forza_recv.py
else
  echo "Test FORZA forza_recv.c: forza_recv.exe not Found - likely build failed"
  exit 1
fi
