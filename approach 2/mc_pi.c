/*
 * mc_pi.c - Monte Carlo estimation of pi, map-reduce style with pthreads.
 *
 * Build:  gcc -O2 -pthread -o mc_pi mc_pi.c
 * Usage:  ./mc_pi <nPoints> <nThreads> [runs=5] [seed=12345] [mode=random|strat]
 *
 * Modes
 *   random : plain Monte Carlo, error ~ 1/sqrt(n)  (about 4 digits at 1e9 points)
 *   strat  : stratified (jittered) Monte Carlo. The unit square is cut into
 *            G x G cells (G = floor(sqrt(nPoints))) and ONE uniform random
 *            point is drawn inside every cell. Error ~ n^(-3/4), so about
 *            5-6 digits at 1e9 points. Threads take disjoint blocks of rows.
 *
 * Output (one CSV line):
 *   nPointsUsed,nThreads,runs,avg_time,min_time,max_time,pi_mean,abs_error
 *
 * Design
 *   MAP    : thread t draws its own share of points (nPoints/N, remainder
 *            spread over the first threads) using a PRIVATE RNG state and
 *            counts hits in a PRIVATE local variable.
 *   REDUCE : after pthread_join, main sums the per-thread counters.
 *
 * No shared variable is touched inside the hot loop. Each thread's result
 * slot is padded to a full cache line to avoid false sharing when the
 * result is written at the end.
 */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CACHE_LINE 64

/* ---------- per-thread RNG: xoshiro256**, seeded via splitmix64 ---------- */
typedef struct { uint64_t s[4]; } rng_t;

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void rng_seed(rng_t *r, uint64_t seed) {
    for (int i = 0; i < 4; i++) r->s[i] = splitmix64(&seed);
}

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static inline uint64_t rng_next(rng_t *r) {
    uint64_t *s = r->s;
    uint64_t result = rotl(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

/* uniform double in [0,1) using the top 53 bits */
static inline double rng_double(rng_t *r) {
    return (rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* ---------- thread data ---------- */
typedef struct {
    uint64_t nSamples;   /* input  : samples for this thread (random mode) */
    uint64_t seed;       /* input  : unique seed                           */
    uint64_t rowStart;   /* input  : first cell row (strat mode)           */
    uint64_t rowEnd;     /* input  : one past last cell row (strat mode)   */
    uint64_t G;          /* input  : grid size, 0 = random mode            */
    uint64_t hits;       /* output : local count inside circle             */
    char pad[CACHE_LINE - 6 * sizeof(uint64_t)];
} __attribute__((aligned(CACHE_LINE))) task_t;

static void *worker(void *arg) {
    task_t *t = (task_t *)arg;
    rng_t rng;
    rng_seed(&rng, t->seed);          /* independent generator */

    uint64_t local = 0;               /* private counter */
    if (t->G == 0) {                  /* plain Monte Carlo */
        for (uint64_t i = 0; i < t->nSamples; i++) {
            double x = rng_double(&rng);
            double y = rng_double(&rng);
            if (x * x + y * y <= 1.0) local++;
        }
    } else {                          /* stratified: one point per cell */
        const uint64_t G = t->G;
        const double inv = 1.0 / (double)G;
        for (uint64_t row = t->rowStart; row < t->rowEnd; row++) {
            for (uint64_t col = 0; col < G; col++) {
                double x = ((double)col + rng_double(&rng)) * inv;
                double y = ((double)row + rng_double(&rng)) * inv;
                if (x * x + y * y <= 1.0) local++;
            }
        }
    }
    t->hits = local;                  /* single write at the end */
    return NULL;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <nPoints> <nThreads> [runs=5] [seed=12345]\n", argv[0]);
        return 1;
    }
    uint64_t nPoints  = strtoull(argv[1], NULL, 10);
    int      nThreads = atoi(argv[2]);
    int      runs     = (argc > 3) ? atoi(argv[3]) : 5;
    uint64_t baseSeed = (argc > 4) ? strtoull(argv[4], NULL, 10) : 12345ULL;
    int      strat    = (argc > 5) && argv[5][0] == 's';
    uint64_t G = 0;
    if (strat) {
        G = (uint64_t)sqrt((double)nPoints);
        if (G < 1) G = 1;
        nPoints = G * G;              /* points actually used */
    }
    if (nPoints == 0 || nThreads < 1 || runs < 1) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    /* ---- initialisation (NOT timed) ---- */
    pthread_t *tid = malloc(sizeof(pthread_t) * nThreads);
    task_t *tasks = aligned_alloc(CACHE_LINE, sizeof(task_t) * nThreads);
    if (!tid || !tasks) { perror("alloc"); return 1; }

    double sum = 0, tmin = 1e30, tmax = 0;
    double piSum = 0;

    for (int r = 0; r < runs; r++) {
        uint64_t base = nPoints / nThreads, rem = nPoints % nThreads;
        uint64_t rbase = G / nThreads, rrem = G % nThreads, rowCur = 0;
        for (int i = 0; i < nThreads; i++) {
            tasks[i].nSamples = base + ((uint64_t)i < rem ? 1 : 0);
            tasks[i].G = G;
            tasks[i].rowStart = rowCur;
            rowCur += rbase + ((uint64_t)i < rrem ? 1 : 0);
            tasks[i].rowEnd = rowCur;
            /* distinct seed per thread AND per run */
            tasks[i].seed = baseSeed + (uint64_t)r * 1000003ULL + (uint64_t)i * 7919ULL;
            tasks[i].hits = 0;
        }

        /* ---- timed region: create threads, compute, join, reduce ---- */
        double t0 = now_sec();
        for (int i = 0; i < nThreads; i++)
            if (pthread_create(&tid[i], NULL, worker, &tasks[i]) != 0) {
                perror("pthread_create"); return 1;
            }
        for (int i = 0; i < nThreads; i++) pthread_join(tid[i], NULL);

        uint64_t total = 0;                       /* REDUCE */
        for (int i = 0; i < nThreads; i++) total += tasks[i].hits;
        double t1 = now_sec();
        /* ------------------------------------------------------------- */

        double dt = t1 - t0;
        sum += dt;
        if (dt < tmin) tmin = dt;
        if (dt > tmax) tmax = dt;
        piSum += 4.0 * (double)total / (double)nPoints;
    }

    /* single CSV line: all I/O happens after timing */
    double pi = piSum / runs;          /* mean over runs */
    printf("%llu,%d,%d,%.6f,%.6f,%.6f,%.9f,%.3e\n",
           (unsigned long long)nPoints, nThreads, runs,
           sum / runs, tmin, tmax, pi, fabs(pi - 3.14159265358979323846));

    free(tid);
    free(tasks);
    return 0;
}
