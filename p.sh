#!/bin/bash
make clean
cd vm
make
cd build/
make tests/userprog/fork-read.result
cd ..
cd ..