#!/bin/bash

touch input
out=`basename $1`_bench.txt
rm -f $out
TIMEFORMAT='%3U'
min=5000000
max=50000000
inc=2500000
for i in $(seq $min $inc $max)
  do
    echo -n `openssl rand -hex ${i}` > input
    echo -n "`expr 2 '*' ${i}`," >> $out
    (time $1 input) >> $out 2>&1
  done

rm input