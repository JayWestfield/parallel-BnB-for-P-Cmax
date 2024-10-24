#!/bin/sh
echo 'started'
./dst/experiment 8 20 > ./results/referenceOld.txt 2>&1 &
echo 'ended'
