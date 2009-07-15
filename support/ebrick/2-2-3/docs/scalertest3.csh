#!/bin/sh
# EBRICK scaler test script
# created 2007-12-14
# Written by David Kline (dkline@aps.anl.gov) for EPICS bricks.

## PV variables
times=0
count=0
PV="32idbUSX:scaler1.CNT"

while true
do
    count=0
    times=$(($times+1))
    echo -n $times " "
    caput -t $PV 1 > /dev/null
    while true
    do
        getit=`caget -t -n $PV`
        if [ "$getit" -eq 0 ]
        then
            echo "."
            break;
        fi

        if [ "$count" -lt 10 ]
        then
            count=$(($count+1))
            echo -n "."
        else
            echo -n "!!! ERROR !!!"
            exit
        fi
    sleep 1
    done
done
