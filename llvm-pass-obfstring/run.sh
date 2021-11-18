#!/bin/bash
rm -f $1.ll $1_obfstring $1_obfstring.ll
clang -S -emit-llvm -Xclang -disable-O0-optnone $1.c -o $1.ll
opt -mem2reg -instnamer -S $1.ll -o $1.ll
opt -load-pass-plugin=build/obfstring/libObfStringPass.so -passes="obfstring" -S $1.ll -o $1_obfstring.ll 
#opt -load=build/obfstring/libObfStringPass.so -obfstring -codec "codec2.bc" -S $1.ll -o $1_obfstring.ll
clang $1_obfstring.ll -o $1_obfstring
./$1_obfstring 
