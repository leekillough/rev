#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f array.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-test.py
else
  echo "Test STL CXX array.c: array.exe not Found - likely build failed"
  exit 1
fi
