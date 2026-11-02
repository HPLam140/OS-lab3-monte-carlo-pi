#!/bin/bash
# Usage: ./run_approach3.sh [N] [RUNS]
# Experiment A: threads scaling, k = 1 (shared update every iteration)
# Experiment B: sync-frequency, fixed thread count, varying k
N=${1:-10000000}
RUNS=${2:-5}
THREADS="1 2 4 8 16 32 64 100"
KS="1 10 100 1000 10000 100000"
CORES=$(nproc)
OUT=results_approach3.csv

gcc -O2 -pthread -o pi_approach3 pi_approach3.c || exit 1

{
  echo "# date: $(date)"
  echo "# cores(nproc): $CORES"
  echo "# n: $N  runs: $RUNS  BASE_SEED: 67"
  lscpu | grep -E "Model name|^CPU\(s\)|Thread|Core" | sed 's/^/# /'
} > machine_info.txt
echo "approach,threads,k,n,time,pi,count,run" > $OUT

echo "Load before measuring: $(cut -d' ' -f1-3 /proc/loadavg)"

# Experiment A
for T in $THREADS; do
  for r in $(seq 1 $RUNS); do
    echo "$(./pi_approach3 $T $N 1),$r" >> $OUT
  done
  echo "A: T=$T done"
done

# Experiment B (T = number of cores)
for K in $KS; do
  for r in $(seq 1 $RUNS); do
    echo "$(./pi_approach3 $CORES $N $K),$r" >> $OUT
  done
  echo "B: k=$K done"
done
echo "Saved $OUT and machine_info.txt"
