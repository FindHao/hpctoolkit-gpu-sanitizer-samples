#!/bin/bash
rm -rf hpctoolkit-* *csv log
hpcrun -e gpu=nvidia -o hpctoolkit-backprop-measurements ./backprop 65536
hpcstruct --gpucfg yes hpctoolkit-backprop-measurements
time hpcrun -o hpctoolkit-backprop-measurements -ck HPCRUN_SANITIZER_MEM_VIEWS=10 -ck HPCRUN_SANITIZER_PC_VIEWS=10 -e gpu=nvidia,sanitizer@50 ./backprop 65536 &> log
hpcstruct ./backprop
hpcprof -S ./backprop.hpcstruct hpctoolkit-backprop-measurements/
