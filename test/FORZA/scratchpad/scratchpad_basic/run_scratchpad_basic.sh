#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -x scratchpad_basic.exe ]; then
	sst --add-lib-path=../../build/src/ ./rev-test.py
else
	echo "Test SCRATCHPAD_BASIC: SCRATCHPAD_BASIC.exe not Found - likely build failed"
	exit 1
fi
