#!/bin/bash
rm -rf build/*
cd build
cmake ..
make
cd ..
clang -S -emit-llvm -Xclang -disable-O0-optnone $1.c -o $1.ll
opt -mem2reg -instnamer -S $1.ll -o $1.ll
opt -load build/bogus/libBogusFlowPass.* -bogus -rng-seed $RANDOM -S $1.ll -o $1_bogus.ll
clang $1_bogus.ll -o $1_bogus 
./$1_bogus 25 10
