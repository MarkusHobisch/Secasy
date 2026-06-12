/*
 * Cell Divergence Analysis
 * ========================
 *
 * PURPOSE:
 *   Measures how many grid cells differ between two inputs after each
 *   successive input byte during Phase 2 (fingerprint formation).
 *
 *   For each trial a pair of random messages (A, B) differing in a single
 *   bit is fed byte-by-byte into the Secasy grid.  After processing each
 *   byte the full 16x16 grid states are compared and the number of cells
 *   whose (value, primeIndex, colorIndex) tuples differ is recorded.
 *
 *   This quantifies the "cell Hamming distance" growth curve: HDC(n) is
 *   the number of different cells after n input bytes.
 *
 * OUTPUT:
 *   CSV on stdout:  byte_index, mean_diff_cells, min, max, stddev
 *   (one row per byte position, averaged over all trials)
 *
 * BUILD TARGET: SecasyCellDivergence
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>

#include "Defines.h"
#include "InitializationPhase.h"

/* ── Globals required by the Secasy core ─────────────────────────── */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

/* Access the grid defined in InitializationPhase.c */
extern Tile field[FIELD_SIZE][FIELD_SIZE];

/* ── Test parameters ─────────────────────────────────────────────── */
#define N_TRIALS 200                          /* pairs of messages to compare        */
#define MSG_LEN 128                           /* bytes per message                   */
#define TOTAL_CELLS (FIELD_SIZE * FIELD_SIZE) /* 256             */

/* ── Tiny xorshift64 RNG ────────────────────────────────────────── */
static uint64_t rng_state = 0;
static uint64_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* ── Snapshot: flat copy of the full grid state ──────────────────── */
typedef struct
{
    uint64_t value;
    uint32_t primeIndex;
    ColorIndex colorIndex;
} CellSnapshot_t;

static void snapshot_grid(CellSnapshot_t snap[TOTAL_CELLS])
{
    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            int idx = x * FIELD_SIZE + y;
            snap[idx].value = field[x][y].value;
            snap[idx].primeIndex = field[x][y].primeIndex;
            snap[idx].colorIndex = field[x][y].colorIndex;
        }
    }
}

static int count_diffs(const CellSnapshot_t a[TOTAL_CELLS],
                       const CellSnapshot_t b[TOTAL_CELLS])
{
    int diffs = 0;
    for (int i = 0; i < TOTAL_CELLS; i++)
    {
        if (a[i].value != b[i].value ||
            a[i].primeIndex != b[i].primeIndex ||
            a[i].colorIndex != b[i].colorIndex)
        {
            diffs++;
        }
    }
    return diffs;
}

/* Process a single byte through the grid (wrapper around processBuffer). */
static void feed_one_byte(unsigned char byte)
{
    processBuffer(&byte, 1);
}

/* Accept optional seed and flip-byte position from command line.
 * Usage: SecasyCellDivergence [seed] [flip_byte]
 *   seed      — RNG seed (hex or decimal), default 0xDEADBEEFCAFE1234
 *   flip_byte — byte position in [0..MSG_LEN-1] where the bit is flipped,
 *               default 0  */
int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        char *end = NULL;
        uint64_t user_seed = (uint64_t)strtoull(argv[1], &end, 0);
        if (end != argv[1] && user_seed != 0)
            rng_state = user_seed;
        else
            rng_state = 0xDEADBEEFCAFE1234ULL;
    }
    else
    {
        rng_state = 0xDEADBEEFCAFE1234ULL;
    }

    int flip_byte = 0;
    if (argc > 2)
    {
        flip_byte = atoi(argv[2]);
        if (flip_byte < 0 || flip_byte >= MSG_LEN)
        {
            fprintf(stderr, "flip_byte must be in [0..%d]\n", MSG_LEN - 1);
            return 1;
        }
    }

    /* Accumulators per byte position */
    double sum[MSG_LEN];
    double sum_sq[MSG_LEN];
    int minv[MSG_LEN];
    int maxv[MSG_LEN];

    for (int b = 0; b < MSG_LEN; b++)
    {
        sum[b] = 0.0;
        sum_sq[b] = 0.0;
        minv[b] = TOTAL_CELLS + 1;
        maxv[b] = -1;
    }

    CellSnapshot_t snap_a[TOTAL_CELLS];
    CellSnapshot_t snap_b[TOTAL_CELLS];

    for (int trial = 0; trial < N_TRIALS; trial++)
    {
        /* Generate random message A */
        unsigned char msgA[MSG_LEN];
        for (int i = 0; i < MSG_LEN; i++)
            msgA[i] = (unsigned char)(rng_next() & 0xFF);

        /* Create B: flip one random bit at the specified byte position */
        unsigned char msgB[MSG_LEN];
        memcpy(msgB, msgA, MSG_LEN);
        int flip_bit = (int)(rng_next() % 8);
        msgB[flip_byte] ^= (unsigned char)(1 << flip_bit);

        /* Process byte-by-byte, snapshotting after each byte */
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        for (int b = 0; b < MSG_LEN; b++)
        {
            feed_one_byte(msgA[b]);
            snapshot_grid(snap_a);

            /* We need to process B up to byte b as well.
             * Reinit + replay from scratch for correctness. */
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            for (int k = 0; k <= b; k++)
                feed_one_byte(msgB[k]);
            snapshot_grid(snap_b);

            int d = count_diffs(snap_a, snap_b);
            sum[b] += d;
            sum_sq[b] += (double)d * d;
            if (d < minv[b])
                minv[b] = d;
            if (d > maxv[b])
                maxv[b] = d;

            /* Restore A's grid state for the next byte.
             * Reinit + replay A up to byte b. */
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            for (int k = 0; k <= b; k++)
                feed_one_byte(msgA[k]);
        }
    }

    /* Output CSV */
    printf("byte_index,mean_diff_cells,min,max,stddev\n");
    for (int b = 0; b < MSG_LEN; b++)
    {
        double mean = sum[b] / N_TRIALS;
        double var = (sum_sq[b] / N_TRIALS) - (mean * mean);
        double stddev = (var > 0.0) ? sqrt(var) : 0.0;
        printf("%d,%.2f,%d,%d,%.2f\n", b + 1, mean, minv[b], maxv[b], stddev);
    }

    return 0;
}
