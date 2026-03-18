/*
 * Phase-Level Profiling
 * ═════════════════════
 *
 * PURPOSE:
 *   Measure time spent in each algorithm phase separately to identify
 *   bottlenecks and verify that processing time scales linearly with rounds.
 *
 * METHOD:
 *   Manual instrumentation using QueryPerformanceCounter (Windows).
 *   Measures 4 phases per hash:
 *     Phase 1: Initialization  (initFieldWithDefaultNumbers)
 *     Phase 2: Input Integration (processBuffer)
 *     Phase 3+4: Processing Rounds + Extraction (calculateHashValue)
 *   Tests across multiple input sizes and round counts.
 *   CLI-overridable hash size (default: DEFAULT_BIT_SIZE).
 *
 * CONCLUSION:
 *   Initialization dominates at low round counts. Processing time scales
 *   linearly with rounds. Extraction is negligible.
 *
 * BUILD TARGET: SecasyProfiling
 * HASH SIZE:    DEFAULT_BIT_SIZE (512), CLI-overridable
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

#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

unsigned long numberOfRounds;
int hashLengthInBits = DEFAULT_BIT_SIZE;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

/* ── High-resolution timer ────────────────────────── */

#ifdef _WIN32
static LARGE_INTEGER qpc_freq;
static void timer_init(void) { QueryPerformanceFrequency(&qpc_freq); }
static double timer_us(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)qpc_freq.QuadPart * 1e6;
}
#else
static void timer_init(void) {}
static double timer_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1e6 + (double)tv.tv_usec;
}
#endif

/* ── Helpers ──────────────────────────────────────── */

static uint8_t *random_data(size_t len)
{
    uint8_t *buf = malloc(len);
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
    return buf;
}

static void print_bar(double pct, int width)
{
    int filled = (int)(pct / 100.0 * width + 0.5);
    if (filled > width)
        filled = width;
    printf("  [");
    for (int i = 0; i < width; i++)
        putchar(i < filled ? '#' : ' ');
    printf("] %5.1f%%", pct);
}

