#!/bin/bash
set -x

read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 1 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 5 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 10 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 20 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 50 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 100 -f out) &>> $1
read -p "Press ENTER to start"
(time ./dgemv_rvm -m 1000000 -n 100 -h f2 -p 11184 -i 100 -c 10000 -f out) &>> $1




