#!/bin/sh
#
# Shell script that tests the Generic
# Digital IO based Scaler module using
# the Scaler record.
#


echo ""
echo "Scaler test"
for y in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
{
    echo "Master iteration" $y

    echo ""
    echo "Scaler done/count test 1 (preset)"
    caput ebr:scaler1.PR6 73 >> junk.txt
    caput ebr:scaler1.G6 1 >> junk.txt
    for x in 0 1 2 3 4 5 6 7 8 9
    {
        echo "Test 1 iteration" $y "-" $x
        sleep 4
        caput ebr:scaler1.CNT 1 >> junk.txt
        sleep 2
        caput ebr:scaler1.CNT 0 >> junk.txt
    }

    sleep 3

    echo ""
    echo "Scaler done/count test 2 (no preset)"
    caput ebr:scaler1.PR6 0 >> junk.txt
    caput ebr:scaler1.G6 0 >> junk.txt
    for x in 0 1 2 3 4 5 6 7 8 9
    {
        echo "Test 2 iteration" $y "-" $x
        sleep 3
        caput ebr:scaler1.CNT 1 >> junk.txt
        sleep 2
        caput ebr:scaler1.CNT 0 >> junk.txt
    }
}

rm junk.txt
