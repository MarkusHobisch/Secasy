/**
 * Precise Hash Timing Test
 *
 * Uses QueryPerformanceCounter (Windows) for nanosecond-precision timing.
 * Measures:
 *   1. Init overhead (initFieldWithDefaultNumbers alone)
 *   2. Single hash at various round counts
 *   3. Bulk hashes to get stable average
 *
 * Usage: SecasyPreciseTiming [hash_bits]  (default: 512)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"
#include "../../util.h"

#define INPUT_LEN 16

unsigned long numberOfRounds;
int hashLengthInBits = 512;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

/* ── High-resolution timer ────────────────────────── */

#ifdef _WIN32
static LARGE_INTEGER qpc_freq;

static void timer_init(void)
{
    QueryPerformanceFrequency(&qpc_freq);
}

static double timer_now_us(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)qpc_freq.QuadPart * 1e6;
}
#else
static void timer_init(void) {}
static double timer_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1e6 + (double)tv.tv_usec;
}
#endif

/* ── Hash helper ──────────────────────────────────── */

static char *hash_buffer(const uint8_t *data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

/* ══════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    int hash_bits = 512;
    if (argc > 1) hash_bits = atoi(argv[1]);
    if (hash_bits < 64 || hash_bits > 512 || hash_bits % 64 != 0) {
        fprintf(stderr, "Invalid: %d (use 64, 128, 256, 512)\n", hash_bits);
        return 1;
    }
    hashLengthInBits = hash_bits;

    timer_init();
    srand((unsigned)time(NULL));

    printf("================================================================\n");
    printf("  Precise Timing Test (QueryPerformanceCounter)\n");
    printf("  Hash: %d bit, Timer resolution: ", hash_bits);

#ifdef _WIN32
    printf("%.3f ns (%.1f MHz)\n",
           1e9 / (double)qpc_freq.QuadPart,
           (double)qpc_freq.QuadPart / 1e6);
#else
    printf("~1 us (gettimeofday)\n");
#endif

    printf("================================================================\n\n");

    /* ── Test 1: Init overhead alone ──────────────── */
    printf("--- Test 1: initFieldWithDefaultNumbers overhead ---\n");
    {
        int N = 10000;
        double t0 = timer_now_us();
        for (int i = 0; i < N; i++) {
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        }
        double t1 = timer_now_us();
        printf("  %d calls: %.1f us total, %.3f us/call\n\n",
               N, t1 - t0, (t1 - t0) / N);
    }

    /* ── Test 2: Single hash (warm cache) ─────────── */
    printf("--- Test 2: Single hash time at various rounds ---\n");
    {
        uint8_t input[INPUT_LEN];
        for (int j = 0; j < INPUT_LEN; j++)
            input[j] = (uint8_t)(rand() & 0xFF);

        unsigned long rounds_list[] = {100000, 10000, 1000, 100, 50, 20, 10, 5, 1};
        int n_rounds = 9;

        /* Warm up */
        numberOfRounds = 10;
        char *w = hash_buffer(input, INPUT_LEN);
        free(w);

        printf("  %-10s  %-15s  %-10s\n", "Rounds", "Time (us)", "Speedup");
        printf("  %-10s  %-15s  %-10s\n", "------", "--------", "-------");

        double base_us = 0;
        for (int i = 0; i < n_rounds; i++) {
            numberOfRounds = rounds_list[i];

            /* For high round counts, measure 1 hash; for low, measure many and average */
            int reps;
            if (rounds_list[i] >= 10000) reps = 1;
            else if (rounds_list[i] >= 100) reps = 10;
            else reps = 100;

            double t0 = timer_now_us();
            for (int r = 0; r < reps; r++) {
                char *h = hash_buffer(input, INPUT_LEN);
                free(h);
            }
            double t1 = timer_now_us();
            double per_hash = (t1 - t0) / reps;

            if (i == 0) base_us = per_hash;
            printf("  %-10lu  %12.1f us   %8.0fx\n",
                   rounds_list[i], per_hash, base_us / per_hash);
        }
    }

    /* ── Test 3: Bulk measurement (10,000 hashes at 10 rounds) ── */
    printf("\n--- Test 3: Bulk measurement (statistical average) ---\n");
    {
        unsigned long test_rounds[] = {100000, 10000, 1000, 100, 10};
        int test_samples[]          = {    10,   100, 1000, 10000, 10000};
        int n = 5;

        printf("  %-10s  %-8s  %-15s  %-15s  %-10s\n",
               "Rounds", "Samples", "Total (ms)", "Per Hash (us)", "Speedup");
        printf("  %-10s  %-8s  %-15s  %-15s  %-10s\n",
               "------", "-------", "----------", "-------------", "-------");

        double base_us = 0;
        for (int i = 0; i < n; i++) {
            numberOfRounds = test_rounds[i];
            int samples = test_samples[i];

            /* Generate random inputs */
            uint8_t (*inputs)[INPUT_LEN] = malloc((size_t)samples * INPUT_LEN);
            for (int s = 0; s < samples; s++)
                for (int j = 0; j < INPUT_LEN; j++)
                    inputs[s][j] = (uint8_t)(rand() & 0xFF);

            double t0 = timer_now_us();
            for (int s = 0; s < samples; s++) {
                char *h = hash_buffer(inputs[s], INPUT_LEN);
                free(h);
            }
            double t1 = timer_now_us();
            free(inputs);

            double total_ms = (t1 - t0) / 1000.0;
            double per_hash_us = (t1 - t0) / samples;
            if (i == 0) base_us = per_hash_us;

            printf("  %-10lu  %-8d  %12.2f ms  %12.2f us  %8.0fx\n",
                   test_rounds[i], samples, total_ms, per_hash_us,
                   base_us / per_hash_us);
        }
    }

    /* ── Test 4: Estimate feasibility of large sample counts ── */
    printf("\n--- Test 4: Feasibility projection for large sample counts ---\n");
    {
        numberOfRounds = 10;

        /* Measure 10,000 hashes precisely */
        int N = 10000;
        uint8_t (*inputs)[INPUT_LEN] = malloc((size_t)N * INPUT_LEN);
        for (int s = 0; s < N; s++)
            for (int j = 0; j < INPUT_LEN; j++)
                inputs[s][j] = (uint8_t)(rand() & 0xFF);

        double t0 = timer_now_us();
        for (int s = 0; s < N; s++) {
            char *h = hash_buffer(inputs[s], INPUT_LEN);
            free(h);
        }
        double t1 = timer_now_us();
        free(inputs);

        double per_hash_us = (t1 - t0) / N;
        printf("  Measured: 10 rounds, %d hashes -> %.2f us/hash\n\n", N, per_hash_us);

        printf("  Projected times for 10-round hashes:\n");
        int counts[] = {1000, 10000, 50000, 100000, 500000, 1000000};
        for (int i = 0; i < 6; i++) {
            double sec = counts[i] * per_hash_us / 1e6;
            printf("    %10d hashes -> %8.1f sec (%5.1f min)\n",
                   counts[i], sec, sec / 60.0);
        }

        printf("\n  Projected times for 100000-round hashes (reference):\n");
        numberOfRounds = 100000;
        /* Measure 3 hashes */
        uint8_t inp[INPUT_LEN];
        for (int j = 0; j < INPUT_LEN; j++) inp[j] = (uint8_t)(rand() & 0xFF);

        double t2 = timer_now_us();
        for (int r = 0; r < 3; r++) {
            char *h = hash_buffer(inp, INPUT_LEN);
            free(h);
        }
        double t3 = timer_now_us();
        double per_100k = (t3 - t2) / 3.0;
        printf("    Measured: %.1f us/hash (%.1f ms)\n", per_100k, per_100k / 1000.0);

        for (int i = 0; i < 6; i++) {
            double sec = counts[i] * per_100k / 1e6;
            double hours = sec / 3600.0;
            if (hours > 1.0)
                printf("    %10d hashes -> %8.1f hours\n", counts[i], hours);
            else
                printf("    %10d hashes -> %8.1f sec (%5.1f min)\n",
                       counts[i], sec, sec / 60.0);
        }
    }

    printf("\n================================================================\n");
    printf("  DONE\n");
    printf("================================================================\n");

    return 0;
}
