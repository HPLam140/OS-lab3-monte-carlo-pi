#include <stdio.h>
#include <stdlib.h>
#include <time.h>






#define NUM_POINTS 10000000 // 10^7 points as recommended in the lab






int main()
{
    long long inside_circle = 0;
    
    // 1. Use a fixed random seed to ensure reproducibility
    srand(42);

    // 2. Start measuring execution time
    clock_t start_time = clock();

    // 3. Main Monte Carlo loop
    for (long long i = 0; i < NUM_POINTS; i++) {
        // Generate random x and y between 0.0 and 1.0
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;

        // Check if the point falls inside the quarter circle (x^2 + y^2 <= 1)
        if ((x * x + y * y) <= 1.0) {
            inside_circle++;
        }
    }

    // 4. Stop measuring execution time
    clock_t end_time = clock();

    // Calculate elapsed time in seconds
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Estimate Pi
    double pi_estimate = 4.0 * (double)inside_circle / (double)NUM_POINTS;

    // 5. Print results AFTER the loop (avoiding I/O inside loops to prevent timing distortion)
    printf("--- Approach 1: Single Thread Baseline ---\n");
    printf("Total Points:     %lld\n", NUM_POINTS);
    printf("Points Inside:    %lld\n", inside_circle);
    printf("Estimated Pi:     %f\n", pi_estimate);
    printf("Execution Time:   %f seconds\n", elapsed_time);

    return 0;
}