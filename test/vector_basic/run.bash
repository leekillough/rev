#!/bin/bash

rm -rf output-spike
rm -rf output-rev

echo Running Spike
make clean; make -s -j USE_SPIKE=1
if [ $? -ne 0]; then
    echo "Spike failed: see output/"
    exit 1
fi
mv output output-spike
echo "Spike passed: see output-spike"

echo Running Rev

# Production
make clean; make -j -s
# Development
# make clean; make TLIST="csr.128.64 csr.64.32 vadd_e16_m1.128.64 vadd_e16_m1.32.16 vadd_e16_m1.64.32 vadd_e16_mf4.128.64 vadd_e32_m1.128.64 vadd_e32_m2.128.64 vadd_e32_m4.128.64 vadd_e32_m8.128.64 vadd_e32_m8_unaligned.128.64 vadd_e32_mf2.128.64 vadd_e64.m1.128.64 vadd_e8_m1.128.64 vadd_e8_mf2.128.64 vadd_e8_mf4.128.64 vadd_e8_mf8.128.64 vec-strcmp.128.64 vec-daxpy.128.64 vec-sgemm.128.64 vec-mixed.128.64"

if [ $? -ne 0 ]; then
    echo "Rev failed: see output/"
    exit 1
fi

# test under development
# make clean; make TLIST=vec-cond.128.64

mv output output-rev
echo "Rev passed: see output-rev"



