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

## Using FORZA with Cornelis' Network Model

There are additional parameters that are required to run this: 

  * `-a, --hartsperzap, sets the number of harts per zap`
  * `-z, --zaps, sets the number of zaps per zone`
  * `-o, --zones, sets the number of zones per precinct`
  * `-p, --precincts, sets the number of precincts`
  * `-r, --program, sets the target program exe`
  * `-b, --baseToolingDir, Sets the base path for the router configurations`
  * `-c, --coreSwitchIdx, Sets the core switch idx for the router configurations`
  * `-t, --maxTerminal, Sets the Max terminal for the switch for the router configurations`
  * `-l, --maxLocal, Sets the Max Local for the switch for the router configurations`
  
The `baseToolingDir` should be configured to: 
    <extracted_cornelis>/

For the full 4608 set maxTerminal and maxLocal to 24. The coreSwitchIdx is 384 for the full system. 

