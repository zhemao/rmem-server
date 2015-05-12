#!/bin/bash

while getopts ":r" opt; do
    case $opt in
        r)
            RECOVER=-r
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            ;;
    esac
done

echo setarch $(uname -m) -R ./gen_assemble -i test -h f2 -p 11184 -c 1000000 $RECOVER &
setarch $(uname -m) -R ./gen_assemble -i test -h f2 -p 11184 -c 1000000 $RECOVER &

TEST_PID=$!
sleep 15
kill -9 $TEST_PID
