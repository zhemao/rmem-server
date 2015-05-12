#!/bin/bash

RATE=$1

START=$(date +%s.%N)
./dgemv_file -m 1000000 -n 100 -i 100 -c 10 -s $RATE -f out
while [ $? -ne 0 ]; do
    ./dgemv_file -m 1000000 -n 100 -i 100 -c 10 -s $RATE -r -f out
done
END=$(date +%s.%N)

TOTAL_TIME=$(echo "$END - $START" | bc)
echo $TOTAL_TIME
