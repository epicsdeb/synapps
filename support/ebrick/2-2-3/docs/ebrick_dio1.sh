#!/bin/sh
# EBRICK-II data register blast-o test
# created 2008-01-29
# Written by David Kline (dkline@aps.anl.gov) for EPICS bricks.

## PV variables
count=0
errors=0
dataset=1
attempts=0
rpv="ebr:pos01:reg01:inpb"
wpv="ebr:pos01:reg03:outb"

## Local variants
DATASET="1 2 4 8 16 32 64 128 0 128 64 32 16 8 4 2 1 0 85 170 0"

## Iterate forever writing/reading registers
while true
do
    echo "Dataset:"$dataset" errors:"$errors

    for data in $DATASET
    do
        count=$(($count+1))

        attempts=0
        while true
        do
            attempts=$(($attempts+1))
            caput -t $wpv $data > /dev/null
            getit=`caget -t -n $rpv`

            if [ "$getit" -eq "$data" ]
            then
                break;
            fi
        done

        if [ "$attempts" -gt 1 ]
        then
            errors=$(($errors+1))
            echo "    "$attempts" attempts at interation "$count" to write data "$data
        fi

    done

    dataset=$(($dataset+1))
done
