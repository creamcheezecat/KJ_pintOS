#!/bin/bash
make clean
cd vm
make
cd build/
make tests/userprog/no-vm/multi-oom.result
cd ..
cd ..