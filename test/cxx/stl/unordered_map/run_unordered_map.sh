#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f unordered_map.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-test.py
else
  echo "Test STL CXX unordered_map: unordered_map.exe not Found - likely build failed"
  exit 1
fi
