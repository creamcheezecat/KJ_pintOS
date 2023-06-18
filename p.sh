#!/bin/bash
make clean
cd vm
make
cd build/
make tests/userprog/fork-read.result
make tests/filesys/base/syn-read.result
cd ..
cd ..