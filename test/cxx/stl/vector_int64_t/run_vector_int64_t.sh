#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f vector_int64_t.exe ]; then
  sst --add-lib-path=../../build/src/ ./rev-test.py
else
  echo "Test STL CXX vector_int64_t: vector_int64_t.exe not Found - likely build failed"
  exit 1
fi
