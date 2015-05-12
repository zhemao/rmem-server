#!/bin/bash
set -x

rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 1 -f out
rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 5 -f out
rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 10 -f out
rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 20 -f out
rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 50 -f out
rm dgemv.rec
time ./dgemv_file -m 1000000 -n 100 -i 100 -c 100 -f out
rm dgemv.rec
