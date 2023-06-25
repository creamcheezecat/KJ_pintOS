#!/bin/bash
make clean
cd vm
make
cd build/
# pintos -v -k -T 600 -m 20   --fs-disk=10 -p tests/vm/page-merge-par:page-merge-par -p tests/vm/child-sort:child-sort --swap-disk=10 -- -q   -f run page-merge-par
# make tests/vm/page-merge-par.result
# pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/page-merge-stk:page-merge-stk -p tests/vm/child-qsort:child-qsort --swap-disk=10 -- -q   -f run page-merge-stk
# make tests/vm/page-merge-stk.result
pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/page-merge-mm:page-merge-mm -p tests/vm/child-qsort-mm:child-qsort-mm --swap-disk=10 -- -q   -f run page-merge-mm
make tests/vm/page-merge-mm.result
# pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/cow/cow-simple:cow-simple --swap-disk=4 -- -q   -f run cow-simple
# make tests/vm/cow/cow-simple.result
cd ..
cd ..