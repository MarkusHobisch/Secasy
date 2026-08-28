/*
 * structural_attack.c
 *
 * White-box structural attack on Secasy. Unlike the black-box probes in
 * linearity_attack.c (which only observe the 512-bit output), this harness
 * instruments the *internal* 16x16 grid state phase by phase and targets the
 * concrete structural seams identified by source analysis:
 *
 *   Seam 1 (Phase 2 sparsity):
 *       An L-byte input performs 4L cursor steps; for short inputs only a
 *       small fraction of the 256 tiles is ever touched. Untouched tiles keep
 *       {value = 2, colorIndex = ADD, primeIndex = 0}. The mixing schedule of
 *       Phase 3 is read from the colorIndex layout, so a sparsely-touched grid
 *       yields a Phase-3 schedule dominated by ADD.
 *
 *   Seam 2 (Phase 3 near-linearity):
 *       ADD and SUB are linear over Z_{2^64}; only XOR, ROTATE_LEFT_XOR,
 *       ROTATE_RIGHT_ADD and INVERT are nonlinear. If most cells carry an ADD
 *       colorIndex, Phase 3 is close to an affine map and may diffuse poorly.
 *
 *   Seam 3 (internal vs. output avalanche):
 *       Black-box tests report ~50% output avalanche. This harness measures
 *       avalanche *after Phase 2* and *after Phase 3* separately, to check
 *       whether good output avalanche hides a narrow internal pipe.
 *
 * Measurements:
 *   M1  Nonlinearity census   - per-input count of touched / nonlinear cells.
 *   M2  Phase-2 avalanche      - internal diffusion of a single input-bit flip.
 *   M3  Phase-3 avalanche      - amplification across the mixing rounds.
 *   M4  Min-avalanche search   - internally-guided differential: the weakest
 *                                single-bit flip (lowest output avalanche).
 *
 * The harness reimplements Phase 3 + Phase 4 locally so it can inspect state
 * between phases; a self-test validates the reimplementation bit-for-bit
 * against the production calculateHashValue() before any attack runs.
 *
 * Build (CMakeLists.txt):
 *     add_secasy_test(SecasyStructuralAttack tests/analysis/structural_attack.c)
 *
 * Run:
 *     ./SecasyStructuralAttack            # full study
 *     ./SecasyStructuralAttack --quick    # smaller N for a smoke test
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
#include "Calculations.h"

/* The production translation units expect these as externs (normally main.c). */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

extern Position pos;
extern Tile field[FIELD_SIZE][FIELD_SIZE];

#define GRID_CELLS        (FIELD_SIZE * FIELD_SIZE) /* 256 */
#define HASH_WORDS        8u                        /* 8 x 64 bit = 512 bit  */
#define STATE_BITS        (GRID_CELLS * 64)         /* 16384 internal bits   */
#define OUT_BITS          512u
#define UNTOUCHED_VALUE   2u                         /* FIRST_PRIME           */
#define ROTATE_LEFT_AMT   13u
#define ROTATE_RIGHT_AMT  7u
#define EXTRACT_ROTATE    7u
#define ROUND_CONSTANT    0x9E3779B97F4A7C15ULL

/* ------------------------- Grid state snapshot ------------------------- */

typedef struct
{
    uint64_t      value[FIELD_SIZE][FIELD_SIZE];
    ColorIndex  color[FIELD_SIZE][FIELD_SIZE];
    uint32_t      primeIndex[FIELD_SIZE][FIELD_SIZE];
    uint32_t      posX;
    uint32_t      posY;
} GridState_t;

/* ------------------------- RNG (xorshift64*) --------------------------- */

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

static unsigned popcount64(uint64_t x)
{
    unsigned c = 0;
    while (x) { c += (unsigned)(x & 1u); x >>= 1; }
    return c;
}

static int color_is_nonlinear(ColorIndex c)
{
    /* ADD and SUB are linear over Z_{2^64}; the rest are not. */
    return (c != ADD && c != SUB);
}

static int cell_is_touched(const GridState_t *st, uint32_t x, uint32_t y)
{
    return st->value[x][y]      != UNTOUCHED_VALUE
        || st->color[x][y]      != ADD
        || st->primeIndex[x][y] != 0u;
}

