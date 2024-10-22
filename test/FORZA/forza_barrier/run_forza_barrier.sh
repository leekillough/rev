#!/bin/bash

#Build the test
make clean && make

# Look for the ZEN and ZQM models
export ZEN=$(sst-info forzazen.ZEN | grep "ELI version")
export ZQM=$(sst-info forzazqm.ZQM | grep "ELI version")

# Check that the exec was built...
EXES="forza_barrier_test_1hart.exe  forza_barrier_test_4zap.exe  forza_barrier_test_multi_barrier.exe"

for APP in $EXES; do
  if [[ -f $APP ]]; then
    PYFILE="$(basename -- "$APP" .exe).py"
    sst --add-lib-path=../../build/src/ "./$PYFILE"
  else
    echo "TEST $APP: not found - likely build failed"
    exit 1
  fi
done;
