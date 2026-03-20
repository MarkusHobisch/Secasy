/*
 * Single-Bit-Flip Collision Search
 * ══════════════════════════════════
 *
 * PURPOSE:
 *   Search for collisions caused by single-bit input flips — the adversarial
 *   scenario most likely to yield structural bias in a hash function.
 *   Because full-width (512-bit) collisions are astronomically unlikely even
 *   with this strategy, the test works on TRUNCATED hash prefixes where
 *   birthday statistics make collisions observable.
 *
 * RATIONALE:
 *   For a random-looking hash function, a single-bit flip produces an output
 *   statistically independent of the original. The expected collision probability
 *   per pair is therefore 2^(-N) for an N-bit prefix — identical to any random
 *   pair. If the hash has structural bias, the collision rate under single-bit
 *   flips will exceed the birthday expectation.
 *
 * METHOD:
 *   For each of M base messages:
 *     - Compute hash(base)
 *     - Flip each of the B input bits in turn (exhaustive, not random)
 *     - Compare the truncated prefix of hash(flipped) against all previously
 *       seen prefixes via a hashtable
 *   Total pairs = M * B.  Three prefix widths are tested: 16, 24, and 32 bits.
 *
 * EXPECTED RESULTS (ideal hash):
 *   Collisions ≈ M*B * M*B / 2^(N+1)  (birthday approximation for large tables)
 *   In practice with M=50000, B=128: ~6.4e6 pairs per prefix width.
 *     - 16-bit: ~3200 collisions expected
 *     - 24-bit: ~12   collisions expected
 *     - 32-bit: ~0.05 collisions expected (unlikely to observe any)
 *
 * COLLISION TYPES DISTINGUISHED:
 *   - Self-flip: base[i] vs flip(base[i], bit j) — direct single-bit flip pair
 *   - Cross-flip: flip(base[i], bit j) vs flip(base[k], bit l) — two different flips
 *
 * CONCLUSION criterion:
 *   If observed collisions exceed 4× the birthday expectation at any prefix
 *   width, a structural bias is flagged as CONCERNING.
 *
 * BUILD TARGET: SecasySingleBitCollision
 * HASH SIZE:    DEFAULT_BIT_SIZE (512), prefix comparison only
 *
 * CLI OPTIONS:
 *   -m <count>   Number of base messages (default: 50000)
 *   -r <rounds>  Hash rounds (default: DEFAULT_NUMBER_OF_ROUNDS)
 *   -n <bits>    Hash buffer size in bits (default: DEFAULT_BIT_SIZE)
 *   -s <seed>    RNG seed (default: time-based)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

/* Globals required by Secasy core */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

/* ── RNG ──────────────────────────────────────────────────── */

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;

static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void rng_seed(uint64_t s)
{
    rng_state = (s == 0) ? (uint64_t)time(NULL) * 0x9e3779b97f4a7c15ULL : s;
}

static uint8_t rng_byte(void)
{
    return (uint8_t)(rng_next() >> 56);
}

/* ── Hash prefix extraction ───────────────────────────────── */

/*
 * Extract the first `bits` bits of a hex hash string as a uint64_t.
 * bits must be <= 64.
 */
static uint64_t extract_prefix(const char *hex, int bits)
{
    int nibbles = (bits + 3) / 4;
    uint64_t val = 0;
    for (int i = 0; i < nibbles && hex[i]; i++)
    {
        int n = secasy_hex_nibble(hex[i]);
        if (n < 0)
            n = 0;
        val = (val << 4) | (uint64_t)n;
    }
    /* If bits is not a multiple of 4, mask off the low bits of the last nibble. */
    int rem = bits % 4;
    if (rem != 0)
        val >>= (4 - rem);
    return val;
}

/* ── Open-addressing hashtable for prefix collision detection ─ */

#define TABLE_LOAD_NUM 3
#define TABLE_LOAD_DEN 4

typedef struct
{
    uint64_t prefix;    /* stored prefix value */
    uint32_t base_idx;  /* which base message this came from */
    uint8_t  bit_idx;   /* which bit was flipped (255 = original) */
    uint8_t  occupied;
} TableEntry;

typedef struct
{
    TableEntry *entries;
    size_t      capacity;
    size_t      count;
    int         prefix_bits;
    uint64_t    prefix_mask;
} PrefixTable;

