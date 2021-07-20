#!/bin/bash
rm -rf build/*
cd build
cmake ..
make
cd ..
clang -S -emit-llvm -Xclang -disable-O0-optnone foo.c
opt-9 -load build/obfzero/libZeroPass.* -obfzero -S foo.ll -o foo_obf.ll
