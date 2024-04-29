#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f forza_security.exe ]; then
  sst --add-lib-path=../../build/src/ ./private-valid.py
else
  echo "Test FORZA Security: forza_security.exe not Found - likely build failed"
  exit 1
fi