/* ------------------- Phase 2: run the production walk ------------------ */

static void compute_phase2_state(const unsigned char *msg, size_t len, GridState_t *st)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(msg, len);

    for (uint32_t x = 0; x < FIELD_SIZE; x++)
    {
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
        {
            st->value[x][y]      = field[x][y].value;
            st->color[x][y]      = field[x][y].colorIndex;
            st->primeIndex[x][y] = field[x][y].primeIndex;
        }
    }
    st->posX = pos.x;
    st->posY = pos.y;
}

/* --------- Phase 3: faithful local reimplementation (per cell) --------- */
/*
 * Operates in place on a value grid V using the fixed colorIndex layout C.
 * The iteration order (i outer, j inner) reproduces the production
 * read-after-write neighbour semantics exactly.
 */
static void faithful_process_cell(ColorIndex c, uint32_t i, uint32_t j,
                                  uint64_t V[FIELD_SIZE][FIELD_SIZE])
{
    uint64_t *cell = &V[i][j];
    switch (c)
    {
    case ADD:
        *cell += (j == 0) ? 1u : V[i][j - 1];
        break;
    case SUB:
        *cell -= (j == (FIELD_SIZE - 1)) ? 1u : V[i][j + 1];
        break;
    case XOR:
        *cell ^= (i == 0) ? 1u : V[i - 1][j];
        break;
    case ROTATE_LEFT_XOR:
        *cell = ROTATE_LEFT_64(*cell, ROTATE_LEFT_AMT)
              ^ ((i == (FIELD_SIZE - 1)) ? 1u : V[i + 1][j]);
        break;
    case ROTATE_RIGHT_ADD:
        *cell = ROTATE_RIGHT_64(*cell, ROTATE_RIGHT_AMT)
              + ((i == 0) ? 1u : V[i - 1][j]);
        break;
    case INVERT:
        *cell = ~*cell;
        break;
    default:
        break;
    }
}

static void run_phase3(const GridState_t *st, unsigned long rounds,
                       uint64_t Vout[FIELD_SIZE][FIELD_SIZE])
{
    uint64_t V[FIELD_SIZE][FIELD_SIZE];
    memcpy(V, st->value, sizeof V);

    uint32_t pX = st->posX;
    uint32_t pY = st->posY;

    for (unsigned long r = 0; r < rounds; r++)
    {
        const uint64_t round_key = ROUND_CONSTANT * (uint64_t)(r + 1u);
        for (uint32_t i = 0; i < FIELD_SIZE; i++)
        {
            for (uint32_t j = 0; j < FIELD_SIZE; j++)
            {
                uint32_t ox = (pX + i) & FIELD_SIZE_MASK;
                uint32_t oy = (pY + j) & FIELD_SIZE_MASK;
                faithful_process_cell(st->color[ox][oy], i, j, V);
                V[i][j] += round_key + (uint64_t)(i * FIELD_SIZE + j);
            }
        }
        if (++pX == FIELD_SIZE)
        {
            pX = 0;
            if (++pY == FIELD_SIZE) pY = 0;
        }
    }
    memcpy(Vout, V, sizeof V);
}

/* ------------------- Phase 4: faithful local extraction ---------------- */

static void run_phase4(const uint64_t V[FIELD_SIZE][FIELD_SIZE], uint64_t out[HASH_WORDS])
{
    for (unsigned b = 0; b < HASH_WORDS; b++)
    {
        uint64_t acc = 0;
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
        {
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
            {
                uint64_t pos = (uint64_t)(x * FIELD_SIZE + y + 1)
                             + (uint64_t)b * GRID_CELLS;
                uint64_t weight = 2u * pos + 1u; /* odd => unit, no dead bits */
                acc += V[x][y] * weight; /* ADD (not XOR): carry-couples cells */
                acc = (acc << EXTRACT_ROTATE) | (acc >> (64 - EXTRACT_ROTATE));
            }
        }
        out[b] = acc;
    }
}

