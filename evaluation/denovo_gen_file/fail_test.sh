#!/bin/bash

RATE=$1

START=$(date +%s.%N)
./gen_assemble -i test -h f2 -p 11184 -c 100000 -s $RATE
while [ $? -ne 0 ]; do
    ./gen_assemble -i test -h f2 -p 11184 -c 100000 -s $RATE -r
done
END=$(date +%s.%N)

TOTAL_TIME=$(echo "$END - $START" | bc)
echo $TOTAL_TIME
