# FORZA PYTHON README

## Overview
The FORZA python script requires a number of unique options in order to drive the 
size and shape of the simulation.  The standard options are listed as follows:

* `-h, --help            show this help message and exit`
* `-a HARTSPERZAP, --hartsperzap HARTSPERZAP sets the number of harts per zap`
* `-z ZAPS, --zaps ZAPS  sets the number of zaps per zone`
* `-o ZONES, --zones ZONES sets the number of zones per precinct`
* `-p PRECINCTS, --precincts PRECINCTS sets the number of precincts`
* `-r PROGRAM, --program PROGRAM sets the target program exe`
* `-s SHAPE, --shape SHAPE sets the shape of the system network`

## Using the script

You can use the script as follows:
```
sst --model-options="--hartsperzap 1 --zaps 1 --zones 1 --precincts 8 --shape 2,2:4 --program ex2.exe" /path/to/forza.py
sst --model-options="--hartsperzap 512 --zaps 4 --zones 4 --precincts 8 --shape 2,2:4 --program ex2.exe" /path/to/forza.py
```
