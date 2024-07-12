#!/bin/bash

function gen_uniform() {
    filename="p_cmax-${6}-n${1}-m${2}-minsize${3}-maxsize${4}-seed${5}.txt"
    echo "Generating $filename ..."
    python3 uniform.py $1 $2  $3 $4 $5 > $filename
    nb_generated=$((nb_generated+1))
}

function gen_normal() {
    filename="p_cmax-${6}-n${1}-m${2}-mu${3}-sigma${4}-seed${5}.txt"
    echo "Generating $filename ..."
    #echo "python3 normal.py $1 $2 $3 $4 $5"
    python3 normal.py $1 $2 $3 $4 $5 > $filename
    nb_generated=$((nb_generated+1))
}

function gen_all_classes() {
    n=$1
    m=$2
    gen_uniform $n $m 1 100 $RANDOM class1
    gen_uniform $n $m 20 100 $RANDOM class2
    gen_uniform $n $m 50 100 $RANDOM class3
    gen_normal $n $m 100 20 $RANDOM class4
    gen_normal $n $m 100 50 $RANDOM class5
    gen_uniform $n $m $n $((4 * $n)) $RANDOM class6
    gen_normal $n $m $((4 * $n)) $n $RANDOM class7
}


nb_generated=0

for i in {0..9}; do

for n in 20 40 60 80 100 120 140 160 180 200; do
    m=$(($n / 2))
    gen_all_classes $n $m
    m=$((($n * 10) / 25))
    gen_all_classes $n $m
done

for n in 36 54 72 90 108 126 144 162 180 198; do
    m=$(($n / 3))
    gen_all_classes $n $m
    m=$((($n * 100) / 225))
    gen_all_classes $n $m
done

for n in 22 44 66 88 110 132 154 176 198 220; do
    m=$((($n * 100) / 275))
    gen_all_classes $n $m
done
done
echo "Generated $nb_generated benchmarks."
