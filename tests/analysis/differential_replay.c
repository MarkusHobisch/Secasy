/*
 * differential_replay.c
 *
 * Targeted replay of the suspicious finding from linearity_attack.c
 * (Attack 2, middle-byte δ, worst per-bit z = 4.98σ at bit 321 with
 * N = 50,000). We rerun the same test with:
 *
 *   - N_LARGE = 200,000 trials per δ position
 *   - five independent RNG seeds
 *   - three δ positions (first / middle / last byte of a 32-byte input)
 *
 * If bit 321 (or any bit) consistently shows z > 4σ across seeds, the
 * differential bias is real. If z fluctuates randomly around < 3σ, the
 * earlier reading was a multiple-comparison artefact (testing 512 bits
 * × 3 deltas simultaneously).
 *
 * For each (seed, position) we report:
 *   - mean Hamming weight of Δ
 *   - the per-bit p, z, and bit index of the worst absolute deviation
 *   - the per-bit p and z for bit 321 specifically (so we can track that
 *     exact location across all seeds)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

#define HASH_WORDS 8u
#define OUT_BITS   512u

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void hash_full(const unsigned char *data, size_t len, uint64_t out[HASH_WORDS])
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    char *h = calculateHashValue();
    for (unsigned w = 0; w < HASH_WORDS; w++)
    {
        uint64_t v = 0;
        for (int i = 0; i < 16; i++)
            v = (v << 4) | (uint64_t)hex_digit(h[(size_t)w * 16u + (size_t)i]);
        out[w] = v;
    }
    free(h);
}

static unsigned popcount64(uint64_t x)
{
    unsigned c = 0;
    while (x) { c += (unsigned)(x & 1u); x >>= 1; }
    return c;
}

static uint64_t rng_state;
static uint64_t rng_u64(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}
static void rng_fill(unsigned char *buf, size_t n)
{
    size_t i = 0;
    while (i + 8 <= n) { uint64_t v = rng_u64(); memcpy(buf + i, &v, 8); i += 8; }
    if (i < n) { uint64_t v = rng_u64(); memcpy(buf + i, &v, n - i); }
}

static void run_one(uint64_t N, size_t L, size_t pos, uint64_t seed,
                    const char *label, unsigned watch_bit)
{
    rng_state = seed;
    unsigned char *m  = malloc(L);
    unsigned char *mp = malloc(L);
    if (!m || !mp) { fprintf(stderr, "OOM\n"); exit(1); }

    uint64_t bit_count[OUT_BITS];
    for (unsigned i = 0; i < OUT_BITS; i++) bit_count[i] = 0;
    uint64_t hw_sum = 0;

    clock_t t0 = clock();
    for (uint64_t t = 0; t < N; t++)
    {
        rng_fill(m, L);
        memcpy(mp, m, L);
        mp[pos] ^= 0xFF;
        uint64_t ha[HASH_WORDS], hb[HASH_WORDS];
        hash_full(m, L, ha);
        hash_full(mp, L, hb);
        unsigned hw = 0;
        for (unsigned w = 0; w < HASH_WORDS; w++)
        {
            uint64_t delta = ha[w] ^ hb[w];
            hw += popcount64(delta);
            uint64_t v = delta;
            for (unsigned bit = 0; bit < 64; bit++)
            {
                if (v & 1ULL) bit_count[w * 64 + bit]++;
                v >>= 1;
            }
        }
        hw_sum += hw;
        if (((t + 1) % 20000) == 0)
        {
            fprintf(stderr, "\r  %s: %5.1f%%", label, 100.0 * (double)(t + 1) / (double)N);
            fflush(stderr);
        }
    }
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "\n");

    double mean_hw = (double)hw_sum / (double)N;
    double max_abs = 0.0;
    unsigned max_bit = 0;
    double max_p = 0.0;
    for (unsigned i = 0; i < OUT_BITS; i++)
    {
        double p = (double)bit_count[i] / (double)N;
        double dev = (p > 0.5) ? (p - 0.5) : (0.5 - p);
        if (dev > max_abs) { max_abs = dev; max_bit = i; max_p = p; }
    }
    double sigma_per_bit = 0.5 / sqrt((double)N);
    double max_z = max_abs / sigma_per_bit;

    double watch_p = (double)bit_count[watch_bit] / (double)N;
    double watch_dev = (watch_p > 0.5) ? (watch_p - 0.5) : (0.5 - watch_p);
    double watch_z = watch_dev / sigma_per_bit;

    printf("  %-30s  mean HW = %7.3f  worst: bit %3u p=%.5f z=%5.2f  bit%u: p=%.5f z=%5.2f  (%.1fs)\n",
           label, mean_hw, max_bit, max_p, max_z,
           watch_bit, watch_p, watch_z, elapsed);

    free(m); free(mp);
}

int main(int argc, char **argv)
{
    uint64_t N = 200000;
    unsigned watch_bit = 321;   /* the suspect bit from the original run */
    if (argc > 1) N = strtoull(argv[1], NULL, 10);
    if (argc > 2) watch_bit = (unsigned)strtoul(argv[2], NULL, 10);

    const size_t L = 32;
    size_t positions[3] = { 0, L / 2, L - 1 };
    const char *pnames[3] = { "first", "middle", "last" };

    /* Fixed independent seeds, chosen to be unrelated to the original run. */
    uint64_t seeds[5] = {
        0x0123456789ABCDEFULL,
        0xFEDCBA9876543210ULL,
        0xA5A5A5A5A5A5A5A5ULL,
        0xC0FFEE12345678FFULL,
        0xB16B00B5CAFEF00DULL
    };

    printf("Replay of suspect finding (linearity_attack.c attack 2)\n");
    printf("  rounds=%lu  hash=%d bit  N=%" PRIu64 "  watch_bit=%u\n",
           numberOfRounds, hashLengthInBits, N, watch_bit);
    printf("  multiple-comparison threshold for max-over-512-bits at N=%" PRIu64 ":  ~3.5 sigma\n", N);
    printf("  Bonferroni-equivalent threshold for max-over-15-runs-times-512-bits:  ~3.9 sigma\n");
    printf("\n  Header: <run-label>  mean HW  worst:bit p z   watch:p z\n\n");

    char label[64];
    for (int s = 0; s < 5; s++)
    {
        for (int p = 0; p < 3; p++)
        {
            snprintf(label, sizeof label, "seed#%d %s", s, pnames[p]);
            run_one(N, L, positions[p], seeds[s], label, watch_bit);
        }
        printf("  ----\n");
    }

    printf("\nDone.\n");
    return 0;
}
