#!/bin/bash
set -e
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
HOST=$1
PORT=$2

$DIR/rvm_test_full $HOST $PORT -1
$DIR/rvm_test_full $HOST $PORT 0
$DIR/rvm_test_full $HOST $PORT 1
$DIR/rvm_test_full $HOST $PORT 2
$DIR/rvm_test_full $HOST $PORT 3
$DIR/rvm_test_full $HOST $PORT 4
$DIR/rvm_test_full $HOST $PORT 5
$DIR/rvm_test_full $HOST $PORT 6
