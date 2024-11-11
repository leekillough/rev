#!/bin/bash

# These test pass on REV
make clean; make -s TLIST="csr.128.64 vadd_e32_m1.128.64 vadd_e8_m1.128.64 vadd_e16_m1.128.64 \
 vadd_e32_m2.128.64 vadd_e32_m4.128.64 vadd_e32_m8.128.64 vadd_e32_m8.128.64 \
 vadd_e32_mf2.128.64"

# test under development
#make TLIST=vadd_e16_m1.64.32

# Remaining tests
# make clean; make TLIST="vadd_e16_m1.32.16"



