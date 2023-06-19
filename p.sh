#!/bin/bash
make clean
cd vm
make
cd build/
make tests/filesys/base/syn-read.result
cd ..
cd ..