#!/bin/sh
#
# Shell script that tests the Generic
# Digital IO based Scaler module using
# the Scaler record.
#


echo ""
echo "Scaler test 2 - reading S7"
for y in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29
{
    for x in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
    {
        echo "Iteration" $y/$x
        sleep 1
        caget ebr:scaler1.S7 >> value2.txt
    }

}