/* ══════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    int hash_bits = 512;
    if (argc > 1)
        hash_bits = atoi(argv[1]);
    if (hash_bits < 64 || hash_bits > 512 || hash_bits % 64 != 0)
    {
        fprintf(stderr, "Usage: %s [64|128|256|512]\n", argv[0]);
        return 1;
    }
    hashLengthInBits = hash_bits;

    timer_init();
    srand((unsigned)time(NULL));

    printf("================================================================\n");
    printf("  Secasy Phase-Level Profiling\n");
    printf("  Hash: %d bit, Field: %dx%d (%d cells)\n",
           hash_bits, FIELD_SIZE, FIELD_SIZE, FIELD_SIZE * FIELD_SIZE);
#ifdef _WIN32
    printf("  Timer: QPC %.1f MHz (%.1f ns resolution)\n",
           (double)qpc_freq.QuadPart / 1e6,
           1e9 / (double)qpc_freq.QuadPart);
#endif
    printf("================================================================\n");

    /* ────────────────────────────────────────────────────────
     * Test 1: Phase breakdown for fixed input size, varying rounds
     * ──────────────────────────────────────────────────────── */
    printf("\n=== Test 1: Phase Breakdown (input = 64 bytes) ===\n\n");
    {
        size_t input_len = 64;
        uint8_t *input = random_data(input_len);
        int samples = 500;

        unsigned long round_list[] = {1, 5, 10, 100, 1000, 10000, 100000};
        int n_rounds = 7;

        printf("  %-8s  %10s  %10s  %10s  %10s  | %5s  %5s  %5s\n",
               "Rounds", "Init(us)", "Input(us)", "Proc(us)", "Total(us)",
               "Init%", "Inp%", "Proc%");
        printf("  %-8s  %10s  %10s  %10s  %10s  | %5s  %5s  %5s\n",
               "------", "--------", "--------", "--------", "--------",
               "-----", "-----", "-----");

        for (int ri = 0; ri < n_rounds; ri++)
        {
            numberOfRounds = round_list[ri];

            /* Adjust sample count for slow runs */
            int reps = samples;
            if (round_list[ri] >= 10000)
                reps = 50;
            else if (round_list[ri] >= 1000)
                reps = 100;

            double t_init = 0, t_input = 0, t_proc = 0;

            for (int s = 0; s < reps; s++)
            {
                double t0, t1, t2, t3;

                /* Phase 1: Init */
                t0 = timer_us();
                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                t1 = timer_us();

                /* Phase 2: Input integration */
                processBuffer(input, input_len);
                t2 = timer_us();

                /* Phase 3+4: Processing + Extraction */
                char *h = calculateHashValue();
                t3 = timer_us();
                free(h);

                t_init += (t1 - t0);
                t_input += (t2 - t1);
                t_proc += (t3 - t2);
            }

            double avg_init = t_init / reps;
            double avg_input = t_input / reps;
            double avg_proc = t_proc / reps;
            double avg_total = avg_init + avg_input + avg_proc;

            double pct_init = avg_init / avg_total * 100.0;
            double pct_input = avg_input / avg_total * 100.0;
            double pct_proc = avg_proc / avg_total * 100.0;

            printf("  %-8lu  %10.1f  %10.1f  %10.1f  %10.1f  | %5.1f  %5.1f  %5.1f\n",
                   round_list[ri],
                   avg_init, avg_input, avg_proc, avg_total,
                   pct_init, pct_input, pct_proc);
        }
        free(input);
    }

    /* ────────────────────────────────────────────────────────
     * Test 2: Phase breakdown for fixed rounds, varying input size
     * ──────────────────────────────────────────────────────── */
    printf("\n=== Test 2: Phase Breakdown (rounds = 10) ===\n\n");
    {
        numberOfRounds = 10;
        size_t input_sizes[] = {1, 8, 16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};
        int n_sizes = 11;
        int base_samples = 1000;

        printf("  %-10s  %10s  %10s  %10s  %10s  | %5s  %5s  %5s\n",
               "InputLen", "Init(us)", "Input(us)", "Proc(us)", "Total(us)",
               "Init%", "Inp%", "Proc%");
        printf("  %-10s  %10s  %10s  %10s  %10s  | %5s  %5s  %5s\n",
               "--------", "--------", "--------", "--------", "--------",
               "-----", "-----", "-----");

        for (int si = 0; si < n_sizes; si++)
        {
            size_t len = input_sizes[si];
            uint8_t *input = random_data(len);

            /* Adjust reps for large inputs */
            int reps = base_samples;
            if (len >= 65536)
                reps = 10;
            else if (len >= 4096)
                reps = 50;
            else if (len >= 1024)
                reps = 200;

            double t_init = 0, t_input = 0, t_proc = 0;

            for (int s = 0; s < reps; s++)
            {
                double t0, t1, t2, t3;

                t0 = timer_us();
                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                t1 = timer_us();

                processBuffer(input, len);
                t2 = timer_us();

                char *h = calculateHashValue();
                t3 = timer_us();
                free(h);

                t_init += (t1 - t0);
                t_input += (t2 - t1);
                t_proc += (t3 - t2);
            }
            free(input);

            double avg_init = t_init / reps;
            double avg_input = t_input / reps;
            double avg_proc = t_proc / reps;
            double avg_total = avg_init + avg_input + avg_proc;

            double pct_init = avg_init / avg_total * 100.0;
            double pct_input = avg_input / avg_total * 100.0;
            double pct_proc = avg_proc / avg_total * 100.0;

            /* Format input size */
            char size_str[24];
            if (len >= 1048576)
                snprintf(size_str, sizeof(size_str), "%zuMB", len / 1048576);
            else if (len >= 1024)
                snprintf(size_str, sizeof(size_str), "%zuKB", len / 1024);
            else
                snprintf(size_str, sizeof(size_str), "%zuB", len);

            printf("  %-10s  %10.1f  %10.1f  %10.1f  %10.1f  | %5.1f  %5.1f  %5.1f\n",
                   size_str,
                   avg_init, avg_input, avg_proc, avg_total,
                   pct_init, pct_input, pct_proc);
        }
    }

    /* ────────────────────────────────────────────────────────
     * Test 3: Visual profile (bar chart) for default config
     * ──────────────────────────────────────────────────────── */
    printf("\n=== Test 3: Visual Profile (64B input, r=10) ===\n\n");
    {
        numberOfRounds = 10;
        size_t len = 64;
        uint8_t *input = random_data(len);
        int reps = 5000;

        double t_init = 0, t_input = 0, t_proc = 0;

        for (int s = 0; s < reps; s++)
        {
            double t0 = timer_us();
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            double t1 = timer_us();
            processBuffer(input, len);
            double t2 = timer_us();
            char *h = calculateHashValue();
            double t3 = timer_us();
            free(h);

            t_init += (t1 - t0);
            t_input += (t2 - t1);
            t_proc += (t3 - t2);
        }
        free(input);

        double avg_total = (t_init + t_input + t_proc) / reps;
        double pct_init = t_init / (t_init + t_input + t_proc) * 100.0;
        double pct_input = t_input / (t_init + t_input + t_proc) * 100.0;
        double pct_proc = t_proc / (t_init + t_input + t_proc) * 100.0;

        printf("  Total per hash: %.1f us (N=%d)\n\n", avg_total, reps);

        printf("  Phase 1 (Init):          %6.2f us", t_init / reps);
        print_bar(pct_init, 40);
        printf("\n");

        printf("  Phase 2 (Input):         %6.2f us", t_input / reps);
        print_bar(pct_input, 40);
        printf("\n");

        printf("  Phase 3+4 (Proc+Extr):   %6.2f us", t_proc / reps);
        print_bar(pct_proc, 40);
        printf("\n");
    }

    /* ────────────────────────────────────────────────────────
     * Test 4: Throughput at default settings
     * ──────────────────────────────────────────────────────── */
    printf("\n=== Test 4: Throughput (r=10, %d-bit hash) ===\n\n", hash_bits);
    {
        numberOfRounds = 10;
        int reps = 10000;
        size_t len = 64;

        uint8_t **inputs = malloc(sizeof(uint8_t *) * (size_t)reps);
        for (int i = 0; i < reps; i++)
            inputs[i] = random_data(len);

        double t0 = timer_us();
        for (int i = 0; i < reps; i++)
        {
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(inputs[i], len);
            char *h = calculateHashValue();
            free(h);
        }
        double t1 = timer_us();

        for (int i = 0; i < reps; i++)
            free(inputs[i]);
        free(inputs);

        double total_s = (t1 - t0) / 1e6;
        double per_hash_us = (t1 - t0) / reps;
        double hashes_per_sec = reps / total_s;
        double mb_per_sec = (double)((size_t)reps * len) / total_s / 1048576.0;

        printf("  %d hashes in %.3f s\n", reps, total_s);
        printf("  Per hash:     %.2f us\n", per_hash_us);
        printf("  Throughput:   %.0f hashes/sec\n", hashes_per_sec);
        printf("  Data rate:    %.2f MB/s (at %zuB input)\n", mb_per_sec, len);
    }

    printf("\n================================================================\n");
    printf("  Profiling complete.\n");
    printf("  Note: gprof is not functional on Windows/MinGW (ITIMER_PROF\n");
    printf("  unsupported). This tool provides equivalent phase-level data\n");
    printf("  via QueryPerformanceCounter instrumentation.\n");
    printf("================================================================\n");

    return 0;
}
