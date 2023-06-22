#!/bin/bash
make clean
cd vm
make
cd build/
pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/mmap-exit:mmap-exit -p tests/vm/child-mm-wrt:child-mm-wrt --swap-disk=4 -- -q   -f run mmap-exit
make tests/vm/mmap-exit.result
pintos -v -k -T 120 -m 20   --fs-disk=10 -p tests/vm/page-merge-mm:page-merge-mm -p tests/vm/child-qsort-mm:child-qsort-mm --swap-disk=10 -- -q   -f run page-merge-mm
make tests/vm/page-merge-mm.result
cd ..
cd ..