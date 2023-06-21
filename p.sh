#!/bin/bash
make clean
cd vm
make
cd build/
make tests/vm/mmap-exit.result
make tests/vm/mmap-inherit.result
make tests/vm/mmap-clean.result
cd ..
cd ..