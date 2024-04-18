#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f ex2.exe ]; then
	sst --add-lib-path=../../build/src/ ./rev-test-harts-per-zap.py
else
	echo "Test FORZA test_config_syscalls: ex2.exe not Found - likely build failed"
	exit 1
fi