/* Full local hash = Phase 2 snapshot + local Phase 3 + local Phase 4. */
static void local_hash(const unsigned char *msg, size_t len, uint64_t out[HASH_WORDS])
{
    GridState_t st;
    uint64_t V[FIELD_SIZE][FIELD_SIZE];
    compute_phase2_state(msg, len, &st);
    run_phase3(&st, numberOfRounds, V);
    run_phase4(V, out);
}

/* ----------------------------- Self-test ------------------------------- */
/*
 * Validate the local Phase 3/4 reimplementation against the production
 * calculateHashValue() for random inputs. Any mismatch aborts the study.
 */
static int self_test(unsigned trials)
{
    unsigned char msg[64];
    for (unsigned t = 0; t < trials; t++)
    {
        size_t len = 1 + (size_t)(rng_u64() % 63u);
        rng_fill(msg, len);

        uint64_t local[HASH_WORDS];
        local_hash(msg, len, local);

        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, len);
        char *hex = calculateHashValue();

        char local_hex[HASH_WORDS * 16 + 1];
        for (unsigned w = 0; w < HASH_WORDS; w++)
            snprintf(local_hex + w * 16, 17, "%016" PRIx64, local[w]);

        if (strncmp(local_hex, hex, HASH_WORDS * 16) != 0)
        {
            printf("  SELF-TEST FAILED at trial %u (len=%zu)\n", t, len);
            printf("    production: %.*s\n", (int)(HASH_WORDS * 16), hex);
            printf("    local:      %s\n", local_hex);
            free(hex);
            return 0;
        }
        free(hex);
    }
    return 1;
}

/* =================== M1: nonlinearity / touch census ================== */

static void measure_census(size_t L, uint64_t N)
{
    unsigned char *msg = malloc(L);
    if (!msg) { fprintf(stderr, "OOM\n"); exit(1); }

    double sum_touched = 0.0, sum_nonlin = 0.0;
    unsigned min_touched = GRID_CELLS + 1, max_touched = 0;
    unsigned min_nonlin = GRID_CELLS + 1, max_nonlin = 0;

    for (uint64_t t = 0; t < N; t++)
    {
        rng_fill(msg, L);
        GridState_t st;
        compute_phase2_state(msg, L, &st);

        unsigned touched = 0, nonlin = 0;
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
            {
                if (cell_is_touched(&st, x, y)) touched++;
                if (color_is_nonlinear(st.color[x][y])) nonlin++;
            }
        sum_touched += touched;
        sum_nonlin  += nonlin;
        if (touched < min_touched) min_touched = touched;
        if (touched > max_touched) max_touched = touched;
        if (nonlin  < min_nonlin)  min_nonlin  = nonlin;
        if (nonlin  > max_nonlin)  max_nonlin  = nonlin;
    }
    free(msg);

    printf("  L=%2zu  touched cells: mean %6.2f  [%3u..%3u]   "
           "nonlinear-color cells: mean %6.2f  [%3u..%3u]  (of %d)\n",
           L, sum_touched / (double)N, min_touched, max_touched,
           sum_nonlin / (double)N, min_nonlin, max_nonlin, GRID_CELLS);
}

/* ============ M2/M3: internal avalanche, phase by phase =============== */

static unsigned state_bit_distance(const uint64_t A[FIELD_SIZE][FIELD_SIZE],
                                   const uint64_t B[FIELD_SIZE][FIELD_SIZE],
                                   unsigned *cells_changed)
{
    unsigned bits = 0, cells = 0;
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
        {
            uint64_t d = A[x][y] ^ B[x][y];
            if (d) cells++;
            bits += popcount64(d);
        }
    *cells_changed = cells;
    return bits;
}

