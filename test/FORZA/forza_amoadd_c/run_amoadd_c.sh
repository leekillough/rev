#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f amoadd_c.exe ]]; then
  sst --add-lib-path=../../build/src/ ./rev-amoadd_c.py
else
  echo "Test FORZA amoadd_c: amoadd_c.exe not Found - likely build failed"
  exit 1
fi
