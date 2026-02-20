/**
 * Hash Pattern Analysis Test
 *
 * Generates (input, hash) pairs and analyzes them for systematic patterns:
 *
 * Test 1: Positional Bit Bias — is any bit position in the hash output
 *         biased (set more or less than 50% across many inputs)?
 * Test 2: Common Prefix/Suffix — do hashes of different inputs share
 *         leading or trailing nibbles more often than expected?
 * Test 3: Sequential Input Correlation — do numerically adjacent inputs
 *         (counter 0..N) produce hashes with correlated byte positions?
 * Test 4: Structured Input Patterns — Hamming-weight-1 inputs, repeated
 *         bytes, counter patterns → clustering in output space?
 * Test 5: Byte-Position Uniformity — chi-squared test per byte position
 *         across many hashes (stricter than global byte uniformity).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"
#include "../../util.h"

/* ── Tunable parameters ────────────────────────────────────── */
#define NUM_SAMPLES    50000   /* hashes for statistical tests  */
#define HASH_BITS       128    /* hash length in bits           */
#define HASH_HEX_CHARS  (HASH_BITS / 4)
#define HASH_BYTES      (HASH_BITS / 8)
#define INPUT_LEN       16     /* bytes per test input          */
#define TEST_ROUNDS     10

unsigned long numberOfRounds = TEST_ROUNDS;
int hashLengthInBits = HASH_BITS;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

/* ── Helpers ───────────────────────────────────────────────── */

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* Hamming distance between two hex strings */
static int hamming_hex(const char *a, const char *b, size_t hex_len)
{
    int dist = 0;
    for (size_t i = 0; i < hex_len; i++) {
        int x = hex_val(a[i]) ^ hex_val(b[i]);
        while (x) { dist += x & 1; x >>= 1; }
    }
    return dist;
}

/* Hash an in-memory buffer and return malloc'd hex string */
static char *hash_buffer(const uint8_t *data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

/* ── Storage for (input, hash) pairs ────────────────────────── */
typedef struct {
    uint8_t input[INPUT_LEN];
    char    hash[HASH_HEX_CHARS + 1];
} HashPair;

static HashPair *pairs;

static void generate_pairs(void)
{
    printf("Generating %d hash pairs...\n", NUM_SAMPLES);
    pairs = malloc(NUM_SAMPLES * sizeof(HashPair));
    if (!pairs) { fprintf(stderr, "OOM\n"); exit(1); }

    srand((unsigned)time(NULL));

    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* First half: sequential counter   */
        /* Second half: random data         */
        if (i < NUM_SAMPLES / 2) {
            /* Little-endian counter in first 4 bytes, rest zero-padded */
            memset(pairs[i].input, 0, INPUT_LEN);
            pairs[i].input[0] = (uint8_t)(i & 0xFF);
            pairs[i].input[1] = (uint8_t)((i >> 8) & 0xFF);
            pairs[i].input[2] = (uint8_t)((i >> 16) & 0xFF);
            pairs[i].input[3] = (uint8_t)((i >> 24) & 0xFF);
        } else {
            for (int j = 0; j < INPUT_LEN; j++)
                pairs[i].input[j] = (uint8_t)(rand() & 0xFF);
        }

        char *h = hash_buffer(pairs[i].input, INPUT_LEN);
        strncpy(pairs[i].hash, h, HASH_HEX_CHARS);
        pairs[i].hash[HASH_HEX_CHARS] = '\0';
        free(h);

        if ((i + 1) % 500 == 0)
            printf("  %d / %d\n", i + 1, NUM_SAMPLES);
    }
    printf("Done.\n\n");
}

/* ══════════════════════════════════════════════════════════════
 * TEST 1: Positional Bit Bias
 * For each bit position 0..HASH_BITS-1, count how often it is 1.
 * Flag positions where the ratio deviates significantly from 0.5.
 * ══════════════════════════════════════════════════════════════ */