static void measure_internal_avalanche(size_t L, uint64_t N)
{
    unsigned char *msg  = malloc(L);
    unsigned char *msg2 = malloc(L);
    if (!msg || !msg2) { fprintf(stderr, "OOM\n"); exit(1); }

    double p2_cells = 0.0, p2_bits = 0.0;
    double p3_cells = 0.0, p3_bits = 0.0;
    double out_bits = 0.0;
    unsigned p2_cells_min = GRID_CELLS + 1;
    double out_min = 1.0, out_max = 0.0;

    for (uint64_t t = 0; t < N; t++)
    {
        rng_fill(msg, L);
        memcpy(msg2, msg, L);
        unsigned bit = (unsigned)(rng_u64() % (L * 8));
        msg2[bit / 8] ^= (unsigned char)(1u << (bit % 8));

        GridState_t a2, b2;
        uint64_t a3[FIELD_SIZE][FIELD_SIZE], b3[FIELD_SIZE][FIELD_SIZE];
        uint64_t ao[HASH_WORDS], bo[HASH_WORDS];

        compute_phase2_state(msg,  L, &a2);
        compute_phase2_state(msg2, L, &b2);

        unsigned cc;
        unsigned bb = state_bit_distance(a2.value, b2.value, &cc);
        p2_cells += cc; p2_bits += bb;
        if (cc < p2_cells_min) p2_cells_min = cc;

        run_phase3(&a2, numberOfRounds, a3);
        run_phase3(&b2, numberOfRounds, b3);
        bb = state_bit_distance(a3, b3, &cc);
        p3_cells += cc; p3_bits += bb;

        run_phase4(a3, ao);
        run_phase4(b3, bo);
        unsigned ob = 0;
        for (unsigned w = 0; w < HASH_WORDS; w++) ob += popcount64(ao[w] ^ bo[w]);
        out_bits += ob;
        double of = (double)ob / (double)OUT_BITS;
        if (of < out_min) out_min = of;
        if (of > out_max) out_max = of;
    }
    free(msg); free(msg2);

    printf("  L=%2zu  (N=%" PRIu64 ", single-bit flip)\n", L, N);
    printf("    after Phase 2:  cells changed %6.2f / %d   state bits %7.2f / %d   (min cells %u)\n",
           p2_cells / (double)N, GRID_CELLS, p2_bits / (double)N, STATE_BITS, p2_cells_min);
    printf("    after Phase 3:  cells changed %6.2f / %d   state bits %7.2f / %d\n",
           p3_cells / (double)N, GRID_CELLS, p3_bits / (double)N, STATE_BITS);
    printf("    output (512b):  flipped bits %7.2f  -> avalanche %.4f%%  [min %.2f%%  max %.2f%%]\n",
           out_bits / (double)N, 100.0 * out_bits / (double)N / OUT_BITS,
           100.0 * out_min, 100.0 * out_max);
}

/* ============ M4: internally-guided minimum-avalanche search ========== */
/*
 * For each base message, flip every single input bit and record the lowest
 * output avalanche observed. A structurally weak input/bit would yield an
 * avalanche far below 50%. We report the global minimum across all bases.
 */
static void measure_min_avalanche(size_t L, uint64_t bases)
{
    unsigned char *msg  = malloc(L);
    unsigned char *msg2 = malloc(L);
    if (!msg || !msg2) { fprintf(stderr, "OOM\n"); exit(1); }

    unsigned global_min = OUT_BITS + 1;
    size_t   worst_len = L;
    unsigned worst_bit = 0;
    double   sum_per_base_min = 0.0;
    uint64_t total_pairs = 0;

    clock_t t0 = clock();
    for (uint64_t s = 0; s < bases; s++)
    {
        rng_fill(msg, L);
        uint64_t base_out[HASH_WORDS];
        local_hash(msg, L, base_out);

        unsigned base_min = OUT_BITS + 1;
        for (unsigned bit = 0; bit < L * 8; bit++)
        {
            memcpy(msg2, msg, L);
            msg2[bit / 8] ^= (unsigned char)(1u << (bit % 8));
            uint64_t out[HASH_WORDS];
            local_hash(msg2, L, out);

            unsigned hd = 0;
            for (unsigned w = 0; w < HASH_WORDS; w++)
                hd += popcount64(base_out[w] ^ out[w]);
            total_pairs++;

            if (hd < base_min) base_min = hd;
            if (hd < global_min)
            {
                global_min = hd;
                worst_bit  = bit;
                worst_len  = L;
            }
        }
        sum_per_base_min += base_min;

        if (((s + 1) % 100) == 0)
        {
            fprintf(stderr, "\r  M4: %5" PRIu64 "/%" PRIu64 " bases", s + 1, bases);
            fflush(stderr);
        }
    }
    double elapsed = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "\n");
    free(msg); free(msg2);

    printf("  L=%2zu  bases=%" PRIu64 "  pairs=%" PRIu64 "  elapsed %.1fs\n",
           L, bases, total_pairs, elapsed);
    printf("    global minimum avalanche : %u / 512 bits = %.4f%%  (len=%zu, bit=%u)\n",
           global_min, 100.0 * global_min / OUT_BITS, worst_len, worst_bit);
    printf("    mean per-base minimum    : %.2f / 512 bits = %.4f%%\n",
           sum_per_base_min / (double)bases,
           100.0 * sum_per_base_min / (double)bases / OUT_BITS);
    printf("    (ideal min-of-%zu ~ a few sigma below 256; collapse toward 0 would signal a weak differential)\n",
           L * 8);
}

