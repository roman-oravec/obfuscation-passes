#!/bin/bash

touch input
out=`basename $1`_bench.txt
rm -f $out
TIMEFORMAT='%3U'
min=500000
max=5000000
inc=250000
for i in $(seq $min $inc $max)
  do
    echo -n `openssl rand -hex ${i}` > input
    echo -n "`expr 2 '*' ${i}`," >> $out
    echo -n "`expr 2 '*' ${i}`,"
    
    (time $1 input) >> /dev/null 2>> $out 
  done

rm input