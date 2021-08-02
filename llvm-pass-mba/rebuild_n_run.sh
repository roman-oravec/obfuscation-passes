#!/bin/bash
rm -rf build/*
cd build
cmake ..
make
cd ..
clang -S -emit-llvm -Xclang -disable-O0-optnone $1.c -o $1.ll
opt -mem2reg -instnamer -S $1.ll -o $1.ll
opt -load build/mba/libMbaPass.* -mba -rng-seed $RANDOM -S $1.ll -o $1_mba.ll
clang $1_mba.ll -o $1_mba 
./$1_mba 25 10
