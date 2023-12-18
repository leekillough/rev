#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -x scratchpad_multi_alloc_overflow.exe ]; then
	sst --add-lib-path=../../build/src/ ./rev-test.py
else
	echo "Test SCRATCHPAD_MULTI_ALLOC_OVERFLOW: SCRATCHPAD_MULTI_ALLOC_OVERFLOW.exe not Found - likely build failed"
	exit 1
fi
