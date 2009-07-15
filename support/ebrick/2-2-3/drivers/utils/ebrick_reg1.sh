#!/bin/sh
# EBRICK-II data register blast-o test
# created 2008-01-29
# Written by David Kline (dkline@aps.anl.gov) for EPICS bricks.

## Local variants
iter=0
count=0
DATASET="1 2 4 8 10 20 40 80 0 80 40 20 10 8 4 2 1 0 55 AA 55 AA 55 AA 0"

## Output banner
echo "Writing addr" $1 "and reading from" $2 "for" $3 "iterations"

## Iterate forever writing/reading registers
while true
do
    iter=$(($iter+1))
    if [ "$iter" -gt "$3" ]
    then
        echo "All write/read tests succeeded at count" $count
        exit
    fi

    for data in $DATASET
    do
        count=$(($count+1))
        app=`launch writeread2 $1 $2 $data`

        if [ "$app" == "BAD" ]
        then
            echo "Write/read failed at count" $count
            exit;
        fi
    done
done
