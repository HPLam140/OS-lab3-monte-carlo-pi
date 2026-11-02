# OS-lab3-monte-carlo-pi
C project for Monte Carlo Pi estimation using POSIX Threads (pthread), built with GCC and Makefile on Linux/Ubuntu/WSL.


//argu
Please standardize the input arguments for all three approaches.

Bash interface:
bash script/approach_X.sh <POINTS> <RUNS>

$1 = total Monte Carlo sample count (default: 10,000,000)

$2 = number of repetitions (default: 5)

Requirements:

Each approach must use the specified total number of points.

Multi-threaded approaches must divide the total workload among threads, not multiply it by the thread count.

The Bash script should compile and execute the corresponding C program.

The sample count must be configurable without modifying source code.

Keep the output CSV format consistent with your current implementation.

This is necessary so I can benchmark all three approaches using the same workload on one Ubuntu VM.

Approaches 2 and 3 already follow this Bash argument format. Approach 1 needs modification because it currently treats $1 as the number of runs.
