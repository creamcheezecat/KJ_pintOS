#!/bin/bash
make clean
cd vm
make
cd build/
make tests/vm/pt-write-code2.result
make tests/vm/mmap-read.result
make tests/vm/mmap-close.result
make tests/vm/mmap-unmap.result
make tests/vm/mmap-overlap.result
make tests/vm/mmap-twice.result
make tests/vm/mmap-write.result
cd ..
cd ..