/* ====== M5: does low nonlinearity correlate with weak diffusion? ====== */
/*
 * The central hypothesis: inputs whose Phase-2 layout has very few nonlinear
 * cells make Phase 3 near-affine over Z_{2^64} and should diffuse worse.
 * We bin single-bit-flip output avalanche by the base input's nonlinear-cell
 * count. A downward trend toward low nonlinearity would confirm the seam.
 */
#define NL_BINS 12u  /* nonlinear-cell-count buckets: 0..1, 2..3, ... */

static void measure_nonlinearity_correlation(size_t L, uint64_t N)
{
    unsigned char *msg  = malloc(L);
    unsigned char *msg2 = malloc(L);
    if (!msg || !msg2) { fprintf(stderr, "OOM\n"); exit(1); }

    double   bin_aval[NL_BINS] = {0};
    uint64_t bin_cnt[NL_BINS]  = {0};
    unsigned global_min_nl = GRID_CELLS + 1;
    double   min_nl_aval = 0.0;

    for (uint64_t t = 0; t < N; t++)
    {
        rng_fill(msg, L);
        GridState_t st;
        compute_phase2_state(msg, L, &st);

        unsigned nonlin = 0;
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
                if (color_is_nonlinear(st.color[x][y])) nonlin++;

        /* one random single-bit flip, measure output avalanche */
        memcpy(msg2, msg, L);
        unsigned bit = (unsigned)(rng_u64() % (L * 8));
        msg2[bit / 8] ^= (unsigned char)(1u << (bit % 8));

        uint64_t ao[HASH_WORDS], bo[HASH_WORDS];
        local_hash(msg,  L, ao);
        local_hash(msg2, L, bo);
        unsigned hd = 0;
        for (unsigned w = 0; w < HASH_WORDS; w++) hd += popcount64(ao[w] ^ bo[w]);
        double aval = (double)hd / (double)OUT_BITS;

        unsigned bin = nonlin / 2u;
        if (bin >= NL_BINS) bin = NL_BINS - 1;
        bin_aval[bin] += aval;
        bin_cnt[bin]  += 1;

        if (nonlin < global_min_nl)
        {
            global_min_nl = nonlin;
            min_nl_aval = aval;
        }
    }
    free(msg); free(msg2);

    printf("  L=%2zu  (N=%" PRIu64 ")  avalanche binned by nonlinear-cell count:\n", L, N);
    for (unsigned b = 0; b < NL_BINS; b++)
    {
        if (bin_cnt[b] == 0) continue;
        printf("    nonlinear %2u-%2u : N=%6" PRIu64 "   mean avalanche %.4f%%\n",
               b * 2u, b * 2u + 1u, bin_cnt[b],
               100.0 * bin_aval[b] / (double)bin_cnt[b]);
    }
    printf("    rarest input had %u nonlinear cells; its flip avalanche = %.4f%%\n",
           global_min_nl, 100.0 * min_nl_aval);
}

