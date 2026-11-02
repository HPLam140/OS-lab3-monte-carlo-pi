/*
 * Approach 3 - Monte Carlo pi, shared counter protected by a mutex.
 *
 * Usage: ./pi_approach3 <threads> <n> [k]
 *   threads : number of worker threads (T)
 *   n       : TOTAL number of random points (N), fixed for all experiments
 *   k       : flush period (default 1).
 *             k = 1  -> lock the mutex and update the shared counter EVERY iteration
 *             k > 1  -> keep a private counter, lock + update the shared counter
 *                       once every k iterations (and once more for the remainder)
 *
 * Output (one CSV line, printed AFTER the timer stops):
 *   approach3,<threads>,<k>,<n>,<time_sec>,<pi>,<count>
 *
 * Seeding rule (must match Approach 2 so that counts are identical):
 *   thread id has seed = BASE_SEED + id,  id = 0 .. T-1
 *   thread id handles n/T points, the first (n % T) threads take 1 extra point.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define BASE_SEED 67u

static long long g_count = 0;                       /* shared counter */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

struct Task {
    int       id;
    long long points;
    long long k;
};

static void *worker(void *arg) {
    struct Task *t = (struct Task *)arg;
    unsigned int seed = BASE_SEED + (unsigned int)t->id;   /* private seed */
    long long local = 0;          /* hits since the last flush */
    long long since = 0;          /* iterations since the last flush */

    for (long long i = 0; i < t->points; i++) {
        double x = (double)rand_r(&seed) / RAND_MAX;
        double y = (double)rand_r(&seed) / RAND_MAX;
        if (x * x + y * y <= 1.0)
            local++;
        since++;

        if (since == t->k) {                      /* time to update shared var */
            pthread_mutex_lock(&g_lock);
            g_count += local;
            pthread_mutex_unlock(&g_lock);
            local = 0;
            since = 0;
        }
    }
    if (since > 0) {                              /* flush the remainder */
        pthread_mutex_lock(&g_lock);
        g_count += local;
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);              /* wall-clock, not CPU time */
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <threads> <n> [k]\n", argv[0]);
        return 1;
    }
    char *e1, *e2, *e3;
    long T = strtol(argv[1], &e1, 10);
    long long n = strtoll(argv[2], &e2, 10);
    long long k = 1;
    if (argc == 4) k = strtoll(argv[3], &e3, 10);
    if (*e1 || *e2 || (argc == 4 && *e3) || T <= 0 || n <= 0 || k <= 0) {
        fprintf(stderr, "Usage: %s <threads> <n> [k]  (all positive integers)\n", argv[0]);
        return 1;
    }
    if (T > n) T = (long)n;

    /* ---- initialization (NOT timed) ---- */
    pthread_t   *th = malloc(T * sizeof(pthread_t));
    struct Task *tk = malloc(T * sizeof(struct Task));
    if (!th || !tk) { perror("malloc"); return 1; }
    long long base = n / T, rem = n % T;
    for (long i = 0; i < T; i++) {
        tk[i].id = (int)i;
        tk[i].points = base + (i < rem ? 1 : 0);
        tk[i].k = k;
    }

    /* ---- timed region: create threads -> all joined ---- */
    double t0 = now_sec();
    for (long i = 0; i < T; i++) {
        if (pthread_create(&th[i], NULL, worker, &tk[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (long i = 0; i < T; i++)
        pthread_join(th[i], NULL);
    double t1 = now_sec();

    /* ---- output AFTER the timer stops ---- */
    double pi = 4.0 * (double)g_count / (double)n;
    printf("approach3,%ld,%lld,%lld,%.6f,%.6f,%lld\n", T, k, n, t1 - t0, pi, g_count);

    pthread_mutex_destroy(&g_lock);
    free(th);
    free(tk);
    return 0;
}
