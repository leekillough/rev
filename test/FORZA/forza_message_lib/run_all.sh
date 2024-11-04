#!/bin/bash

set -e

#Build the test
make clean && make

# Run all tests and check for completion
exec > test_all.out

date

echo ./run_setup.sh
./run_setup.sh | grep -a "Simulation is complete"

echo ./run_send.sh
./run_send.sh | grep -a "Simulation is complete"

echo ./run_send_debug.sh
./run_send_debug.sh | grep -a "Simulation is complete"

echo ./run_thread.sh
./run_thread.sh | grep -a "Simulation is complete"

echo ./run_actor.sh
./run_actor.sh | grep -a "Simulation is complete"

echo ./run_actor_concurrent.sh
./run_actor_concurrent.sh | grep -a "Simulation is complete"
