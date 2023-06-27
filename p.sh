#!/bin/bash
make clean
cd vm
make
cd build/
pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/cow/cow-simple:cow-simple --swap-disk=4 -- -q   -f run cow-simple
make tests/vm/cow/cow-simple.result
cd ..
cd ..