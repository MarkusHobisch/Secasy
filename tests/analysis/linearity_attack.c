/*
 * linearity_attack.c
 *
 * Targeted empirical attacks on Secasy aimed at the structural concerns
 * raised in README Section 2.3 / 4.5:
 *
 *   (1) GF(2)-linearity probe (4-tuple sum):
 *       For random triples (a, b, c) of equal-length inputs compute
 *           Δ = H(a) ⊕ H(b) ⊕ H(c) ⊕ H(a ⊕ b ⊕ c)
 *       For a fully GF(2)-linear function, Δ ≡ 0.
 *       For an ideal random oracle, every bit of Δ is Bernoulli(0.5).
 *       Measured per output bit: empirical p(bit=1), |p − 0.5| × N gives
 *       a z-score; the maximum |z| across 512 bits is the headline number.
 *
 *   (2) Differential distribution (single-byte input difference):
 *       For a fixed input difference δ (one byte XOR with 0xFF at a fixed
 *       position) hash pairs (m, m ⊕ δ) and accumulate the per-bit XOR of
 *       the outputs. An ideal hash gives mean Hamming weight = 256 (for
 *       512-bit output) with σ = √(512·0.25) ≈ 11.3. We report mean,
 *       min, max, and the per-bit flip rate distribution.
 *
 *   (3) Cross-length truncated collision search:
 *       Hash N random inputs of length uniformly drawn from {4,…,64}
 *       bytes, truncate to 32 bits, and count collisions. Birthday
 *       expectation E = N(N−1)/(2·2^32). Cross-length collisions are
 *       reported separately because Secasy lacks length padding.
 *
 * Build (added to CMakeLists.txt):
 *     add_secasy_test(SecasyLinearityAttack tests/analysis/linearity_attack.c)
 *
 * Run:
 *     ./SecasyLinearityAttack            # all three attacks, default N
 *     ./SecasyLinearityAttack --quick    # smaller N for smoke test
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
int hashLengthInBits = DEFAULT_BIT_SIZE; /* 512 bits = 64 bytes = 8 × uint64_t */

#define HASH_WORDS 8u    /* 8 × 64 bit = 512 bit */
#define HASH_BYTES 64u
#define OUT_BITS   512u

/* ------------------------ Hash helpers --------------------------------- */

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* Compute the full 512-bit Secasy hash into out[0..7] (big-endian word order). */
static void hash_full(const unsigned char *data, size_t len, uint64_t out[HASH_WORDS])
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    char *h = calculateHashValue();
    /* hex string of 128 chars expected */
    for (unsigned w = 0; w < HASH_WORDS; w++)
    {
        uint64_t v = 0;
        for (int i = 0; i < 16; i++)
        {
            v = (v << 4) | (uint64_t)hex_digit(h[(size_t)w * 16u + (size_t)i]);
        }
        out[w] = v;
    }
    free(h);
}

/* Count number of set bits in a uint64_t. */
static unsigned popcount64(uint64_t x)
{
    unsigned c = 0;
    while (x) { c += (unsigned)(x & 1u); x >>= 1; }
    return c;
}

/* ------------------------ RNG (xorshift64*) ---------------------------- */

static uint64_t rng_state;

