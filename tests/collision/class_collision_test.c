/*
 * ASCII-Class Collision Test
 * ==========================
 *
 * PURPOSE:
 *   Test whether inputs restricted to specific ASCII character classes
 *   produce elevated collision rates relative to the birthday expectation.
 *
 * BACKGROUND:
 *   Secasy's init phase decomposes each input byte into 4 direction steps
 *   of 2 bits each (LSB-first).  For all ASCII decimal digits ('0'–'9',
 *   0x30–0x39), bits 4–5 = 11 (DOWN) and bits 6–7 = 00 (UP) for every
 *   character.  Steps 3 and 4 are therefore CONSTANT across the whole
 *   class, reducing the effective per-byte entropy from 8 bits to 4 bits.
 *
 *   The same applies to two uppercase letter ranges:
 *     'A'–'O' (0x41–0x4F): step3=UP,    step4=RIGHT  (constant)
 *     'P'–'Z' (0x50–0x5A): step3=RIGHT, step4=RIGHT  (constant)
 *   Note: 'U' (0x55 = 01010101) encodes ALL FOUR steps as RIGHT —
 *   the most degenerate single character in the printable ASCII range.
 *
 * HYPOTHESIS:
 *   If Secasy's processing phase provides sufficient diffusion to compensate
 *   for this structural input-entropy reduction, the observed collision rate
 *   should remain near the birthday expectation for all classes.
 *   A ratio (observed / expected) significantly above 1.0 would indicate
 *   that the diffusion does NOT fully compensate for reduced input entropy.
 *
 * METHOD:
 *   For each class (DIGIT, UPPER1, UPPER2) and a fully-random CONTROL:
 *     1. Generate N random strings drawn uniformly from the class alphabet.
 *     2. Hash each string with default Secasy parameters.
 *     3. Insert 16-bit, 24-bit, and 32-bit hash prefixes into collision tables.
 *     4. Compare observed collisions to the exact birthday expectation.
 *
 *   Expected collisions (exact occupancy formula):
 *     E = n - M * (1 - exp(-n/M))   where M = 2^prefix_bits
 *
 * VERDICT:
 *   Ratio < 2.0: No structural bias detectable.
 *   Ratio 2–4x: Moderate concern — warrants further analysis.
 *   Ratio > 4x: Structural bias strongly indicated.
 *
 * BUILD TARGET: SecasyClassCollision
 *
 * CLI OPTIONS:
 *   -m <count>   Strings per class (default 20000)
 *   -l <len>     String length in characters (default 16)
 *   -r <rounds>  Hash rounds (default DEFAULT_NUMBER_OF_ROUNDS)
 *   -s <seed>    RNG seed (0 = time-based)
 *   -v           Verbose: print first 10 collision pairs per bucket
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
unsigned long numberOfRounds  = DEFAULT_NUMBER_OF_ROUNDS;
int           hashLengthInBits = DEFAULT_BIT_SIZE;

/* ── RNG (xorshift64 * Weyl) ─────────────────────────────── */

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

/* ── Hash prefix extraction ───────────────────────────────── */

static uint64_t extract_prefix(const char *hex, int bits)
{
    int     nibbles = (bits + 3) / 4;
    uint64_t val    = 0;
    for (int i = 0; i < nibbles && hex[i]; i++)
    {
        int n = secasy_hex_nibble(hex[i]);
        val = (val << 4) | (uint64_t)(n < 0 ? 0 : n);
    }
    int rem = bits % 4;
    if (rem != 0)
        val >>= (4 - rem);
    return val;
}

/* ── Open-addressing prefix collision table ───────────────── */

#define TABLE_LOAD_NUM 3
#define TABLE_LOAD_DEN 4

typedef struct
{
    uint64_t prefix;
    uint32_t msg_idx;
    uint8_t  occupied;
} TableEntry;

typedef struct
{
    TableEntry *entries;
    size_t      capacity;
    int         prefix_bits;
    uint64_t    prefix_mask;
} PrefixTable;

