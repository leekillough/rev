#!/bin/bash

#Build the test
make clean && make

# Check that the exec was built...
if [ -f amoadd_c.exe ]; then
  sst --add-lib-path=../../build/src/ ./private-invalid.py | tee private_invalid.log
  # sst --add-lib-path=../../build/src/ ./private-valid.py


else
  echo "Test FORZA amoadd_c: amoadd_c.exe not Found - likely build failed"
  exit 1
fi


# count the number of Invalid PAddr
count=$(grep -c "Invalid PAddr" private_invalid.log)

if [ "$count" -gt 0 ]; then
    echo "Test Passed"
    result=0
else
    echo "Test Failed"
    result=1
fi

rm -f private_invalid.log

exit $result