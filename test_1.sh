#!/bin/bash
# Usage: ./test_1.sh [number_of_runs]   (default 5)

RUNS=${1:-5}
EXE=./approach_1
OUT=results_approach_1.csv

# Compile with the group's agreed flags
gcc -O2 -o approach_1 approach_1.c || { echo "Compile failed"; exit 1; }

echo "run,time_seconds,pi_estimate" > "$OUT"

for i in $(seq 1 "$RUNS"); do
    output=$($EXE)
    time=$(echo "$output" | grep "Execution Time" | awk '{print $3}')
    pi=$(echo "$output"   | grep "Estimated Pi"   | awk '{print $3}')
    echo "$i,$time,$pi" >> "$OUT"
    echo "Run $i: $time s  (pi = $pi)"
done

# Mean and standard deviation of the times (skip the CSV header)
awk -F, 'NR>1 {t[NR-1]=$2; s+=$2}
         END {n=NR-1; m=s/n;
              for(i=1;i<=n;i++) v+=(t[i]-m)^2;
              printf "\nRuns: %d\nMean: %.4f s\nStd dev: %.4f s\n", n, m, sqrt(v/n)}' "$OUT"