static int table_create(PrefixTable *t, size_t min_cap, int prefix_bits)
{
    size_t cap = 1;
    while (cap < min_cap) cap <<= 1;
    cap <<= 1;

    t->entries     = (TableEntry *)calloc(cap, sizeof(TableEntry));
    t->capacity    = cap;
    t->prefix_bits = prefix_bits;
    t->prefix_mask = (prefix_bits >= 64) ? ~(uint64_t)0
                                         : ((uint64_t)1 << prefix_bits) - 1;
    return t->entries ? 0 : -1;
}

static void table_destroy(PrefixTable *t)
{
    free(t->entries);
    t->entries = NULL;
}

/* Returns 1 on collision, 0 on new insertion. */
static int table_insert(PrefixTable *t, uint64_t prefix, uint32_t msg_idx)
{
    uint64_t masked = prefix & t->prefix_mask;
    size_t   idx    = (size_t)(masked % t->capacity);

    for (size_t probe = 0; probe < t->capacity; probe++)
    {
        size_t i = (idx + probe) % t->capacity;
        if (!t->entries[i].occupied)
        {
            t->entries[i].prefix   = masked;
            t->entries[i].msg_idx  = msg_idx;
            t->entries[i].occupied = 1;
            return 0;
        }
        if (t->entries[i].prefix == masked)
            return 1;
    }
    return 0; /* table full — should not happen with proper sizing */
}

/* ── Character classes ────────────────────────────────────── */

typedef struct
{
    const char *name;
    const char *description;
    const char *alphabet;
    int         alphabet_size;
} CharClass;

static const CharClass CLASSES[] = {
    {
        "CONTROL",
        "Random bytes 0x00-0xFF (full 8-bit entropy baseline)",
        NULL, /* NULL = use raw random bytes, not an alphabet */
        256
    },
    {
        "DIGIT",
        "'0'-'9' (0x30-0x39): step3=DOWN, step4=UP constant for all digits",
        "0123456789",
        10
    },
    {
        "UPPER1",
        "'A'-'O' (0x41-0x4F): step3=UP, step4=RIGHT constant",
        "ABCDEFGHIJKLMNO",
        15
    },
    {
        "UPPER2",
        "'P'-'Z' (0x50-0x5A): step3=RIGHT, step4=RIGHT constant ('U'=all-RIGHT)",
        "PQRSTUVWXYZ",
        11
    },
};
static const int NUM_CLASSES = (int)(sizeof(CLASSES) / sizeof(CLASSES[0]));

/* ── Per-prefix results ───────────────────────────────────── */

typedef struct
{
    int      prefix_bits;
    uint64_t n;
    uint64_t collisions;
    double   expected;
    double   ratio;
} PrefixResult;

/* ── Per-class test ───────────────────────────────────────── */

typedef struct
{
    const char   *class_name;
    PrefixResult  prefix[3];
    int           any_concerning;
} ClassResult;

static const int PREFIX_WIDTHS[]  = {16, 24, 32};
static const int NUM_WIDTHS       = 3;
static const int VERBOSE_LIMIT    = 10;

