#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_send.exe ]; then
  #sst --add-lib-path=../../build/src/ ./rev_forza_send.py
  sst --add-lib-path=../../build/src/ ./rev-forza-onezap-twozone.py
else
  echo "Test FORZA forza_send.c: forza_send.exe not Found - likely build failed"
  exit 1
fi
