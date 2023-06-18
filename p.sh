#!/bin/bash
make clean
cd vm
make
cd build/
make tests/userprog/fork-read.result
make tests/userprog/fork-close.result
make tests/userprog/fork-boundary.result
make tests/userprog/exec-boundary.result
make tests/userprog/exec-read.result
cd ..
cd ..