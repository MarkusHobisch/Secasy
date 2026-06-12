/*
 * field_size_sweep.c
 * ==================
 * Secasy — Field-Size Diffusion & Symmetry Sweep
 *
 * PURPOSE:
 *   Measures avalanche diffusion quality and output symmetry for five grid
 *   sizes:  4×4, 8×8, 16×16 (baseline), 32×32, 64×64.
 *
 *   The default design uses a 16×16 = 256-cell grid.  This test answers:
 *     • Does diffusion improve/degrade with more or fewer cells?
 *     • Is there a field size where output symmetry breaks down?
 *     • Is 16×16 the empirically optimal choice?
 *
 * METRICS per field size:
 *   1. Hamming-distance histogram (20 bins, each 5 % wide, 0–100 %)
 *   2. Mean µ and standard deviation σ of Hamming distance [%]
 *      → σ is the primary quality indicator: smaller = more uniform diffusion
 *   3. Min / Max observed Hamming distance [%]
 *   4. Nibble symmetry bias: maximum deviation of any 4-bit output-nibble
 *      flip-rate from the ideal 50 %.
 *      → Low bias = all hash output positions are equally sensitive to
 *        input changes.  High bias = some output regions are "dark spots".
 *
 * METHOD:
 *   Self-contained re-implementation of the Secasy algorithm with a
 *   runtime field-size parameter g_fs (max 64).  All four phases are
 *   faithfully reproduced: initialisation, Phase-2 cursor walk (prime
 *   numbers, commutativity breaking), Phase-3 mixing rounds, Phase-4
 *   hash extraction.
 *
 *   For each field size fs:
 *     For N_MESSAGES random 32-byte messages:
 *       Compute H0 = hash(original, fs).
 *       For each of MSG_BYTES×8 = 256 input bits:
 *         Flip bit → Compute H1 → record Hamming(H0,H1) %
 *                                + per-nibble bit-flip counts.
 *     → histogram, µ/σ, nibble bias
 *
 * OUTPUT:
 *   - ASCII report to stdout (histograms + summary table)
 *   - CSV: field_size_results.csv in the current working directory
 *     (intended to be read by scripts/python/plot_field_size_sweep.py)
 *
 * BUILD TARGET:  SecasyFieldSizeSweep  (see CMakeLists.txt)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

#include "Defines.h"
#include "primes.h"

/* ── Globals required by ProcessingPhase.c (linked via CORE_SOURCES) ── */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

/* ─────────────────────────────────────────────────────────────────────
 *  Constants
 * ──────────────────────────────────────────────────────────────────── */
#define MAX_FS 64         /* maximum supported field size (power of 2)   */
#define HASH_BITS_512 512 /* always produce a 512-bit hash               */
#define HASH_HEX_LEN 128  /* HASH_BITS_512 / 4 hex chars                 */
#define N_HASH_BLOCKS 8   /* HASH_BITS_512 / 64 blocks                   */
#define N_MESSAGES 400    /* random messages per field size               */
#define MSG_BYTES 32      /* input message length in bytes                */
#define N_BINS 20         /* histogram bins  (each 5 % wide, 0–100 %)    */
#define N_NIBBLES 128     /* = HASH_HEX_LEN                               */
#define BAR_MAX_W 44      /* max ASCII bar width in characters            */

static const int FIELD_SIZES[] = {4, 8, 16, 32, 64};
#define N_FS ((int)(sizeof(FIELD_SIZES) / sizeof(FIELD_SIZES[0])))

/* ─────────────────────────────────────────────────────────────────────
 *  Per-field-size result struct
 * ──────────────────────────────────────────────────────────────────── */
typedef struct
{
    int fs;
    int cells;
    double mean;
    double std;
    double min_pct;
    double max_pct;
    double nib_bias; /* max per-nibble deviation from 50 % [pp] */
    long long bins[N_BINS];
    long long total;
} FsResult;

/* ─────────────────────────────────────────────────────────────────────
 *  Global Secasy state — prefixed g_ to avoid link conflicts with
 *  the core sources that are also compiled into this target.
 * ──────────────────────────────────────────────────────────────────── */
static int g_fs = 16;
static Tile_t g_field[MAX_FS][MAX_FS];
static uint32_t g_px = 0, g_py = 0;