static int table_create(PrefixTable *t, size_t capacity, int prefix_bits)
{
    /* Round up to next power of two */
    size_t cap = 1;
    while (cap < capacity)
        cap <<= 1;
    cap <<= 1; /* extra headroom */

    t->entries = (TableEntry *)calloc(cap, sizeof(TableEntry));
    if (!t->entries)
        return -1;

    t->capacity    = cap;
    t->count       = 0;
    t->prefix_bits = prefix_bits;
    t->prefix_mask = (prefix_bits >= 64) ? ~(uint64_t)0 : ((uint64_t)1 << prefix_bits) - 1;
    return 0;
}

static void table_destroy(PrefixTable *t)
{
    free(t->entries);
    t->entries = NULL;
}

/*
 * Try to insert prefix into the table.
 * Returns 0 and stores entry if new.
 * Returns 1 (collision) and fills `out` with the colliding entry if duplicate.
 */
static int table_insert_or_collide(PrefixTable *t, uint64_t prefix,
                                   uint32_t base_idx, uint8_t bit_idx,
                                   TableEntry *out)
{
    uint64_t masked = prefix & t->prefix_mask;
    size_t idx = (size_t)(masked % t->capacity);

    for (size_t probe = 0; probe < t->capacity; probe++)
    {
        size_t i = (idx + probe) % t->capacity;

        if (!t->entries[i].occupied)
        {
            t->entries[i].prefix    = masked;
            t->entries[i].base_idx  = base_idx;
            t->entries[i].bit_idx   = bit_idx;
            t->entries[i].occupied  = 1;
            t->count++;
            return 0;
        }

        if (t->entries[i].prefix == masked)
        {
            *out = t->entries[i];
            return 1;
        }
    }

    /* Table full — this should not happen with proper sizing */
    return 0;
}

/* ── Per-prefix-width test ────────────────────────────────── */

typedef struct
{
    int      prefix_bits;
    uint64_t total_pairs;     /* M * B */
    uint64_t collisions;
    uint64_t self_flip_coll;  /* base vs its own flipped version */
    uint64_t cross_flip_coll; /* flipped vs different flipped */
    double   expected;
} TestResult;

static int verbose_mode = 0;

static const uint64_t MAX_VERBOSE_COLLISIONS = 20;

/*
 * Run the single-bit-flip collision search for one prefix width.
 *
 * base_hashes[i]  — precomputed hex hash of base message i
 * bases[i]        — the actual input bytes of base message i
 * num_bases       — M
 * input_len       — length of each input in bytes (B = input_len * 8)
 * prefix_bits     — N: how many output bits to compare
 */
static void print_collision_pair(
    const uint8_t (*bases)[16],
    int input_len,
    int prefix_bits,
    uint64_t prefix_val,
    uint32_t base_a, uint8_t bit_a,
    uint32_t base_b, uint8_t bit_b)
{
    printf("    Collision (%d-bit prefix = 0x%0*" PRIx64 "):\n",
           prefix_bits, (prefix_bits + 3) / 4, prefix_val);

    uint8_t msg_a[16];
    memcpy(msg_a, bases[base_a], (size_t)input_len);
    msg_a[bit_a / 8] ^= (uint8_t)(1u << (bit_a % 8));
    printf("      A: base[%u] flip-bit %u  bytes=", base_a, (unsigned)bit_a);
    for (int k = 0; k < input_len; k++) printf("%02x", msg_a[k]);
    printf("\n");

    uint8_t msg_b[16];
    memcpy(msg_b, bases[base_b], (size_t)input_len);
    msg_b[bit_b / 8] ^= (uint8_t)(1u << (bit_b % 8));
    printf("      B: base[%u] flip-bit %u  bytes=", base_b, (unsigned)bit_b);
    for (int k = 0; k < input_len; k++) printf("%02x", msg_b[k]);
    printf("\n");
}

