#!/bin/bash
rm -rf build/*
rm -f $1.ll $1_obfstring $1_obfstring.ll
cd build
cmake ..
make
cd ..
# Compile codec.bc if supplied as 2nd arg
[ ! -z "$2" ] && clang -c -emit-llvm $2 -o codec.bc

clang -S -emit-llvm -Xclang -disable-O0-optnone $1.c -o $1.ll
#opt -load-pass-plugin=build/obfstring/libObfStringPass.so -passes="obfstring" -S $1.ll -o $1_obfstring.ll
opt -load=build/obfstring/libObfStringPass.so -obfstring -S $1.ll -o $1_obfstring.ll
clang $1_obfstring.ll -o $1_obfstring
./$1_obfstring 