/* nibble flip accumulators reset before each field-size test */
static long long g_nib_flips[N_NIBBLES];

/* ─────────────────────────────────────────────────────────────────────
 *  xorshift64 RNG
 * ──────────────────────────────────────────────────────────────────── */
static uint64_t rng_s = 0;

static uint64_t rng_next(void)
{
    rng_s ^= rng_s << 13;
    rng_s ^= rng_s >> 7;
    rng_s ^= rng_s << 17;
    return rng_s;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Nibble popcount lookup
 * ──────────────────────────────────────────────────────────────────── */
static const int lut4[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static int hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: init field
 * ──────────────────────────────────────────────────────────────────── */
static void sweep_init(int fs)
{
    g_fs = fs;
    g_px = 0;
    g_py = 0;
    for (int i = 0; i < fs; i++)
    {
        for (int j = 0; j < fs; j++)
        {
            g_field[i][j].posX = (uint32_t)i;
            g_field[i][j].posY = (uint32_t)j;
            g_field[i][j].value = 2; /* FIRST_PRIME */
            g_field[i][j].primeIndex = 0;
            g_field[i][j].colorIndex = ADD;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: advance one tile's metadata and return its next prime
 *  (mirrors updateColorAndPrimeIndexOfTile + nextPrimeNumber)
 * ──────────────────────────────────────────────────────────────────── */
static int sweep_next_prime(Tile_t *t, int direction)
{
    int pi = (int)t->primeIndex + 1 + direction;
    pi = pi % NUMBER_OF_PRIMES;
    ColorIndex_t ci = (ColorIndex_t)(((unsigned int)t->colorIndex + 1U) % 6U);
    t->primeIndex = (uint32_t)pi;
    t->colorIndex = ci;
    return storedPrimesArray[pi];
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: one cursor step (mirrors writeNextNumber)
 * ──────────────────────────────────────────────────────────────────── */
static void sweep_step(int move)
{
    uint32_t mask = (uint32_t)(g_fs - 1);
    Tile_t *t = &g_field[g_px][g_py];
    uint32_t old = (uint32_t)t->value; /* lower 32 bits sufficient for mask */
    t->value = (uint64_t)sweep_next_prime(t, move);

    switch (move)
    {
    case 0: /* UP */
        g_py = (g_py - old) & mask;
        break;
    case 1: /* RIGHT */
        g_px = (g_px + old + 1u) & mask;
        break;
    case 2: /* LEFT */
        g_px = (g_px - old) & mask;
        break;
    case 3: /* DOWN */
        g_py = (g_py + old + 1u) & mask;
        break;
    default:
        break;
    }
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: process one input byte → 4 cursor steps
 *  (mirrors calcAndSetDirections)
 * ──────────────────────────────────────────────────────────────────── */
static void sweep_byte(int byte)
{
    sweep_step(byte & 3);
    sweep_step((byte >> 2) & 3);
    sweep_step((byte >> 4) & 3);
    sweep_step((byte >> 6) & 3);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: Phase 2 — process input buffer
 *  (mirrors processBuffer + setPrimeNumberOfLastTile)
 * ──────────────────────────────────────────────────────────────────── */
static void sweep_phase2(const unsigned char *data, size_t len)
{
    if (!data || len == 0)
        return; /* mirrors processBuffer: empty → no-op */

    for (size_t i = 0; i < len; i++)
        sweep_byte(data[i] & 0xFF);

    /* finalise last touched tile (mirrors setPrimeNumberOfLastTile) */
    Tile_t *t = &g_field[g_px][g_py];
    t->value = (uint64_t)sweep_next_prime(t, 0);
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: Phase 3 — update one cell
 *  (mirrors processData; px/py here are loop indices, NOT cursor coords)
 * ──────────────────────────────────────────────────────────────────── */
static void sweep_cell(ColorIndex_t ci, uint32_t px, uint32_t py)
{
    int fs = g_fs;
    Tile_t *t = &g_field[px][py];

    switch (ci)
    {
    case ADD:
        t->value += (py == 0u) ? 1ULL : g_field[px][py - 1u].value;
        break;
    case SUB:
        t->value -= (py == (uint32_t)(fs - 1)) ? 1ULL : g_field[px][py + 1u].value;
        break;
    case XOR:
        t->value ^= (px == 0u) ? 1ULL : g_field[px - 1u][py].value;
        break;
    case ROTATE_LEFT_XOR:
        t->value = ROTATE_LEFT_64(t->value, 13) ^ ((px == (uint32_t)(fs - 1)) ? 1ULL : g_field[px + 1u][py].value);
        break;
    case ROTATE_RIGHT_ADD:
        t->value = ROTATE_RIGHT_64(t->value, 7) + ((px == 0u) ? 1ULL : g_field[px - 1u][py].value);
        break;
    case INVERT:
        t->value = ~t->value;
        break;
    }
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: Phase 4 — extract one 64-bit hash block
 *  (mirrors hashValue from Calculations.c, using g_fs)
 * ──────────────────────────────────────────────────────────────────── */
static uint64_t sweep_block(unsigned long b)
{
    int fs = g_fs;
    uint64_t acc = 0;
    for (int x = 0; x < fs; x++)
    {
        for (int y = 0; y < fs; y++)
        {
            uint64_t cell_pos = (uint64_t)(unsigned)(x * fs + y + 1);
            uint64_t block_off = (uint64_t)b * (uint64_t)(unsigned)(fs * fs);
            uint64_t w = cell_pos + block_off;
            acc += g_field[x][y].value * w;
            acc = (acc << 7) | (acc >> 57);
        }
    }
    return acc;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Sweep core: full Phase-3 + Phase-4 → returns malloc'd hex string
 *  (mirrors calculateHashValue from ProcessingPhase.c)
 * ──────────────────────────────────────────────────────────────────── */
static char *sweep_hash(void)
{
    int fs = g_fs;
    uint32_t px = g_px;
    uint32_t py = g_py;

    /* ensure at least N_HASH_BLOCKS rounds so all blocks are covered */
    unsigned long eff_rounds = DEFAULT_NUMBER_OF_ROUNDS;
    if (eff_rounds < (unsigned long)N_HASH_BLOCKS)
        eff_rounds = (unsigned long)N_HASH_BLOCKS;

    /* Phase 3: mixing rounds */
    for (unsigned long r = 0; r < eff_rounds; r++)
    {
        for (int i = 0; i < fs; i++)
        {
            for (int j = 0; j < fs; j++)
            {
                uint32_t ox = (px + (uint32_t)i) & (uint32_t)(fs - 1);
                uint32_t oy = (py + (uint32_t)j) & (uint32_t)(fs - 1);
                ColorIndex_t ci = g_field[ox][oy].colorIndex;
                sweep_cell(ci, (uint32_t)i, (uint32_t)j);
            }
        }
        /* advance position (mirrors setPositionsToZeroIfOutOfRange) */
        if (++px == (uint32_t)fs)
        {
            px = 0;
            if (++py == (uint32_t)fs)
                py = 0;
        }
    }

    /* Phase 4: extraction */
    char *out = (char *)malloc(HASH_HEX_LEN + 1);
    if (!out)
    {
        fputs("OOM\n", stderr);
        exit(EXIT_FAILURE);
    }
    for (int b = 0; b < N_HASH_BLOCKS; b++)
        snprintf(out + b * 16, 17, "%016" PRIx64, sweep_block((unsigned long)b));
    out[HASH_HEX_LEN] = '\0';
    return out;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Hamming distance + per-nibble flip accumulation
 * ──────────────────────────────────────────────────────────────────── */
static int hamming_and_nibbles(const char *a, const char *b)
{
    int dist = 0;
    for (int i = 0; i < HASH_HEX_LEN; i++)
    {
        int xv = hex_val(a[i]) ^ hex_val(b[i]);
        dist += lut4[xv];
        g_nib_flips[i] += (long long)lut4[xv];
    }
    return dist;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Run measurement for one field size
 * ──────────────────────────────────────────────────────────────────── */
static void run_fs(int fs, FsResult *r)
{
    r->fs = fs;
    r->cells = fs * fs;
    r->total = 0LL;
    r->min_pct = 999.0;
    r->max_pct = -999.0;
    memset(r->bins, 0, sizeof(r->bins));
    memset(g_nib_flips, 0, sizeof(g_nib_flips));

    double sum = 0.0, sum2 = 0.0;
    long long n_trials = 0;
    unsigned char msg[MSG_BYTES];

    for (int m = 0; m < N_MESSAGES; m++)
    {
        /* random message */
        for (int b = 0; b < MSG_BYTES; b++)
            msg[b] = (unsigned char)(rng_next() & 0xFFu);

        /* original hash */
        sweep_init(fs);
        sweep_phase2(msg, (size_t)MSG_BYTES);
        char *h0 = sweep_hash();

        /* per-input-bit flips */
        for (int bit = 0; bit < MSG_BYTES * 8; bit++)
        {
            int bi = bit / 8;
            int bj = bit % 8;
            msg[bi] ^= (unsigned char)(1u << bj);

            sweep_init(fs);
            sweep_phase2(msg, (size_t)MSG_BYTES);
            char *h1 = sweep_hash();

            msg[bi] ^= (unsigned char)(1u << bj); /* restore */

            int d = hamming_and_nibbles(h0, h1);
            double pct = (double)d / (double)HASH_BITS_512 * 100.0;

            /* ── Collision alert: log full details when HD = 0 ── */
            if (d == 0)
            {
                printf("\n  *** COLLISION FOUND (fs=%d) ***\n", fs);
                printf("  Message #%d, bit %d (byte %d, bit %d)\n",
                       m, bit, bi, bj);
                printf("  Message: ");
                for (int k = 0; k < MSG_BYTES; k++)
                    printf("%02x", msg[k] ^ ((k == bi) ? (1u << bj) : 0u));
                printf("\n");
                printf("  Flipped: ");
                for (int k = 0; k < MSG_BYTES; k++)
                    printf("%02x", msg[k]);
                printf("\n");
                printf("  H(orig): %s\n", h0);
                printf("  H(flip): %s\n", h1);
            }

            int bin = (int)(pct / 5.0);
            if (bin >= N_BINS)
                bin = N_BINS - 1;
            if (bin < 0)
                bin = 0;

            r->bins[bin]++;
            r->total++;
            n_trials++;
            sum += pct;
            sum2 += pct * pct;
            if (pct < r->min_pct)
                r->min_pct = pct;
            if (pct > r->max_pct)
                r->max_pct = pct;

            free(h1);
        }
        free(h0);
    }

    long long n = r->total;
    r->mean = sum / (double)n;
    double var = sum2 / (double)n - r->mean * r->mean;
    r->std = sqrt(var > 0.0 ? var : 0.0);

    /*
     * Nibble symmetry metric:
     * Each of the 128 output nibbles should flip with probability 0.5
     * regardless of which input bit was flipped.
     * nib_flips[i] = total bit flips in nibble position i across all trials.
     * Ideal: n_trials × 4 bits × 0.5 = 2 × n_trials bits.
     * Max deviation (in percentage points) quantifies output asymmetry.
     */
    double max_dev = 0.0;
    double bits_per_nib = (double)n_trials * 4.0;
    for (int ni = 0; ni < N_NIBBLES; ni++)
    {
        double rate = (bits_per_nib > 0.0) ? (double)g_nib_flips[ni] / bits_per_nib : 0.5;
        double dev = fabs(rate - 0.5) * 100.0; /* percentage points */
        if (dev > max_dev)
            max_dev = dev;
    }
    r->nib_bias = max_dev;
}

/* ─────────────────────────────────────────────────────────────────────
 *  ASCII histogram output
 * ──────────────────────────────────────────────────────────────────── */
static void print_histogram(const FsResult *r)
{
    long long peak = 1;
    for (int i = 0; i < N_BINS; i++)
        if (r->bins[i] > peak)
            peak = r->bins[i];

    const int ideal_bin = (int)(50.0 * N_BINS / 100.0); /* bin covering 50–55% */

    char title[64];
    snprintf(title, sizeof(title), "%dx%d grid  (%d cells)", r->fs, r->fs, r->cells);

    printf("\n  +--[ %-44s ]--+\n", title);
    printf("  |  %%-range  |  count  | distribution                              ideal |\n");
    printf("  +-----------+---------+--------------------------------------------------+\n");

    for (int i = 0; i < N_BINS; i++)
    {
        int lo = i * 5, hi = lo + 5;
        int w = (peak > 0) ? (int)((double)r->bins[i] / (double)peak * (double)BAR_MAX_W) : 0;
        printf("  | %3d-%3d%%  | %7lld | ", lo, hi, r->bins[i]);
        for (int k = 0; k < w; k++)
            putchar((i == ideal_bin) ? '*' : '#');
        for (int k = w; k < BAR_MAX_W; k++)
            putchar(' ');
        printf(" |%s\n", (i == ideal_bin) ? " <- ideal" : "");
    }
    printf("  +-----------+---------+--------------------------------------------------+\n");
    printf("  mu=%.2f%%  sigma=%.2f%%  range=[%.1f%%, %.1f%%]  nibble-bias=%.2f%%\n",
           r->mean, r->std, r->min_pct, r->max_pct, r->nib_bias);
}

/* ─────────────────────────────────────────────────────────────────────
 *  CSV output (for plot_field_size_sweep.py)
 * ──────────────────────────────────────────────────────────────────── */
static void write_csv(const FsResult rs[], int n, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "[WARN] Cannot write CSV to: %s\n", path);
        return;
    }

    /* header */
    fprintf(f, "fieldsize,cells,mean,std,min,max,nib_bias_pct");
    for (int i = 0; i < N_BINS; i++)
        fprintf(f, ",bin%d", i);
    fprintf(f, "\n");

    /* rows */
    for (int i = 0; i < n; i++)
    {
        const FsResult *r = &rs[i];
        fprintf(f, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f",
                r->fs, r->cells, r->mean, r->std, r->min_pct, r->max_pct, r->nib_bias);
        for (int b = 0; b < N_BINS; b++)
            fprintf(f, ",%lld", r->bins[b]);
        fprintf(f, "\n");
    }
    fclose(f);
    printf("\n[CSV] Written: %s\n", path);
}

/* ─────────────────────────────────────────────────────────────────────
 *  main
 * ──────────────────────────────────────────────────────────────────── */
int main(void)
{
    rng_s = UINT64_C(0xDEADBEEFCAFE1234);  /* fixed seed for reproducibility */

    printf("=== Secasy Field-Size Diffusion & Symmetry Sweep ===\n");
    printf("Config: %d messages x %d bytes, %d input bits each\n",
           N_MESSAGES, MSG_BYTES, MSG_BYTES * 8);
    printf("        -> %lld avalanche samples per field size\n",
           (long long)N_MESSAGES * MSG_BYTES * 8);
    printf("        RNG seed: fixed (0xDEADBEEFCAFE1234)\n\n");

    FsResult results[N_FS];

    for (int i = 0; i < N_FS; i++)
    {
        int fs = FIELD_SIZES[i];
        printf("[%d/%d] Testing %dx%d grid (%d cells)...  ", i + 1, N_FS, fs, fs, fs * fs);
        fflush(stdout);
        run_fs(fs, &results[i]);
        printf("done  (mu=%.1f%%  sigma=%.2f%%  nibble-bias=%.2f%%)\n",
               results[i].mean, results[i].std, results[i].nib_bias);
        print_histogram(&results[i]);
    }

    /* ── Summary table ─────────────────────────────────────────────── */
    printf("\n\n=== Summary Table ===\n");
    printf("%-10s  %-8s  %-8s  %-8s  %-8s  %-8s  %-12s\n",
           "FieldSize", "Cells", "Mean%", "Sigma%", "Min%", "Max%", "NibBias%");
    printf("%-10s  %-8s  %-8s  %-8s  %-8s  %-8s  %-12s\n",
           "----------", "------", "------", "------", "------", "------", "----------");
    for (int i = 0; i < N_FS; i++)
    {
        const FsResult *r = &results[i];
        const char *mk = (r->fs == 16) ? "  [baseline]" : "";
        printf("%-10d  %-8d  %-8.2f  %-8.2f  %-8.2f  %-8.2f  %-12.2f%s\n",
               r->fs, r->cells, r->mean, r->std, r->min_pct, r->max_pct, r->nib_bias, mk);
    }

    write_csv(results, N_FS, "field_size_results.csv");

    return 0;
}