static TestResult run_prefix_test(
    char **base_hashes,
    const uint8_t (*bases)[16],
    int num_bases,
    int input_len,
    int prefix_bits)
{
    TestResult result = {0};
    result.prefix_bits = prefix_bits;

    int bits_per_input = input_len * 8;
    uint64_t total_entries = (uint64_t)num_bases * (uint64_t)bits_per_input;

    printf("  [%d-bit prefix] Building table (%"PRIu64" entries)...\n",
           prefix_bits, total_entries);

    PrefixTable table;
    if (table_create(&table, (size_t)(total_entries * TABLE_LOAD_DEN / TABLE_LOAD_NUM + 1024), prefix_bits) != 0)
    {
        fprintf(stderr, "  OOM: cannot allocate hashtable for %d-bit test\n", prefix_bits);
        return result;
    }

    uint64_t collisions = 0;
    uint64_t self_flip  = 0;

    for (int base_i = 0; base_i < num_bases; base_i++)
    {
        uint8_t flipped[16];

        for (int bit = 0; bit < bits_per_input; bit++)
        {
            memcpy(flipped, bases[base_i], (size_t)input_len);
            flipped[bit / 8] ^= (uint8_t)(1u << (bit % 8));

            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(flipped, (size_t)input_len);
            char *flip_hash = calculateHashValue();

            uint64_t prefix = extract_prefix(flip_hash, prefix_bits);
            free(flip_hash);

            TableEntry collision_entry;
            int hit = table_insert_or_collide(&table,
                                              prefix,
                                              (uint32_t)base_i,
                                              (uint8_t)bit,
                                              &collision_entry);
            if (hit)
            {
                collisions++;
                /* Self-flip: same base message, different bit → direct near-collision */
                if (collision_entry.base_idx == (uint32_t)base_i)
                    self_flip++;

                if (verbose_mode && collisions <= MAX_VERBOSE_COLLISIONS)
                {
                    uint64_t masked = prefix & table.prefix_mask;
                    print_collision_pair(bases, input_len, prefix_bits, masked,
                                        collision_entry.base_idx, collision_entry.bit_idx,
                                        (uint32_t)base_i, (uint8_t)bit);
                }
                else if (verbose_mode && collisions == MAX_VERBOSE_COLLISIONS + 1)
                {
                    printf("    (further collisions suppressed — showing first %" PRIu64 " only)\n",
                           MAX_VERBOSE_COLLISIONS);
                }
            }
        }

        if ((base_i + 1) % 5000 == 0)
            printf("    Progress: %d / %d bases\n", base_i + 1, num_bases);
    }

    table_destroy(&table);

    result.total_pairs     = total_entries;
    result.collisions      = collisions;
    result.self_flip_coll  = self_flip;
    result.cross_flip_coll = collisions - self_flip;

    /*
     * Expected collisions: exact occupancy formula for a coupon-collector scenario.
     *
     * When n elements are inserted into M = 2^prefix_bits buckets uniformly,
     * the expected number that collide with an earlier entry is:
     *     E = n - M * (1 - (1 - 1/M)^n)  ≈  n - M * (1 - exp(-n/M))
     *
     * The simpler birthday formula  n²/(2M)  is only valid when n << M.
     * For 16-bit prefixes with n ≈ 6.4M, n >> M = 65536, so the exact formula
     * must be used to avoid grossly over-stating the expectation.
     */
    double n = (double)total_entries;
    double M = pow(2.0, (double)prefix_bits);
    result.expected = n - M * (1.0 - exp(-n / M));

    return result;
}

