#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f vector_edge.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-test.py
else
  echo "Test STL CXX vector_edge: vector_edge.exe not Found - likely build failed"
  exit 1
fi
