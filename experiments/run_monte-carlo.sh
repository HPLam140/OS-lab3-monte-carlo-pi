#!/bin/bash

NPOINTS=10000000
RUNS=3

echo "nThreads,Approach2_Local,Approach3_Shared" > results_6_3.csv

THREADS=(1 2 4 8 12 16)

for t in "${THREADS[@]}"; do
    echo "Running 6.3 for $t threads..."
    t2=$(./monte-carlo 2 $NPOINTS $t $RUNS | cut -d',' -f5)
    t3=$(./monte-carlo 3 $NPOINTS $t $RUNS | cut -d',' -f5)
    echo "$t,$t2,$t3" >> results_6_3.csv
done
