#!/bin/bash

out=`basename $1`_bench.txt
rm -f $out
TIMEFORMAT='%3U'
min=1000000
max=10000000
inc=500000
for i in $(seq $min $inc $max)
  do
    echo -n "$i, " >> $out
    echo $i
    
    (time $1 $i) >> /dev/null 2>> $out 
    #(time $1 $i) >> $out 2>&1
  done