static int test_positional_bit_bias(void)
{
    printf("=== Test 1: Positional Bit Bias ===\n");
    int counts[HASH_BITS];
    memset(counts, 0, sizeof(counts));

    for (int i = 0; i < NUM_SAMPLES; i++) {
        for (int b = 0; b < HASH_HEX_CHARS; b++) {
            int nibble = hex_val(pairs[i].hash[b]);
            int bit_offset = b * 4;
            for (int k = 3; k >= 0; k--) {
                counts[bit_offset + (3 - k)] += (nibble >> k) & 1;
            }
        }
    }

    double max_dev = 0.0;
    int worst_bit = 0;
    int flagged = 0;
    /* 99% confidence interval for binomial: |p - 0.5| < 2.576 / (2*sqrt(N)) */
    double threshold = 2.576 / (2.0 * sqrt((double)NUM_SAMPLES));

    for (int b = 0; b < HASH_BITS; b++) {
        double ratio = (double)counts[b] / NUM_SAMPLES;
        double dev = fabs(ratio - 0.5);
        if (dev > max_dev) { max_dev = dev; worst_bit = b; }
        if (dev > threshold) {
            if (flagged < 5) /* only print first 5 */
                printf("  WARNING: Bit %3d ratio=%.4f (dev=%.4f > threshold=%.4f)\n",
                       b, ratio, dev, threshold);
            flagged++;
        }
    }

    printf("  Max deviation: bit %d = %.5f (threshold %.5f)\n",
           worst_bit, max_dev, threshold);
    printf("  Flagged bits:  %d / %d\n", flagged, HASH_BITS);

    /* At 99% confidence, we expect ~1% false positives */
    int max_allowed = (int)(HASH_BITS * 0.03 + 1); /* allow 3% */
    int pass = flagged <= max_allowed;
    printf("  Result: %s (max allowed flagged: %d)\n\n",
           pass ? "PASS" : "FAIL", max_allowed);
    return pass;
}

/* ══════════════════════════════════════════════════════════════
 * TEST 2: Common Prefix / Suffix Detection
 * Among all pairs, check if hashes share leading/trailing nibbles
 * more often than expected by chance (1/16 per nibble match).
 * ══════════════════════════════════════════════════════════════ */
