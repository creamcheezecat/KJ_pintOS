#!/bin/bash
make clean
cd vm
make
cd build/
pintos -v -k -T 600 -m 40   --fs-disk=10 -p tests/vm/swap-fork:swap-fork -p tests/vm/child-swap:child-swap --swap-disk=200 -- -q   -f run swap-fork
make tests/vm/swap-fork.result
pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/cow/cow-simple:cow-simple --swap-disk=4 -- -q   -f run cow-simple
make tests/vm/cow/cow-simple.result
cd ..
cd ..