static ClassResult run_class_test(
    const CharClass *cls,
    int              n_strings,
    int              str_len,
    int              verbose)
{
    ClassResult cr;
    memset(&cr, 0, sizeof(cr));
    cr.class_name = cls->name;

    printf("  Class %-8s  [%s]\n", cls->name, cls->description);
    printf("    Generating %d strings of length %d...\n", n_strings, str_len);

    /* Allocate string buffer: n_strings × str_len bytes */
    unsigned char *messages = (unsigned char *)malloc((size_t)n_strings * (size_t)str_len);
    if (!messages)
    {
        fprintf(stderr, "OOM allocating messages for class %s\n", cls->name);
        return cr;
    }

    char **hashes = (char **)malloc((size_t)n_strings * sizeof(char *));
    if (!hashes)
    {
        fprintf(stderr, "OOM allocating hash array for class %s\n", cls->name);
        free(messages);
        return cr;
    }

    /* Generate strings and compute hashes */
    for (int i = 0; i < n_strings; i++)
    {
        unsigned char *msg = messages + (size_t)i * (size_t)str_len;
        if (cls->alphabet)
        {
            for (int j = 0; j < str_len; j++)
                msg[j] = (unsigned char)cls->alphabet[rng_next() % (uint64_t)cls->alphabet_size];
        }
        else
        {
            /* Control: raw random bytes */
            for (int j = 0; j < str_len; j++)
                msg[j] = (unsigned char)(rng_next() >> 56);
        }

        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, (size_t)str_len);
        hashes[i] = calculateHashValue();

        if ((i + 1) % 5000 == 0)
            printf("    Progress: %d / %d\n", i + 1, n_strings);
    }

    /* For each prefix width: insert all hashes, count collisions */
    for (int w = 0; w < NUM_WIDTHS; w++)
    {
        int         pbits    = PREFIX_WIDTHS[w];
        PrefixTable table;
        size_t      min_cap  = (size_t)n_strings * TABLE_LOAD_DEN / TABLE_LOAD_NUM + 1024;

        if (table_create(&table, min_cap, pbits) != 0)
        {
            fprintf(stderr, "OOM for %d-bit table, class %s\n", pbits, cls->name);
            continue;
        }

        uint64_t collisions  = 0;
        uint64_t verbose_cnt = 0;

        for (int i = 0; i < n_strings; i++)
        {
            uint64_t prefix = extract_prefix(hashes[i], pbits);
            int hit = table_insert(&table, prefix, (uint32_t)i);
            if (hit)
            {
                collisions++;
                if (verbose && verbose_cnt < (uint64_t)VERBOSE_LIMIT)
                {
                    printf("    [%d-bit] Collision #%"PRIu64": msg[%d] prefix=0x%0*"PRIx64"\n",
                           pbits, collisions,
                           i, (pbits + 3) / 4, prefix & table.prefix_mask);
                    printf("      Input: ");
                    const unsigned char *msg = messages + (size_t)i * (size_t)str_len;
                    for (int k = 0; k < str_len && k < 32; k++) printf("%c", (msg[k] >= 0x20 && msg[k] < 0x7F) ? (char)msg[k] : '.');
                    printf("\n");
                    verbose_cnt++;
                }
            }
        }

        table_destroy(&table);

        double n_d  = (double)n_strings;
        double M    = pow(2.0, (double)pbits);
        double exp_coll = n_d - M * (1.0 - exp(-n_d / M));

        cr.prefix[w].prefix_bits = pbits;
        cr.prefix[w].n           = (uint64_t)n_strings;
        cr.prefix[w].collisions  = collisions;
        cr.prefix[w].expected    = exp_coll;
        cr.prefix[w].ratio       = (exp_coll > 0.001) ? (double)collisions / exp_coll : 0.0;

        if (cr.prefix[w].ratio > 4.0 && collisions > 0)
            cr.any_concerning = 1;
    }

    /* Free hashes */
    for (int i = 0; i < n_strings; i++)
        free(hashes[i]);
    free(hashes);
    free(messages);

    return cr;
}

