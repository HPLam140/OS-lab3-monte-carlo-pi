#!/bin/bash


gcc -O2 -pthread -o mc_pi_6_3 mc_pi_6_3.c

NPOINTS=100000000
RUNS=3

echo "nThreads,ExecutionTime,Speedup" > results_6_2.csv

# Quét dải thread rộng: Nhỏ hơn, Bằng, và Lớn hơn số cores (Oversubscription)
THREADS=(1 2 4 8 12 16 32)

# Lấy thời gian đơn luồng (1 thread) làm chuẩn tính Speedup
T1=$(./monte-carlo 2 $NPOINTS 1 $RUNS | cut -d',' -f5)

for t in "${THREADS[@]}"; do
    echo "Running 6.2 Thread Scaling for $t threads..."
    t2=$(./monte-carlo 2 $NPOINTS $t $RUNS | cut -d',' -f5)
    
    # Tính Speedup = T1 / TN
    speedup=$(echo "scale=4; $T1 / $t2" | bc)
    
    echo "$t,$t2,$speedup" >> results_6_2.csv
done

echo "Hoàn thành mô phỏng 6.2! Kết quả đã ghi vào results_6_2.csv"
