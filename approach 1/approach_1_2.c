#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>





#define NUM_POINTS 100000000000LL   // 10^11, as the group decided (LL so it matches %lld)
#define SEED       67



static inline unsigned long long next_u64(unsigned long long *s)
{
    unsigned long long x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 2685821657736338717ULL;
}
static inline double rand01(unsigned long long *s)
{
    return (next_u64(s) >> 11) * (1.0 / 9007199254740992.0);  // [0,1), 53 bits
}





int main(void)
{
    setlocale(LC_NUMERIC, "en_US.UTF-8");   // enables the ' flag; falls back silently if unavailable

    long long inside_circle = 0;
    unsigned int seed = SEED;

    unsigned long long state = 67ULL * 0x9E3779B97F4A7C15ULL;  // nonzero, scrambled seed (still fixed)

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    
    for (long long i = 0; i < NUM_POINTS; i++)
    {
        double x = rand01(&state);
        double y = rand01(&state);
        //double x = (double)rand_r(&seed) / RAND_MAX;
        //double y = (double)rand_r(&seed) / RAND_MAX;
        if (x * x + y * y <= 1.0)
            inside_circle++;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    double pi_estimate = 4.0 * (double)inside_circle / (double)NUM_POINTS;

    printf("--- Approach 1: Single Thread Baseline ---\n");
    printf("Total Points:   %'lld\n", NUM_POINTS);
    printf("Points Inside:  %'lld\n", inside_circle);
    printf("Estimated Pi:   %.10f\n", pi_estimate);
    printf("Execution Time: %.10f seconds\n", elapsed);
    return 0;
}
