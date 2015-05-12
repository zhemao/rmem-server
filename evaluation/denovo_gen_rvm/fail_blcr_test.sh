#!/bin/bash

RATE=$1

START=$(date +%s.%N)
cr_run ./gen_assemble -i test -h f2 -p 11184 -c 1000000 -s $RATE
while [ $? -ne 0 ]; do
    cr_restart gen.blcr
done
END=$(date +%s.%N)

TOTAL_TIME=$(echo "$END - $START" | bc)
echo $TOTAL_TIME