/* ── Usage ────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-m count] [-l len] [-r rounds] [-s seed] [-v]\n"
            "  -m <count>   Strings per class (default 20000)\n"
            "  -l <len>     String length in chars (default 16)\n"
            "  -r <rounds>  Hash rounds (default %d)\n"
            "  -s <seed>    RNG seed (0 = time-based)\n"
            "  -v           Verbose: show first %d collision pairs per prefix\n",
            prog, DEFAULT_NUMBER_OF_ROUNDS, VERBOSE_LIMIT);
}

/* ── main ─────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    int      n_strings = 20000;
    int      str_len   = 16;
    uint64_t seed      = 0;
    int      verbose   = 0;

    for (int i = 1; i < argc; i++)
    {
        if      (strcmp(argv[i], "-m") == 0 && i + 1 < argc) n_strings = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) str_len   = (int)strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) numberOfRounds = strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) seed      = strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "-v") == 0)                  verbose   = 1;
        else { print_usage(argv[0]); return EXIT_FAILURE; }
    }

    if (n_strings <= 0 || str_len <= 0)
    {
        fprintf(stderr, "Error: -m and -l must be positive\n");
        return EXIT_FAILURE;
    }

    rng_seed(seed);

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  ASCII-Class Collision Test                                  ║\n");
    printf("║  Hypothesis: restricted alphabets → elevated collision rate  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    printf("Configuration:\n");
    printf("  Strings per class: %d\n", n_strings);
    printf("  String length:     %d chars\n", str_len);
    printf("  Rounds:            %lu\n", numberOfRounds);
    printf("  Hash size:         %d bits\n\n", hashLengthInBits);

    /* Expected collisions for reference */
    printf("Expected collisions (birthday bound) per class:\n");
    for (int w = 0; w < NUM_WIDTHS; w++)
    {
        double n_d  = (double)n_strings;
        double M    = pow(2.0, (double)PREFIX_WIDTHS[w]);
        double e    = n_d - M * (1.0 - exp(-n_d / M));
        printf("  %2d-bit prefix: %.2f\n", PREFIX_WIDTHS[w], e);
    }
    printf("\n");

    ClassResult results[4];

    for (int c = 0; c < NUM_CLASSES; c++)
    {
        printf("─────────────────────────────────────────────────────────────\n");
        results[c] = run_class_test(&CLASSES[c], n_strings, str_len, verbose);
        printf("\n");
    }

    /* ── Summary table ─────────────────────────────────────── */

    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Results Summary                                             ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("%-8s  %6s  %8s  %8s  %8s  %s\n",
           "Class", "Prefix", "Expected", "Observed", "Ratio", "Status");
    printf("%.8s  %.6s  %.8s  %.8s  %.8s  %.8s\n",
           "--------", "------", "--------", "--------", "--------", "--------");

    int any_concerning = 0;

    for (int c = 0; c < NUM_CLASSES; c++)
    {
        const ClassResult *cr = &results[c];
        for (int w = 0; w < NUM_WIDTHS; w++)
        {
            const PrefixResult *pr = &cr->prefix[w];
            const char *status;
            if      (pr->ratio > 4.0 && pr->collisions > 0) { status = "⚠ CONCERNING"; any_concerning = 1; }
            else if (pr->ratio > 2.0 && pr->collisions > 0)   status = "△ ELEVATED";
            else if (pr->expected < 0.5 && pr->collisions == 0) status = "✓ PASS (none expected)";
            else                                                 status = "✓ PASS";

            printf("%-8s  %5d-bit  %8.2f  %8"PRIu64"  %8.3f  %s\n",
                   cr->class_name,
                   pr->prefix_bits,
                   pr->expected,
                   pr->collisions,
                   pr->ratio,
                   status);
        }
        printf("\n");
    }

    /* ── Relative analysis: compare restricted classes vs CONTROL ── */
    printf("─────────────────────────────────────────────────────────────\n");
    printf("Ratio vs. CONTROL class:\n");
    for (int w = 0; w < NUM_WIDTHS; w++)
    {
        const PrefixResult *ctrl = &results[0].prefix[w]; /* CONTROL is index 0 */
        printf("  %2d-bit  CONTROL=%"PRIu64, ctrl->prefix_bits, ctrl->collisions);
        for (int c = 1; c < NUM_CLASSES; c++)
        {
            const PrefixResult *pr = &results[c].prefix[w];
            double vs_ctrl = (ctrl->collisions > 0)
                           ? (double)pr->collisions / (double)ctrl->collisions
                           : 0.0;
            printf("  %s=%.2fx", results[c].class_name, vs_ctrl);
        }
        printf("\n");
    }

    printf("\n══════════════════════════════════════════════════════════════\n");
    if (any_concerning)
    {
        printf("VERDICT: ⚠ CONCERNING — at least one class exceeds 4x birthday bound.\n");
        printf("         Input-entropy reduction is NOT compensated by diffusion.\n");
    }
    else
    {
        printf("VERDICT: ✓ PASS — all classes within expected birthday bounds.\n");
        printf("         Diffusion appears sufficient to compensate for restricted alphabets.\n");
    }
    printf("══════════════════════════════════════════════════════════════\n");

    return any_concerning ? EXIT_FAILURE : EXIT_SUCCESS;
}
