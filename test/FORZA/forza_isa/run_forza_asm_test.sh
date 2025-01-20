#!/bin/bash

#Build the test
make

# Check that the exec was built...
if [[ -x $RVASM.exe ]]; then
	sst --add-lib-path=../../../build/src/ ./rev-forza-isa-test.py -- --program="$RVASM.exe" --hartsperzap="1" --zaps="1" --zones="1" --precincts="1" --shape="1,1:1" --progargs=""
else
	echo "Test $RVASM ASM: File not found - likely build failed"
	exit 1
fi
