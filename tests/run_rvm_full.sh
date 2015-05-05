#!/bin/bash
set -e
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
$DIR/rvm_test_full f2 12345 -1
$DIR/rvm_test_full f2 12345 0
$DIR/rvm_test_full f2 12345 1
$DIR/rvm_test_full f2 12345 2
$DIR/rvm_test_full f2 12345 3
$DIR/rvm_test_full f2 12345 4
$DIR/rvm_test_full f2 12345 5
$DIR/rvm_test_full f2 12345 6
