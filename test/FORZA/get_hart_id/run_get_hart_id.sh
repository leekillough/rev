#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [[ -f get_hart_id.exe ]]; then
	sst --add-lib-path=../../build/src/ ./rev_get_hart_id.py
else
	echo "Test FORZA get_hart_id: get_hart_id.exe not Found - likely build failed"
	exit 1
fi
