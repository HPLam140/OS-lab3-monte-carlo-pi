
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CACHE_LINE 64

/* ---------- Per-thread RNG: xoshiro256** (Từ Source của bạn) ---------- */
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

static inline double rng_double(rng_t *r) {
    return (rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* ---------- Biến dùng chung cho Approach 3 ---------- */
static uint64_t global_shared_hits = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Struct Task (Căn chỉnh bộ nhớ chống False Sharing) ---------- */
typedef struct {
    uint64_t nSamples;   /* input  : samples for this thread */
    uint64_t seed;       /* input  : unique seed             */
    uint64_t hits;       /* output : local count inside circle */
    int approach;        /* 2 (Local) hoặc 3 (Shared) */
    char pad[CACHE_LINE - 3 * sizeof(uint64_t) - sizeof(int)];
} __attribute__((aligned(CACHE_LINE))) task_t;

/* ---------- Worker Function ---------- */
static void *worker(void *arg) {
    task_t *t = (task_t *)arg;
    rng_t rng;
    rng_seed(&rng, t->seed);

    if (t->approach == 2) {
        /* 
         * APPROACH 2: Local Accumulation (Map-Reduce)
         * Đếm điểm vào biến cục bộ trong private stack frame của luồng.
         * Không chạm vào miền nhớ chung trong hot loop.
         */
        uint64_t local = 0;
        for (uint64_t i = 0; i < t->nSamples; i++) {
            double x = rng_double(&rng);
            double y = rng_double(&rng);
            if (x * x + y * y <= 1.0) local++;
        }
        t->hits = local; /* Đưa kết quả ra duy nhất 1 lần khi kết thúc */
    } 
    else if (t->approach == 3) {
        /* 
         * APPROACH 3: Shared Accumulation (Mutex Lock)
         * Cập nhật trực tiếp biến dùng chung ngay khi trúng điểm.
         */
        for (uint64_t i = 0; i < t->nSamples; i++) {
            double x = rng_double(&rng);
            double y = rng_double(&rng);
            if (x * x + y * y <= 1.0) {
                pthread_mutex_lock(&lock);
                global_shared_hits++;
                pthread_mutex_unlock(&lock);
            }
        }
    }
    return NULL;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <approach: 2|3> <nPoints> <nThreads> [runs=5] [seed=12345]\n", argv[0]);
        return 1;
    }

    int      approach = atoi(argv[1]);
    uint64_t nPoints  = strtoull(argv[2], NULL, 10);
    int      nThreads = atoi(argv[3]);
    int      runs     = (argc > 4) ? atoi(argv[4]) : 5;
    uint64_t baseSeed = (argc > 5) ? strtoull(argv[5], NULL, 10) : 12345ULL;

    if ((approach != 2 && approach != 3) || nPoints == 0 || nThreads < 1 || runs < 1) {
        fprintf(stderr, "Lỗi: Tham số truyền vào không hợp lệ!\n");
        return 1;
    }

    pthread_t *tid = malloc(sizeof(pthread_t) * nThreads);
    task_t *tasks = aligned_alloc(CACHE_LINE, sizeof(task_t) * nThreads);
    if (!tid || !tasks) { perror("alloc"); return 1; }

    double sum = 0, tmin = 1e30, tmax = 0;
    double pi = 0;

    for (int r = 0; r < runs; r++) {
        global_shared_hits = 0;
        uint64_t base = nPoints / nThreads, rem = nPoints % nThreads;

        for (int i = 0; i < nThreads; i++) {
            tasks[i].nSamples = base + ((uint64_t)i < rem ? 1 : 0);
            tasks[i].seed     = baseSeed + (uint64_t)r * 1000003ULL + (uint64_t)i * 7919ULL;
            tasks[i].hits     = 0;
            tasks[i].approach = approach;
        }

        double t0 = now_sec();

        for (int i = 0; i < nThreads; i++) {
            if (pthread_create(&tid[i], NULL, worker, &tasks[i]) != 0) {
                perror("pthread_create"); return 1;
            }
        }

        for (int i = 0; i < nThreads; i++) pthread_join(tid[i], NULL);

        /* Tổng hợp dữ liệu (REDUCE) */
        uint64_t total = 0;
        if (approach == 3) {
            total = global_shared_hits;
        } else {
            for (int i = 0; i < nThreads; i++) total += tasks[i].hits;
        }

        double t1 = now_sec();
        double dt = t1 - t0;
        sum += dt;
        if (dt < tmin) tmin = dt;
        if (dt > tmax) tmax = dt;
        pi = 4.0 * (double)total / (double)nPoints;
    }

    /* Định dạng đầu ra CSV khớp hoàn toàn với code mẫu của bạn */
    printf("%d,%llu,%d,%d,%.6f,%.6f,%.6f,%.8f\n",
           approach, (unsigned long long)nPoints, nThreads, runs,
           sum / runs, tmin, tmax, pi);

    free(tid);
    free(tasks);
    pthread_mutex_destroy(&lock);
    return 0;
}