/* ====== M6: algebraic attack on the fully-affine Phase-3 instances ===== */
/*
 * For inputs whose Phase-2 layout uses ONLY the ADD/SUB colorIndex (no XOR,
 * rotation or INVERT), Phase 3 is a Z_{2^64}-affine map  W = M*V + b  with an
 * integer 256x256 matrix M. Such a map is a bijection on the 16384-bit state
 * iff det(M) is odd, equivalently iff (M mod 2) has full GF(2) rank.
 *
 * If M is SINGULAR there exist nonzero differences dV with M*dV = 0: these are
 * guaranteed internal collisions of the affine sub-cipher, independent of the
 * Phase-4 extractor. This is the sharpest possible attack on the structurally
 * weakest input class.
 *
 * Because reduction Z_{2^64} -> Z_2 is a ring homomorphism and ADD/SUB are
 * LSB-closed (carry only propagates upward), the matrix (M mod 2) is recovered
 * exactly by probing Phase 3 with unit value-vectors and reading output LSBs.
 */

/* Find a reachable Phase-2 layout that uses only ADD/SUB colors. */
static int find_affine_layout(size_t L, GridState_t *out, unsigned max_tries)
{
    unsigned char *msg = malloc(L);
    if (!msg) { fprintf(stderr, "OOM\n"); exit(1); }
    for (unsigned t = 0; t < max_tries; t++)
    {
        rng_fill(msg, L);
        GridState_t st;
        compute_phase2_state(msg, L, &st);
        int affine = 1;
        for (uint32_t x = 0; x < FIELD_SIZE && affine; x++)
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
            {
                ColorIndex c = st.color[x][y];
                if (c != ADD && c != SUB) { affine = 0; break; }
            }
        if (affine) { *out = st; free(msg); return 1; }
    }
    free(msg);
    return 0;
}

/* GF(2) rank of 256 column vectors, each a 256-bit vector (uint64_t[4]). */
static int gf2_rank256(uint64_t cols[256][4])
{
    int rank = 0;
    for (int bit = 0; bit < 256 && rank < 256; bit++)
    {
        int pivot = -1;
        for (int c = rank; c < 256; c++)
            if ((cols[c][bit / 64] >> (bit % 64)) & 1ULL) { pivot = c; break; }
        if (pivot < 0) continue;
        for (int w = 0; w < 4; w++)
        {
            uint64_t tmp = cols[rank][w];
            cols[rank][w] = cols[pivot][w];
            cols[pivot][w] = tmp;
        }
        for (int c = 0; c < 256; c++)
        {
            if (c == rank) continue;
            if ((cols[c][bit / 64] >> (bit % 64)) & 1ULL)
                for (int w = 0; w < 4; w++) cols[c][w] ^= cols[rank][w];
        }
        rank++;
    }
    return rank;
}

static void measure_affine_invertibility(size_t L)
{
    GridState_t st;
    if (!find_affine_layout(L, &st, 5000000u))
    {
        printf("  L=%zu: no fully-affine (ADD/SUB-only) layout found.\n", L);
        return;
    }

    /* Affine offset b = Phase3(0). */
    GridState_t probe = st;
    uint64_t base[FIELD_SIZE][FIELD_SIZE];
    memset(probe.value, 0, sizeof probe.value);
    run_phase3(&probe, numberOfRounds, base);

    /* Column k of (M mod 2) = LSB( Phase3(e_k) XOR Phase3(0) ). */
    static uint64_t cols[256][4];
    memset(cols, 0, sizeof cols);
    for (int k = 0; k < 256; k++)
    {
        memset(probe.value, 0, sizeof probe.value);
        probe.value[k / FIELD_SIZE][k % FIELD_SIZE] = 1u;
        uint64_t outk[FIELD_SIZE][FIELD_SIZE];
        run_phase3(&probe, numberOfRounds, outk);

        for (int idx = 0; idx < 256; idx++)
        {
            int x = idx / FIELD_SIZE, y = idx % FIELD_SIZE;
            if ((outk[x][y] ^ base[x][y]) & 1ULL)
                cols[k][idx / 64] |= (1ULL << (idx % 64));
        }
    }

    int rank = gf2_rank256(cols);
    printf("  L=%zu  fully-affine (ADD/SUB-only) layout found and probed.\n", L);
    printf("    GF(2) rank of Phase-3 map (mod 2) = %d / 256\n", rank);
    if (rank == 256)
        printf("    => det(M) is odd: Phase 3 is a BIJECTION for this fixed schedule.\n"
               "       It has no internal value collision within that schedule; this says\n"
               "       nothing about different schedules or message-level collisions.\n");
    else
        printf("    => SINGULAR! kernel dimension = %d. The affine sub-cipher has\n"
               "       guaranteed internal collisions dV with M*dV = 0 (structural break).\n",
               256 - rank);
}

