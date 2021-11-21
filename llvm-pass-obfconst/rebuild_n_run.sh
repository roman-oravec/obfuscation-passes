#!/bin/bash
#rm -rf build/*
cd build
cmake ..
make
cd ..
clang -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names foo.c
opt -load build/obfconst/libObfConstPass.* -obfconst -mem2reg -S foo.ll -o foo_obf.ll 
clang foo_obf.ll -o foo_obf
./foo_obf
