#!/bin/bash

#Build the test
make clean && make

#    ap.add_argument("-a", "--hartsperzap", required=True, help="sets the number of harts per zap")
#    ap.add_argument("-z", "--zaps", required=True, help="sets the number of zaps per zone")
#    ap.add_argument("-o", "--zones", required=True, help="sets the number of zones per precinct")
#    ap.add_argument("-p", "--precincts", required=True, help="sets the number of precincts")
#    ap.add_argument("-r", "--program", required=True, help="sets the target program exe")
#    ap.add_argument("-s", "--shape", required=True, help="sets the shape of the system network")
#    ap.add_argument("-g", "--progargs", default="", required=False, help="sets the arguments for the program ran by the zaps")


# Check that the exec was built...
if [[ -f forza_rmozops.exe ]]; then
  sst --add-lib-path=../../build/src/ ../forza-test-config-ring.py -- -a 32 -z 1 -o 2 -p 1 -r ./forza_rmozops.exe -s 1,1:1
else
  echo "Test FORZA forza_rmozops.c: forza_rmozops.exe not Found - likely build failed"
  exit 1
fi