static uint64_t rng_u64(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void rng_fill(unsigned char *buf, size_t n)
{
    size_t i = 0;
    while (i + 8 <= n)
    {
        uint64_t v = rng_u64();
        memcpy(buf + i, &v, 8);
        i += 8;
    }
    if (i < n)
    {
        uint64_t v = rng_u64();
        memcpy(buf + i, &v, n - i);
    }
}

/* ===================== Attack 1: GF(2) 4-tuple linearity =============== */
/*
 * If H were perfectly GF(2)-linear we would have Δ = 0 always.
 * We measure, per output bit position k ∈ [0, 511], the fraction
 *      p_k = #{trials : bit k of Δ is 1} / N
 * Under the null (ideal random oracle) E[p_k] = 0.5, σ = √(0.25/N).
 * The headline number is max |p_k − 0.5| over all 512 bits, reported as
 * both an absolute deviation and a z-score.
 *
 * We use input length L = 16 bytes (long enough for non-trivial Phase 2
 * walks, short enough to keep the test fast).
 */
static void attack1_linearity(uint64_t N)
{
    const size_t L = 16;
    unsigned char *a = malloc(L);
    unsigned char *b = malloc(L);
    unsigned char *c = malloc(L);
    unsigned char *d = malloc(L);
    if (!a || !b || !c || !d) { fprintf(stderr, "OOM\n"); exit(1); }

    /* per-bit count of '1' in Δ */
    uint64_t bit_count[OUT_BITS];
    for (unsigned i = 0; i < OUT_BITS; i++) bit_count[i] = 0;

    uint64_t exact_zero_deltas = 0;     /* trials where Δ == 0 (would be 100% if linear) */
    uint64_t hw_sum = 0;                /* sum of Hamming weight of Δ */
    unsigned hw_min = OUT_BITS + 1;
    unsigned hw_max = 0;

    clock_t t0 = clock();
    for (uint64_t t = 0; t < N; t++)
    {
        rng_fill(a, L);
        rng_fill(b, L);
        rng_fill(c, L);
        for (size_t i = 0; i < L; i++) d[i] = a[i] ^ b[i] ^ c[i];

        uint64_t ha[HASH_WORDS], hb[HASH_WORDS], hc[HASH_WORDS], hd[HASH_WORDS];
        hash_full(a, L, ha);
        hash_full(b, L, hb);
        hash_full(c, L, hc);
        hash_full(d, L, hd);

        unsigned hw = 0;
        int all_zero = 1;
        for (unsigned w = 0; w < HASH_WORDS; w++)
        {
            uint64_t delta = ha[w] ^ hb[w] ^ hc[w] ^ hd[w];
            if (delta != 0) all_zero = 0;
            unsigned pc = popcount64(delta);
            hw += pc;
            /* per-bit accumulation, bit 0 = LSB of word 0 */
            uint64_t v = delta;
            for (unsigned bit = 0; bit < 64; bit++)
            {
                if (v & 1ULL) bit_count[w * 64 + bit]++;
                v >>= 1;
            }
        }
        hw_sum += hw;
        if (hw < hw_min) hw_min = hw;
        if (hw > hw_max) hw_max = hw;
        if (all_zero) exact_zero_deltas++;

        if (((t + 1) % 5000) == 0)
        {
            double pct = 100.0 * (double)(t + 1) / (double)N;
            fprintf(stderr, "\r  attack 1: %6.2f%% (%" PRIu64 "/%" PRIu64 ")", pct, t + 1, N);
            fflush(stderr);
        }
    }
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "\n");

    /* Statistics */
    double mean_hw = (double)hw_sum / (double)N;
    double ideal_hw = (double)OUT_BITS / 2.0;        /* 256 */
    double ideal_sigma = 0.5 * sqrt((double)OUT_BITS); /* √(N·p·q) where N=512,p=q=0.5 ≈ 11.31 */

    double max_abs_dev = 0.0;
    unsigned max_bit = 0;
    double max_p = 0.0;
    for (unsigned i = 0; i < OUT_BITS; i++)
    {
        double p = (double)bit_count[i] / (double)N;
        double dev = (p > 0.5) ? (p - 0.5) : (0.5 - p);
        if (dev > max_abs_dev)
        {
            max_abs_dev = dev;
            max_bit = i;
            max_p = p;
        }
    }
    double sigma_per_bit = 0.5 / sqrt((double)N);
    double max_z = max_abs_dev / sigma_per_bit;

    printf("\n=== Attack 1: GF(2) 4-tuple linearity test ===\n");
    printf("  trials N         = %" PRIu64 "\n", N);
    printf("  input length L   = %zu bytes\n", L);
    printf("  elapsed          = %.2f s\n", elapsed);
    printf("  exact Δ == 0     = %" PRIu64 " / %" PRIu64 "    (linear hash: N, ideal hash: ~0)\n",
           exact_zero_deltas, N);
    printf("  mean HW(Δ)       = %.4f    (ideal: %.1f, σ ≈ %.2f)\n",
           mean_hw, ideal_hw, ideal_sigma);
    printf("  min / max HW(Δ)  = %u / %u\n", hw_min, hw_max);
    printf("  worst per-bit p  = %.6f at bit %u   (ideal: 0.5)\n", max_p, max_bit);
    printf("  worst |p−0.5|    = %.6f\n", max_abs_dev);
    printf("  worst z-score    = %.2f σ    (≳ 4σ ⇒ structurally biased; ~3σ tolerable for 512 bits)\n", max_z);

    free(a); free(b); free(c); free(d);
}

