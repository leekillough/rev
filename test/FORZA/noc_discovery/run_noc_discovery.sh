#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f noc_discovery.exe ]; then
	sst --add-lib-path=../../build/src/ ./rev-test-noc-discovery.py
else
	echo "Test NOC_DISCOVERY: noc_discovery.exe not Found - likely build failed"
	exit 1
fi
