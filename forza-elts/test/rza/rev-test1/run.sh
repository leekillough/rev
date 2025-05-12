#!/bin/bash

#Build the test
make clean && make
# sst --model-options="1 standardCPU" ./1.py # pass
# sst --model-options="2 standardCPU" ./1.py # pass
sst --model-options="1 rev program1.exe" ./1.py # pass
sst --model-options="1 rev program2.exe" ./1.py # pass
sst --model-options="1 rev program3.exe" ./1.py # pass
sst --model-options="1 rev program4.exe" ./1.py # fail
sst --model-options="1 rev program5.exe" ./1.py # pass
sst --model-options="1 rev program6.exe" ./1.py # fail
sst --model-options="1 rev program7.exe" ./1.py # pass
sst --model-options="1 rev program8.exe" ./1.py # fail
sst --model-options="1 rev program9.exe" ./1.py # pass
# sst --model-options="10 rev program1.exe" ./1.py # pass