/* ===================== Attack 2: Differential distribution ============= */
/*
 * Fixed single-byte XOR difference δ at fixed byte position. For each
 * trial, draw m, compute Δ = H(m) ⊕ H(m ⊕ δ), accumulate per-bit flip
 * counts.
 *
 * For an ideal hash: each output bit should flip with probability 0.5
 * regardless of δ (strict avalanche). We test three δ positions
 * (first byte, middle, last byte) and report the worst bias across all
 * three deltas and all 512 output bits.
 */
static void attack2_differential(uint64_t N)
{
    const size_t L = 32; /* 32-byte input; covers 8 input bytes per axis dimension */
    unsigned char *m  = malloc(L);
    unsigned char *mp = malloc(L);
    if (!m || !mp) { fprintf(stderr, "OOM\n"); exit(1); }

    size_t delta_positions[3] = { 0, L / 2, L - 1 };
    const char *delta_names[3] = { "first byte", "middle byte", "last byte" };

    printf("\n=== Attack 2: Single-byte differential distribution ===\n");
    printf("  trials N per δ   = %" PRIu64 "\n", N);
    printf("  input length L   = %zu bytes,  δ = 0xFF at one byte position\n", L);

    double worst_z_overall = 0.0;
    const char *worst_pos = "";
    unsigned worst_bit = 0;

    for (int dp = 0; dp < 3; dp++)
    {
        size_t pos = delta_positions[dp];

        uint64_t bit_count[OUT_BITS];
        for (unsigned i = 0; i < OUT_BITS; i++) bit_count[i] = 0;

        uint64_t hw_sum = 0;
        unsigned hw_min = OUT_BITS + 1;
        unsigned hw_max = 0;

        clock_t t0 = clock();
        for (uint64_t t = 0; t < N; t++)
        {
            rng_fill(m, L);
            memcpy(mp, m, L);
            mp[pos] ^= 0xFF;

            uint64_t ha[HASH_WORDS], hb[HASH_WORDS];
            hash_full(m,  L, ha);
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
            if (hw < hw_min) hw_min = hw;
            if (hw > hw_max) hw_max = hw;

            if (((t + 1) % 10000) == 0)
            {
                double pct = 100.0 * (double)(t + 1) / (double)N;
                fprintf(stderr, "\r  attack 2 (%s): %6.2f%%", delta_names[dp], pct);
                fflush(stderr);
            }
        }
        double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
        fprintf(stderr, "\n");

        double mean_hw = (double)hw_sum / (double)N;
        double max_abs_dev = 0.0;
        unsigned max_bit = 0;
        double max_p = 0.0;
        for (unsigned i = 0; i < OUT_BITS; i++)
        {
            double p = (double)bit_count[i] / (double)N;
            double dev = (p > 0.5) ? (p - 0.5) : (0.5 - p);
            if (dev > max_abs_dev) { max_abs_dev = dev; max_bit = i; max_p = p; }
        }
        double sigma_per_bit = 0.5 / sqrt((double)N);
        double max_z = max_abs_dev / sigma_per_bit;

        printf("  --- δ at %s (byte %zu) ---\n", delta_names[dp], pos);
        printf("    elapsed         = %.2f s\n", elapsed);
        printf("    mean HW(Δ)      = %.4f   (ideal 256.00, σ ≈ 11.31)\n", mean_hw);
        printf("    min / max HW(Δ) = %u / %u\n", hw_min, hw_max);
        printf("    worst per-bit p = %.6f at bit %u\n", max_p, max_bit);
        printf("    worst |p−0.5|   = %.6f\n", max_abs_dev);
        printf("    worst z-score   = %.2f σ\n", max_z);

        if (max_z > worst_z_overall)
        {
            worst_z_overall = max_z;
            worst_pos = delta_names[dp];
            worst_bit = max_bit;
        }
    }

    printf("  overall worst    = %.2f σ at output bit %u  (δ = %s)\n",
           worst_z_overall, worst_bit, worst_pos);

    free(m); free(mp);
}

