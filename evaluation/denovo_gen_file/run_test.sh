#!/bin/bash

OUT=$1
echo No Failure Checkpoint 100k | tee -a $OUT
read -p "Press [Enter] key to start"
(time cr_run ./gen_assemble -i test 11184 -c 100000) &>> $OUT

echo No Failure Checkpoint 1M | tee -a $OUT
read -p "Press [Enter] key to start"
(time cr_run ./gen_assemble -i test -c 1000000) &>> $OUT

echo No Failure Checkpoint 4M \(2 total\) | tee -a $OUT
read -p "Press [Enter] key to start"
(time cr_run ./gen_assemble -i test -c 4000000) &>> $OUT

echo No failures or checkpointing | tee -a $OUT
read -p "Press [Enter] key to start"
(time cr_run ./gen_assemble -i test) &>> $OUT

echo Unmodified Benchmark | tee -a $OUT
(time cr_run ../denovo_gen/gen_assemble test) &>> $OUT

echo No Failure Checkpoint 10k | tee -a $OUT
read -p "Press [Enter] key to start"
(time cr_run ./gen_assemble -i test -c 10000) &>> $OUT


