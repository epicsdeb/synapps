#!/bin/sh
# EBRICK-II data register bit blast-o test
# created 2008-01-30
# Written by David Kline (dkline@aps.anl.gov) for EPICS bricks.

## PV variables
count=0
errors=0
dataset=1
attempts=0
rpv="ebr:pos01:reg01:bi0"
wpv="ebr:pos01:reg03:bo0"

## Local variants
DATASET="1 0"
BITSET="1 2 3 4 5 6 7 8 7 6 5 4 3 2"

## Iterate forever writing/reading registers
while true
do
    echo "Bitset:"$dataset" errors:"$errors

    for bit in $BITSET
    do

        for data in $DATASET
        do
            count=$(($count+1))

            attempts=0
            while true
            do
                attempts=$(($attempts+1))
                caput -t -n $wpv$bit $data >> /dev/null
                getit=`caget -t -n $rpv$bit`

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

    done

    dataset=$(($dataset+1))
done
