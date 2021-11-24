#!/bin/bash
rm -f $1_bogus $1.ll $1_bogus.ll  
clang -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names $1.c -o $1.ll
opt -load build/bogus/libBogusFlowPass.* -bogus -S $1.ll -o $1_bogus.ll
clang $1_bogus.ll -o $1_bogus 
./$1_bogus 254 30 18 
opt -dot-cfg $1_bogus.ll
mv .main.dot $1_bogus.dot