/* ── Usage ────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-m messages] [-r rounds] [-n hashBits] [-s seed] [-v]\n"
            "  -m <count>   Base messages to test (default 50000)\n"
            "  -r <rounds>  Hash rounds (default %d)\n"
            "  -n <bits>    Hash buffer size in bits (default %d)\n"
            "  -s <seed>    RNG seed (0 = time-based)\n"
            "  -v           Verbose: print individual collision pairs (max 20 per width)\n",
            prog, DEFAULT_NUMBER_OF_ROUNDS, DEFAULT_BIT_SIZE);
}

/* ── main ─────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    int    num_bases  = 50000;
    uint64_t seed     = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            num_bases = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc)
            numberOfRounds = strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            hashLengthInBits = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            seed = strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-v") == 0)
            verbose_mode = 1;
        else
        {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (num_bases <= 0)
    {
        fprintf(stderr, "Error: -m must be positive\n");
        return EXIT_FAILURE;
    }

    rng_seed(seed);

    static const int INPUT_LEN   = 16;   /* bytes per message */
    static const int BITS_PER_INPUT = 128; /* INPUT_LEN * 8 */

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Single-Bit-Flip Collision Search                            ║\n");
    printf("║  Searching for hash collisions caused by 1-bit input flips   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  Base messages:  %d\n",        num_bases);
    printf("  Input length:   %d bytes (%d bits per message)\n", INPUT_LEN, BITS_PER_INPUT);
    printf("  Total flip ops: %"PRIu64"\n", (uint64_t)num_bases * (uint64_t)BITS_PER_INPUT);
    printf("  Rounds:         %lu\n",       numberOfRounds);
    printf("  Hash size:      %d bits\n\n", hashLengthInBits);

    /* Allocate base message storage */
    uint8_t (*bases)[16] = malloc((size_t)num_bases * sizeof(*bases));
    char   **base_hashes  = malloc((size_t)num_bases * sizeof(char *));
    if (!bases || !base_hashes)
    {
        fprintf(stderr, "OOM allocating base arrays\n");
        free(bases);
        free(base_hashes);
        return EXIT_FAILURE;
    }

    /* Generate base messages and their hashes */
    printf("Generating %d base messages and hashes...\n", num_bases);
    for (int i = 0; i < num_bases; i++)
    {
        for (int b = 0; b < INPUT_LEN; b++)
            bases[i][b] = rng_byte();

        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(bases[i], INPUT_LEN);
        base_hashes[i] = calculateHashValue();

        if ((i + 1) % 10000 == 0)
            printf("  %d / %d\n", i + 1, num_bases);
    }

    printf("\nRunning collision search across 3 prefix widths...\n\n");

    static const int PREFIX_WIDTHS[] = {16, 24, 32};
    static const int NUM_WIDTHS = 3;

    TestResult results[3];

    for (int w = 0; w < NUM_WIDTHS; w++)
        results[w] = run_prefix_test(base_hashes, (const uint8_t (*)[16])bases,
                                     num_bases, INPUT_LEN, PREFIX_WIDTHS[w]);

    /* Free hashes */
    for (int i = 0; i < num_bases; i++)
        free(base_hashes[i]);

    free(base_hashes);
    free(bases);

    /* Print results */
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Results                                                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    int any_concerning = 0;

    for (int w = 0; w < NUM_WIDTHS; w++)
    {
        const TestResult *r = &results[w];
        double ratio = (r->expected > 0.0) ? (double)r->collisions / r->expected : 0.0;
        int concerning = (ratio > 4.0 && r->collisions > 0);

        if (concerning)
            any_concerning = 1;

        printf("  ── %d-bit prefix ─────────────────────────────────────────\n",
               r->prefix_bits);
        printf("  Total flip pairs:    %"PRIu64"\n",    r->total_pairs);
        printf("  Expected collisions: %.2f\n",         r->expected);
        printf("  Observed collisions: %"PRIu64"\n",    r->collisions);
        printf("  Ratio obs/expected:  %.3f\n",         ratio);
        printf("  Self-flip coll.:     %"PRIu64"  (base[i] vs flip(base[i],bit j))\n",
               r->self_flip_coll);
        printf("  Cross-flip coll.:    %"PRIu64"  (flip(base[i],j) vs flip(base[k],l))\n",
               r->cross_flip_coll);

        if (concerning)
            printf("  Status: ⚠ CONCERNING — collision rate %.1fx above birthday bound\n\n", ratio);
        else if (r->collisions == 0 && r->expected < 0.5)
            printf("  Status: ✓ PASS — no collisions expected at this width\n\n");
        else
            printf("  Status: ✓ PASS — within birthday expectation (ratio %.2f)\n\n", ratio);
    }

    printf("══════════════════════════════════════════════════════════════\n");
    if (any_concerning)
    {
        printf("VERDICT: ⚠ CONCERNING — single-bit flips produce collision rates\n");
        printf("         exceeding the birthday bound. Structural bias suspected.\n");
    }
    else
    {
        printf("VERDICT: ✓ PASS — single-bit-flip collision rates match birthday\n");
        printf("         expectation. No structural input-output bias detected.\n");
    }
    printf("══════════════════════════════════════════════════════════════\n\n");

    return any_concerning ? EXIT_FAILURE : EXIT_SUCCESS;
}