/* ====== M7: dead input bits of the Phase-4 extractor (attack) ========= */
/*
 * Phase 4 (the production hashValue()) reads only field[x][y].value, so we can
 * exercise it directly as a 16384 -> 512 bit map by writing the grid values
 * and reading the 8 output blocks.
 *
 * Unfolding the accumulator (rotation after every XOR, and 7*256 = 0 mod 64):
 *     block_b = XOR_k ROL(V_k * w_{k,b}, r_k),
 *     w_{k,b} = 2 * ((k+1) + 256*b) + 1.
 * Flipping bit i of V_k changes the product by +-2^i * w_{k,b} mod 2^64; if
 * that is zero the product is unchanged. Hence the top v2(w_{k,b}) bits of V_k
 * would be dead. Production weights are always odd, so this predicts zero dead
 * input bits for the entire 512-bit output.
 */
static void set_field_values(const uint64_t V[FIELD_SIZE][FIELD_SIZE])
{
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
            field[x][y].value = V[x][y];
}

static void extractor_output(const uint64_t V[FIELD_SIZE][FIELD_SIZE], uint64_t out[HASH_WORDS])
{
    set_field_values(V);
    for (unsigned b = 0; b < HASH_WORDS; b++)
        out[b] = hashValue(b);   /* the real production extractor */
}

static unsigned val2_u64(uint64_t w)
{
    unsigned v = 0;
    if (w == 0) return 64u;
    while (((w >> v) & 1ULL) == 0ULL) v++;
    return v;
}

static void measure_phase4_deadbits(void)
{
    /* Predicted dead bits per cell = min over blocks of v2(weight). */
    unsigned predicted_per_cell[GRID_CELLS];
    unsigned predicted_total = 0;
    for (int k = 0; k < GRID_CELLS; k++)
    {
        unsigned minv = 64;
        for (int b = 0; b < (int)HASH_WORDS; b++)
        {
            uint64_t pos = (uint64_t)(k + 1) + (uint64_t)b * GRID_CELLS;
            uint64_t w = 2u * pos + 1u; /* production weight: always odd */
            unsigned v = val2_u64(w);
            if (v < minv) minv = v;
        }
        predicted_per_cell[k] = minv;
        predicted_total += minv;
    }

    /* Empirical: flip every one of the 16384 input bits over several random
       base states and count bits that NEVER change the 512-bit output. */
    unsigned dead_total = 0;
    unsigned mismatch_position = 0;   /* dead bits that are NOT the predicted top bits */
    const int trials = 3;

    uint64_t V[FIELD_SIZE][FIELD_SIZE];
    for (int t = 0; t < trials; t++)
    {
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
                V[x][y] = rng_u64();

        uint64_t base[HASH_WORDS];
        extractor_output(V, base);

        unsigned dead_this_trial = 0;
        for (int k = 0; k < GRID_CELLS; k++)
        {
            int x = k / FIELD_SIZE, y = k % FIELD_SIZE;
            for (int i = 0; i < 64; i++)
            {
                uint64_t saved = V[x][y];
                V[x][y] ^= (1ULL << i);
                uint64_t out[HASH_WORDS];
                extractor_output(V, out);
                V[x][y] = saved;

                int changed = 0;
                for (unsigned b = 0; b < HASH_WORDS; b++)
                    if (out[b] != base[b]) { changed = 1; break; }

                if (!changed)
                {
                    dead_this_trial++;
                    /* predicted dead bits are the top predicted_per_cell[k] bits */
                    int is_predicted = (i >= 64 - (int)predicted_per_cell[k]);
                    if (!is_predicted) mismatch_position++;
                }
            }
        }
        if (t == 0) dead_total = dead_this_trial;
        else if (dead_this_trial != dead_total) mismatch_position++; /* state-dependence => flag */
        /* restore production field for safety */
        extractor_output(V, base);
    }

    printf("  predicted dead input bits (min_b v2(weight)) = %u / %d\n",
           predicted_total, STATE_BITS);
    printf("  empirical dead input bits (per trial)     = %u / %d   [%d trials, state-independent: %s]\n",
           dead_total, STATE_BITS, trials, (mismatch_position == 0) ? "yes" : "NO");
    printf("  predicted and empirical dead-bit sets match: %s\n",
           (mismatch_position == 0) ? "confirmed" : "VIOLATED");
    printf("  => the Phase-4 extractor has %u individually dead state bits (%.2f%%).\n",
           predicted_total, 100.0 * predicted_total / STATE_BITS);

    if (predicted_total == 0)
    {
        printf("     No extractor collision follows from a single dead-bit direction.\n");
        return;
    }

    /* If a future weight definition introduces dead bits, verify them together. */
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
            V[x][y] = rng_u64();
    uint64_t outA[HASH_WORDS];
    extractor_output(V, outA);

    uint64_t V2[FIELD_SIZE][FIELD_SIZE];
    memcpy(V2, V, sizeof V2);
    for (int k = 0; k < GRID_CELLS; k++)
    {
        int x = k / FIELD_SIZE, y = k % FIELD_SIZE;
        unsigned a = predicted_per_cell[k];
        for (unsigned bit = 0; bit < a; bit++)
            V2[x][y] ^= (1ULL << (63 - bit));   /* flip the top a bits */
    }
    uint64_t outB[HASH_WORDS];
    extractor_output(V2, outB);

    int collide = (memcmp(outA, outB, sizeof outA) == 0);
    int differ  = (memcmp(V, V2, sizeof V) != 0);
    printf("  dead-bit check (flip all %u): states %s, hashes %s\n",
           predicted_total,
           differ ? "DIFFER" : "identical",
           collide ? "identical" : "differ");

    /* restore a clean production field */
    extractor_output(V, outA);
}