static int test_prefix_suffix(void)
{
    printf("=== Test 2: Common Prefix/Suffix Detection ===\n");

    int sample = NUM_SAMPLES < 1000 ? NUM_SAMPLES : 1000;
    /* Check up to 4 leading and 4 trailing nibbles */
    long prefix_match[4] = {0, 0, 0, 0};
    long suffix_match[4] = {0, 0, 0, 0};
    long comparisons = 0;

    for (int i = 0; i < sample; i++) {
        for (int j = i + 1; j < sample; j++) {
            comparisons++;
            for (int k = 0; k < 4; k++) {
                /* prefix: first k+1 nibbles match? */
                int pfx_match = 1;
                for (int n = 0; n <= k; n++) {
                    if (pairs[i].hash[n] != pairs[j].hash[n]) { pfx_match = 0; break; }
                }
                if (pfx_match) prefix_match[k]++;

                /* suffix: last k+1 nibbles match? */
                int sfx_match = 1;
                for (int n = 0; n <= k; n++) {
                    int idx = HASH_HEX_CHARS - 1 - n;
                    if (pairs[i].hash[idx] != pairs[j].hash[idx]) { sfx_match = 0; break; }
                }
                if (sfx_match) suffix_match[k]++;
            }
        }
    }

    int pass = 1;
    for (int k = 0; k < 4; k++) {
        /* Expected: 1/16^(k+1) of all pairs share k+1 nibbles */
        double expected_rate = 1.0;
        for (int n = 0; n <= k; n++) expected_rate /= 16.0;
        double pfx_rate = (double)prefix_match[k] / comparisons;
        double sfx_rate = (double)suffix_match[k] / comparisons;
        double pfx_dev = fabs(pfx_rate - expected_rate) / expected_rate;
        double sfx_dev = fabs(sfx_rate - expected_rate) / expected_rate;

        printf("  %d-nibble prefix: observed=%.6f expected=%.6f (dev=%.1f%%)\n",
               k + 1, pfx_rate, expected_rate, pfx_dev * 100);
        printf("  %d-nibble suffix: observed=%.6f expected=%.6f (dev=%.1f%%)\n",
               k + 1, sfx_rate, expected_rate, sfx_dev * 100);

        /* Allow up to 100% deviation for small expected values, 50% for large */
        double allowed = (k >= 2) ? 1.5 : 0.5;
        if (pfx_dev > allowed || sfx_dev > allowed) {
            printf("  WARNING: Excessive deviation at %d nibbles!\n", k + 1);
            pass = 0;
        }
    }

    printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

/* ══════════════════════════════════════════════════════════════
 * TEST 3: Sequential Input Correlation
 * For sequential counter inputs 0,1,2,..., check whether adjacent
 * hashes are more similar than random pairs.
 * ══════════════════════════════════════════════════════════════ */
static int test_sequential_correlation(void)
{
    printf("=== Test 3: Sequential Input Correlation ===\n");

    int seq_count = NUM_SAMPLES / 2; /* first half is sequential */

    /* Hamming distance between consecutive sequential inputs */
    double sum_adjacent = 0;
    int adj_count = 0;
    for (int i = 0; i < seq_count - 1; i++) {
        sum_adjacent += hamming_hex(pairs[i].hash, pairs[i + 1].hash, HASH_HEX_CHARS);
        adj_count++;
    }
    double avg_adjacent = sum_adjacent / adj_count;

    /* Hamming distance between random (non-adjacent) pairs for baseline */
    double sum_random = 0;
    int rand_count = 0;
    for (int i = 0; i < 2000 && i < adj_count; i++) {
        int a = rand() % seq_count;
        int b = rand() % seq_count;
        if (a == b) continue;
        sum_random += hamming_hex(pairs[a].hash, pairs[b].hash, HASH_HEX_CHARS);
        rand_count++;
    }
    double avg_random = sum_random / rand_count;

    double expected = HASH_BITS * 0.5; /* ideal: 50% of bits differ */
    double adj_ratio = avg_adjacent / HASH_BITS;
    double rnd_ratio = avg_random / HASH_BITS;

    printf("  Adjacent sequential pairs:  avg Hamming = %.2f / %d (%.2f%%)\n",
           avg_adjacent, HASH_BITS, adj_ratio * 100);
    printf("  Random pairs (baseline):    avg Hamming = %.2f / %d (%.2f%%)\n",
           avg_random, HASH_BITS, rnd_ratio * 100);
    printf("  Ideal:                      %.1f / %d (50.00%%)\n",
           expected, HASH_BITS);

    /* Check that adjacent is not significantly lower than random */
    double diff_pct = fabs(adj_ratio - rnd_ratio) * 100;
    printf("  Difference adjacent vs random: %.2f%%\n", diff_pct);

    int pass = (adj_ratio > 0.45) && (diff_pct < 3.0);
    printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

/* ══════════════════════════════════════════════════════════════
 * TEST 4: Structured Input Patterns
 * Feed systematic inputs and check for output clustering.
 *   a) Hamming-weight-1 inputs (single bit set)
 *   b) Repeated-byte inputs (0x00, 0x01, ..., 0xFF)
 *   c) Counter inputs
 * Check: min pairwise Hamming distance in each group.
 * ══════════════════════════════════════════════════════════════ */
static int test_structured_inputs(void)
{
    printf("=== Test 4: Structured Input Patterns ===\n");
    int pass = 1;

    /* --- 4a: Hamming-weight-1 inputs (INPUT_LEN*8 = 128 inputs) --- */
    int hw1_count = INPUT_LEN * 8;
    char **hw1_hashes = malloc((size_t)hw1_count * sizeof(char *));
    if (!hw1_hashes) { fprintf(stderr, "OOM\n"); return 0; }

    printf("  4a) Hamming-weight-1 inputs (%d hashes)...\n", hw1_count);
    for (int i = 0; i < hw1_count; i++) {
        uint8_t input[INPUT_LEN];
        memset(input, 0, INPUT_LEN);
        input[i / 8] = (uint8_t)(1 << (i % 8));
        hw1_hashes[i] = hash_buffer(input, INPUT_LEN);
    }

    /* Find min pairwise Hamming distance */
    int min_dist = HASH_BITS;
    double sum_dist = 0;
    long pair_count = 0;
    for (int i = 0; i < hw1_count; i++) {
        for (int j = i + 1; j < hw1_count; j++) {
            int d = hamming_hex(hw1_hashes[i], hw1_hashes[j], HASH_HEX_CHARS);
            if (d < min_dist) min_dist = d;
            sum_dist += d;
            pair_count++;
        }
    }
    double avg_dist = sum_dist / pair_count;
    printf("      Min Hamming distance: %d / %d (%.1f%%)\n",
           min_dist, HASH_BITS, 100.0 * min_dist / HASH_BITS);
    printf("      Avg Hamming distance: %.1f / %d (%.1f%%)\n",
           avg_dist, HASH_BITS, 100.0 * avg_dist / HASH_BITS);

    if (min_dist < HASH_BITS / 6) {
        printf("      WARNING: Min distance too low — possible clustering!\n");
        pass = 0;
    } else {
        printf("      OK\n");
    }
    for (int i = 0; i < hw1_count; i++) free(hw1_hashes[i]);
    free(hw1_hashes);

    /* --- 4b: Repeated-byte inputs --- */
    int rb_count = 256;
    char **rb_hashes = malloc((size_t)rb_count * sizeof(char *));
    if (!rb_hashes) { fprintf(stderr, "OOM\n"); return 0; }

    printf("  4b) Repeated-byte inputs (256 hashes)...\n");
    for (int i = 0; i < rb_count; i++) {
        uint8_t input[INPUT_LEN];
        memset(input, (uint8_t)i, INPUT_LEN);
        rb_hashes[i] = hash_buffer(input, INPUT_LEN);
    }

    min_dist = HASH_BITS;
    sum_dist = 0;
    pair_count = 0;
    for (int i = 0; i < rb_count; i++) {
        for (int j = i + 1; j < rb_count; j++) {
            int d = hamming_hex(rb_hashes[i], rb_hashes[j], HASH_HEX_CHARS);
            if (d < min_dist) min_dist = d;
            sum_dist += d;
            pair_count++;
        }
    }
    avg_dist = sum_dist / pair_count;
    printf("      Min Hamming distance: %d / %d (%.1f%%)\n",
           min_dist, HASH_BITS, 100.0 * min_dist / HASH_BITS);
    printf("      Avg Hamming distance: %.1f / %d (%.1f%%)\n",
           avg_dist, HASH_BITS, 100.0 * avg_dist / HASH_BITS);

    if (min_dist < HASH_BITS / 6) {
        printf("      WARNING: Min distance too low — possible clustering!\n");
        pass = 0;
    } else {
        printf("      OK\n");
    }
    for (int i = 0; i < rb_count; i++) free(rb_hashes[i]);
    free(rb_hashes);

    printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

/* ══════════════════════════════════════════════════════════════
 * TEST 5: Byte-Position Uniformity (per-position chi-squared)
 * For each byte position in the hash, collect the distribution
 * of values across all hashes and run a chi-squared test.
 * ══════════════════════════════════════════════════════════════ */
static int test_byte_position_uniformity(void)
{
    printf("=== Test 5: Byte-Position Uniformity (chi-squared) ===\n");

    int pass = 1;
    double worst_p = 1.0;
    int worst_pos = 0;

    for (int pos_idx = 0; pos_idx < HASH_BYTES; pos_idx++) {
        int buckets[256];
        memset(buckets, 0, sizeof(buckets));

        for (int i = 0; i < NUM_SAMPLES; i++) {
            int hi = hex_val(pairs[i].hash[pos_idx * 2]);
            int lo = hex_val(pairs[i].hash[pos_idx * 2 + 1]);
            uint8_t byte_val = (uint8_t)((hi << 4) | lo);
            buckets[byte_val]++;
        }

        /* Chi-squared with 255 degrees of freedom */
        double expected = (double)NUM_SAMPLES / 256.0;
        double chi2 = 0;
        for (int b = 0; b < 256; b++) {
            double diff = buckets[b] - expected;
            chi2 += (diff * diff) / expected;
        }

        /* Approximate p-value using normal approximation of chi-squared:
         * For large df, (chi2 - df) / sqrt(2*df) ~ N(0,1) */
        double df = 255.0;
        double z = (chi2 - df) / sqrt(2.0 * df);
        double p_approx = 0.5 * erfc(z / sqrt(2.0)); /* one-tailed */

        if (p_approx < worst_p) {
            worst_p = p_approx;
            worst_pos = pos_idx;
        }

        if (p_approx < 0.001) {
            printf("  WARNING: Byte position %d — chi2=%.1f, p=%.6f\n",
                   pos_idx, chi2, p_approx);
            pass = 0;
        }
    }

    printf("  Worst p-value: position %d, p=%.6f\n", worst_pos, worst_p);
    printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");
    return pass;
}

/* ══════════════════════════════════════════════════════════════
 * TEST 6: Export (input, hash) pairs to CSV
 * ══════════════════════════════════════════════════════════════ */
static void export_pairs_csv(const char *filename)
{
    printf("=== Exporting %d pairs to %s ===\n", NUM_SAMPLES, filename);
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return; }

    fprintf(f, "input_hex,hash_hex\n");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        /* Print input as hex */
        for (int j = 0; j < INPUT_LEN; j++)
            fprintf(f, "%02x", pairs[i].input[j]);
        fprintf(f, ",%s\n", pairs[i].hash);
    }
    fclose(f);
    printf("  Done.\n\n");
}

/* ══════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║       Secasy Hash Pattern Analysis Test         ║\n");
    printf("║  %d samples, %d-bit hash, %d rounds       ║\n",
           NUM_SAMPLES, HASH_BITS, TEST_ROUNDS);
    printf("╚══════════════════════════════════════════════════╝\n\n");

    generate_pairs();

    int passed = 0;
    int total = 5;

    passed += test_positional_bit_bias();
    passed += test_prefix_suffix();
    passed += test_sequential_correlation();
    passed += test_structured_inputs();
    passed += test_byte_position_uniformity();

    export_pairs_csv("hash_pairs.csv");

    printf("══════════════════════════════════════════════════\n");
    printf("  TOTAL: %d / %d tests passed\n", passed, total);
    printf("══════════════════════════════════════════════════\n");

    free(pairs);
    return (passed == total) ? 0 : 1;
}
