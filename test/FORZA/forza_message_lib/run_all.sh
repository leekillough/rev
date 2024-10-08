#!/bin/bash

#Build the test
make clean && make

# Run all tests and check for completion
echo "$(date)" > test_all.out
echo ./run_setup.sh >> test_all.out
./run_setup.sh | grep -a "Simulation is complete" >> test_all.out
echo ./run_send.sh >> test_all.out
./run_send.sh | grep -a "Simulation is complete" >> test_all.out
echo ./run_send_debug.sh >> test_all.out
./run_send_debug.sh | grep -a "Simulation is complete" >> test_all.out
echo ./run_thread.sh >> test_all.out
./run_thread.sh | grep -a "Simulation is complete" >> test_all.out
echo ./run_actor.sh >> test_all.out
./run_actor.sh | grep -a "Simulation is complete" >> test_all.out
echo ./run_actor_concurrent.sh >> test_all.out
./run_actor_concurrent.sh | grep -a "Simulation is complete" >> test_all.out