/* ------------------------------- main ---------------------------------- */

int main(int argc, char **argv)
{
    int quick = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--quick") == 0) quick = 1;

    rng_state = 0x9E3779B97F4A7C15ULL ^ (uint64_t)time(NULL);

    printf("============================================================\n");
    printf(" Secasy white-box structural attack\n");
    printf(" rounds=%lu  output=%d bits  state=%d bits (%d cells)\n",
           numberOfRounds, hashLengthInBits, STATE_BITS, GRID_CELLS);
    printf("============================================================\n\n");

    printf("[0] Self-test: local Phase 3/4 vs production calculateHashValue()\n");
    if (!self_test(quick ? 200 : 2000))
    {
        printf("  Reimplementation does not match production. Aborting.\n");
        return 1;
    }
    printf("  PASSED — local hash matches production bit-for-bit.\n\n");

    uint64_t n_census = quick ? 2000  : 20000;
    uint64_t n_aval   = quick ? 2000  : 20000;
    uint64_t n_bases  = quick ? 200   : 3000;

    printf("[M1] Nonlinearity / touch census (Phase-2 grid)\n");
    measure_census(16, n_census);
    measure_census(32, n_census);
    measure_census(64, n_census);
    measure_census(128, n_census);
    printf("\n");

    printf("[M2/M3] Internal avalanche, phase by phase\n");
    measure_internal_avalanche(16, n_aval);
    measure_internal_avalanche(32, n_aval);
    measure_internal_avalanche(64, n_aval);
    printf("\n");

    printf("[M4] Internally-guided minimum-avalanche differential search\n");
    measure_min_avalanche(16, n_bases);
    measure_min_avalanche(32, n_bases);
    printf("\n");

    printf("[M5] Avalanche vs. Phase-3 nonlinearity (the central hypothesis)\n");
    measure_nonlinearity_correlation(16, quick ? 5000 : 50000);
    measure_nonlinearity_correlation(32, quick ? 5000 : 50000);
    printf("\n");

    printf("[M6] Algebraic invertibility of the fully-affine Phase-3 instances\n");
    measure_affine_invertibility(16);
    printf("\n");

    printf("[M7] Dead input bits of the Phase-4 extractor\n");
    measure_phase4_deadbits();
    printf("\n");

    printf("Done.\n");
    return 0;
}