/* ===================== Attack 3: Cross-length truncated collisions ===== */
/*
 * Hash N random inputs of length uniformly in {4, …, 64} bytes, retain
 * the first 32 bits of the output, count truncated collisions, and
 * separately count cross-length collisions (different input lengths
 * hashing to same 32-bit prefix).
 */
typedef struct CEntry
{
    uint32_t key;
    uint8_t  len;
    struct CEntry *next;
} CEntry;

#define CTBL_BITS 24u
#define CTBL_SIZE (1u << CTBL_BITS)
#define CTBL_MASK (CTBL_SIZE - 1u)

static void attack3_crosslen(uint64_t N)
{
    CEntry **tbl = calloc(CTBL_SIZE, sizeof(CEntry *));
    CEntry *pool = malloc((size_t)N * sizeof(CEntry));
    if (!tbl || !pool) { fprintf(stderr, "OOM\n"); exit(1); }
    size_t used = 0;

    unsigned char buf[64];
    uint64_t coll_total = 0;
    uint64_t coll_cross = 0;

    clock_t t0 = clock();
    for (uint64_t t = 0; t < N; t++)
    {
        unsigned r = (unsigned)(rng_u64() & 63u);
        uint8_t len = (uint8_t)(4u + (r % 61u));   /* L ∈ [4, 64] */
        rng_fill(buf, len);

        uint64_t h[HASH_WORDS];
        hash_full(buf, len, h);
        uint32_t key = (uint32_t)(h[0] >> 32);     /* top 32 bits */

        uint32_t bucket = key & CTBL_MASK;
        for (CEntry *e = tbl[bucket]; e; e = e->next)
        {
            if (e->key == key)
            {
                coll_total++;
                if (e->len != len) coll_cross++;
            }
        }
        CEntry *ne = &pool[used++];
        ne->key = key;
        ne->len = len;
        ne->next = tbl[bucket];
        tbl[bucket] = ne;

        if (((t + 1) % 100000) == 0)
        {
            double pct = 100.0 * (double)(t + 1) / (double)N;
            fprintf(stderr, "\r  attack 3: %6.2f%%", pct);
            fflush(stderr);
        }
    }
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "\n");

    double expected = (double)N * (double)(N - 1) / (2.0 * 4294967296.0);
    double sigma = sqrt(expected);
    double dev_sigma = ((double)coll_total - expected) / sigma;

    printf("\n=== Attack 3: 32-bit truncated cross-length collisions ===\n");
    printf("  trials N         = %" PRIu64 "\n", N);
    printf("  length range     = [4, 64] bytes (uniform)\n");
    printf("  elapsed          = %.2f s\n", elapsed);
    printf("  total collisions = %" PRIu64 "   (ideal %.1f, σ ≈ %.1f, dev %.2f σ)\n",
           coll_total, expected, sigma, dev_sigma);
    printf("  cross-length     = %" PRIu64 "   (would be elevated by Phase-2 trajectory coincidences)\n",
           coll_cross);
    /* For uniform length sampling, the fraction of same-length pairs is 1/61 ≈ 1.64%,
       so under the null hypothesis cross-length should be ≈ 98.36% of total. */
    double cross_frac = (coll_total > 0) ? (double)coll_cross / (double)coll_total : 0.0;
    printf("  cross-length frac= %.4f   (null hypothesis: ≈ 0.9836 if no length bias)\n", cross_frac);

    free(tbl); free(pool);
}

/* ===================== Main ============================================ */

int main(int argc, char **argv)
{
    int quick = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--quick") == 0) quick = 1;

    rng_state = (uint64_t)time(NULL) ^ 0xDEADBEEFCAFEBABEULL;

    uint64_t N1 = quick ? 2000u    : 20000u;     /* attack 1: 4 hashes per trial */
    uint64_t N2 = quick ? 5000u    : 50000u;     /* attack 2: 2 hashes per trial */
    uint64_t N3 = quick ? 200000u  : 2000000u;   /* attack 3: 1 hash per trial */

    printf("Secasy targeted attack suite\n");
    printf("  rounds=%lu  hash=%d bit  quick=%s\n",
           numberOfRounds, hashLengthInBits, quick ? "yes" : "no");
    printf("  RNG seed = 0x%016" PRIx64 "\n", rng_state);

    attack1_linearity(N1);
    attack2_differential(N2);
    attack3_crosslen(N3);

    printf("\nDone.\n");
    return 0;
}
