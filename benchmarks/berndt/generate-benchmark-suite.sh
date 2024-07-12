#!/bin/bash

function gen() {
    filename="p_cmax-${6}-n${1}-m${2}-minsize${3}-maxsize${4}-seed${5}.txt"
    echo "Generating $filename ..."
    python3 generate-single-benchmark.py $1 $2 $3 $4 $5 > $filename
    nb_generated=$((nb_generated+1))
}


nb_generated=0

for i in {0..29}; do

for m in 3 4 5; do
    for n in 2 3 5; do
        a=$(($n * $m))
        gen $a $m 1 20 $RANDOM E1
        gen $a $m 20 50 $RANDOM E1
    done
done

for m in 2 3; do
    for n in 10 30 50 100; do
        gen $n $m 100 800 $RANDOM E2
    done
done

for m in 4 6 8 10; do
    for n in 30 50 100; do
        gen $n $m 100 800 $RANDOM E2
    done
done

for m in 3 5 8 10; do
    for n in 3 4 5; do
        a=$(($n * $m + 1))
        gen $a $m 1 100 $RANDOM E3
        gen $a $m 100 200 $RANDOM E3
        a=$(($n * $m + 2))
        gen $a $m 1 100 $RANDOM E3
        gen $a $m 100 200 $RANDOM E3
    done
done

for m in 2; do
    for n in 10; do
        gen $n $m 1 20 $RANDOM E4
        gen $n $m 20 50 $RANDOM E4
        gen $n $m 1 100 $RANDOM E4
        gen $n $m 50 100 $RANDOM E4
        gen $n $m 100 200 $RANDOM E4
        gen $n $m 100 800 $RANDOM E4
    done
done

for m in 3; do
    for n in 9; do
        gen $n $m 1 20 $RANDOM E4
        gen $n $m 20 50 $RANDOM E4
        gen $n $m 1 100 $RANDOM E4
        gen $n $m 50 100 $RANDOM E4
        gen $n $m 100 200 $RANDOM E4
        gen $n $m 100 800 $RANDOM E4
    done
done


for m in 25 50 75 100; do
    a=$((4 * $m))
    gen $a $m 1 1000 $RANDOM BIG
done
done
echo "Generated $nb_generated benchmarks."
