#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_send_init.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-forza-send-init-with-two-zone.py
else
  echo "Test FORZA forza_send_init.c: forza_send_init.exe not Found - likely build failed"
  exit 1
fi
