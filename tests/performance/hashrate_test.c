/*
 * Hashrate Benchmark
 * ══════════════════
 *
 * PURPOSE:
 *   Measure sustained hash throughput (H/s) at the current default
 *   configuration across all supported hash sizes and a range of input
 *   lengths. Gives a single, easy-to-read answer to "how fast is Secasy?"
 *
 * METHOD:
 *   For each (hash_size, input_length) combination the benchmark runs
 *   hashes continuously for MEASURE_SECONDS wall-clock seconds (measured
 *   via clock() for CPU time) and counts how many complete hashes fit in
 *   that window. H/s, µs/hash and MiB/s (throughput over input bytes) are
 *   reported. Results are also written to hashrate_results.csv.
 *
 * CLI:
 *   SecasyHashrate [seconds]          – measurement duration (default 2)
 *
 * BUILD TARGET: SecasyHashrate
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

/* ── Tunables ──────────────────────────────────────────────────────── */
#define DEFAULT_MEASURE_SECONDS 2   /* wall-clock seconds per cell     */
#define WARMUP_HASHES           50  /* throwaway hashes before timing  */

/* Input lengths to sweep (bytes) */
static const int INPUT_SIZES[] = {16, 64, 256, 1024, 4096};
#define NUM_INPUT_SIZES ((int)(sizeof(INPUT_SIZES) / sizeof(INPUT_SIZES[0])))

/* Hash sizes to sweep (bits) */
static const int HASH_SIZES[] = {64, 128, 256, 512};
#define NUM_HASH_SIZES ((int)(sizeof(HASH_SIZES) / sizeof(HASH_SIZES[0])))

/* ── Globals required by Secasy core ───────────────────────────────── */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits         = DEFAULT_BIT_SIZE;

extern Tile_t    field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

/* ── Internal helpers ──────────────────────────────────────────────── */

static char *compute_hash(const uint8_t *data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

/*
 * Measure H/s for a fixed (input_len, hash_bits, rounds) configuration.
 * Returns hashes per second; writes elapsed seconds into *elapsed_out.
 */
static double measure_hashrate(const uint8_t *data, size_t input_len,
                                int hash_bits, unsigned long rounds,
                                double target_seconds, double *elapsed_out)
{
    hashLengthInBits = hash_bits;
    numberOfRounds   = rounds;

    /* Warm-up */
    for (int w = 0; w < WARMUP_HASHES; w++)
    {
        char *h = compute_hash(data, input_len);
        free(h);
    }

    /* Measure */
    long count          = 0;
    clock_t budget      = (clock_t)(target_seconds * (double)CLOCKS_PER_SEC);
    clock_t start       = clock();
    clock_t deadline    = start + budget;

    while (clock() < deadline)
    {
        char *h = compute_hash(data, input_len);
        free(h);
        count++;
    }

    clock_t end      = clock();
    double elapsed   = (double)(end - start) / (double)CLOCKS_PER_SEC;
    *elapsed_out     = elapsed;
    return (elapsed > 0.0) ? (double)count / elapsed : 0.0;
}

/* ── Entry point ───────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    double measure_seconds = DEFAULT_MEASURE_SECONDS;
    if (argc > 1)
    {
        char *end;
        double val = strtod(argv[1], &end);
        if (*end == '\0' && val > 0.0)
            measure_seconds = val;
    }

    srand((unsigned int)time(NULL));

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              Secasy Hash Throughput Benchmark                ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Rounds:        %4lu (DEFAULT_NUMBER_OF_ROUNDS)              ║\n",
           (unsigned long)DEFAULT_NUMBER_OF_ROUNDS);
    printf("║  Measure time:  %4.1f s per cell                             ║\n",
           measure_seconds);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* Allocate worst-case input */
    const int max_input = INPUT_SIZES[NUM_INPUT_SIZES - 1];
    uint8_t *input = malloc((size_t)max_input);
    if (!input)
    {
        fprintf(stderr, "OOM\n");
        return 1;
    }
    for (int i = 0; i < max_input; i++)
        input[i] = (uint8_t)(rand() & 0xFF);

    /* CSV file */
    FILE *csv = fopen("hashrate_results.csv", "w");
    if (csv)
        fprintf(csv, "hash_bits,input_bytes,rounds,hashes_per_sec,us_per_hash,mib_per_sec\n");

    /* Print header row */
    printf("%-10s  %-12s  %12s  %12s  %12s\n",
           "Hash size", "Input (bytes)", "H/s", "µs/hash", "MiB/s (input)");
    printf("%-10s  %-12s  %12s  %12s  %12s\n",
           "----------", "------------", "------------", "------------", "------------");

    for (int hi = 0; hi < NUM_HASH_SIZES; hi++)
    {
        const int hash_bits = HASH_SIZES[hi];

        for (int ii = 0; ii < NUM_INPUT_SIZES; ii++)
        {
            const int input_len = INPUT_SIZES[ii];

            double elapsed;
            double hps = measure_hashrate(input, (size_t)input_len,
                                          hash_bits, DEFAULT_NUMBER_OF_ROUNDS,
                                          measure_seconds, &elapsed);

            double us_per_hash = (hps > 0.0) ? 1e6 / hps : 0.0;
            double mib_per_sec = hps * (double)input_len / (1024.0 * 1024.0);

            printf("%-10d  %-12d  %12.1f  %12.2f  %12.3f\n",
                   hash_bits, input_len, hps, us_per_hash, mib_per_sec);

            if (csv)
                fprintf(csv, "%d,%d,%lu,%.2f,%.4f,%.4f\n",
                        hash_bits, input_len,
                        (unsigned long)DEFAULT_NUMBER_OF_ROUNDS,
                        hps, us_per_hash, mib_per_sec);
        }

        if (hi < NUM_HASH_SIZES - 1)
            printf("\n");
    }

    printf("\n");

    if (csv)
    {
        fclose(csv);
        printf("Results written to hashrate_results.csv\n");
    }

    free(input);
    return 0;
}
