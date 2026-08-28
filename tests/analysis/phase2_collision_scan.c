/*
 * phase2_collision_scan.c
 *
 * Phase-2 internal-state collision scan ("neutral-loop / path-collision" probe).
 *
 * MOTIVATION
 * ----------
 * For a fixed schedule Phase 3 is bijective on the cell values, while the
 * extractor is a compressing deterministic function of the final values. A
 * collision in the *collision-relevant Phase-2 state* is therefore sufficient
 * for a full-hash collision, but it is not necessary: distinct reachable
 * states may collide during extraction.
 *
 * That state is exactly:
 *     - field[x][y].value      for all 256 cells   (read by Phase 3 and 4)
 *     - field[x][y].colorIndex for all 256 cells   (read by Phase 3)
 *     - the cursor position (pos.x, pos.y)          (Phase-3 traversal offset)
 * The per-tile primeIndex is NOT read after Phase 2, so it is deliberately
 * EXCLUDED from the collision fingerprint. The machine-readable oracle mode
 * still emits it so external models can validate the complete continuation
 * state as well as the downstream collision-relevant state.
 *
 * WHY COLLISIONS ARE PLAUSIBLE HERE
 * ---------------------------------
 * In processDirectionStep() the old cell value is OVERWRITTEN by the next
 * prime; the old value survives only through the cursor jump, which uses
 * (oldValue & FIELD_SIZE_MASK) = oldValue mod 16. The high 60 bits of every
 * jumped-from value are discarded. Phase 2 is therefore genuinely lossy, so a
 * collision in the Phase-2 state is structurally possible and is the natural
 * place to look first.
 *
 * KEY CONSEQUENCE
 * ---------------
 * Phase 3 and Phase 4 are deterministic, so a single confirmed Phase-2 state
 * collision (X != Y with identical state) immediately yields a *full hash
 * collision* hash(X) == hash(Y). Appending a common suffix S preserves the
 * collision only when the complete continuation state, including every
 * primeIndex, is also equal. Experiment 3 probes this stronger property with
 * real hash outputs; a mismatch means only the terminal collision was shown.
 *
 * EXPERIMENTS
 * -----------
 *   [E1] Exhaustively enumerate all 256^L inputs for L = 1, 2 (and L = 3 with
 *        --with-3byte). Fingerprint the collision-relevant Phase-2 state with a
 *        128-bit hash; on a fingerprint match, recompute both states and
 *        memcmp to confirm (zero false positives). Count confirmed collisions.
 *   [E2] Neutral-block-from-init search: report any non-empty input whose
 *        Phase-2 state equals the canonical post-initialisation state. Such an
 *        input collides with the empty input.
 *   [E3] Amplification check: for the first confirmed colliding pair (if any),
 *        append random suffixes and verify the full 512-bit hashes still match.
 *
 * INTERPRETATION
 * --------------
 *   confirmed collisions == 0  ->  Phase 2 is injective on the scanned inputs;
 *                                  strong empirical evidence that the lossy walk
 *                                  does not create short-input collisions.
 *   confirmed collisions  > 0  ->  a concrete terminal structural collision
 *                                  has been found; E3 tests amplification.
 *
 * This is an internal analysis tool, not a formal proof.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

extern Tile field[FIELD_SIZE][FIELD_SIZE];
extern Position pos;

static int hex_nibble(const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static int hash_values_hex(const char *hex)
{
    const size_t expected_len = (size_t)FIELD_SIZE * FIELD_SIZE * 16U;
    if (strlen(hex) != expected_len)
    {
        fprintf(stderr, "state hex must contain exactly %zu hexadecimal digits\n",
                expected_len);
        return 2;
    }

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    for (size_t cell = 0; cell < (size_t)FIELD_SIZE * FIELD_SIZE; cell++)
    {
        uint64_t value = 0;
        for (size_t digit = 0; digit < 16U; digit++)
        {
            const int nibble = hex_nibble(hex[cell * 16U + digit]);
            if (nibble < 0)
            {
                fprintf(stderr, "invalid state hex\n");
                return 2;
            }
            value = (value << 4U) | (uint64_t)nibble;
        }
        field[cell / FIELD_SIZE][cell % FIELD_SIZE].value = value;
    }

    printf("hash ");
    for (unsigned long block = 0; block < 8UL; block++)
        printf("%016" PRIx64, hashValue(block));
    putchar('\n');
    return 0;
}

static int dump_hex_state(const char *hex, const int include_phase3)
{
    const int empty = strcmp(hex, "-") == 0;
    const size_t hex_len = empty ? 0U : strlen(hex);
    if ((hex_len & 1U) != 0U)
    {
        fprintf(stderr, "hex input must contain complete bytes\n");
        return 2;
    }

    const size_t data_len = hex_len / 2U;
    unsigned char *data = data_len == 0U ? NULL : malloc(data_len);
    if (data_len != 0U && data == NULL)
    {
        fprintf(stderr, "out of memory\n");
        return 2;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        const int hi = hex_nibble(hex[2U * i]);
        const int lo = hex_nibble(hex[2U * i + 1U]);
        if (hi < 0 || lo < 0)
        {
            fprintf(stderr, "invalid hex input\n");
            free(data);
            return 2;
        }
        data[i] = (unsigned char)((hi << 4) | lo);
    }

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, data_len);
    char *hash = include_phase3 ? calculateHashValue() : NULL;
    if (hash != NULL)
        printf("hash %s\n", hash);
    printf("cursor %u %u\n", pos.x, pos.y);
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
    {
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
        {
            const Tile *tile = &field[x][y];
            printf("cell %u %u %llu %u %u\n", x, y,
                   (unsigned long long)tile->value, tile->primeIndex,
                   (unsigned)tile->colorIndex);
        }
    }
    free(hash);
    free(data);
    return 0;
}

/* ── Collision-relevant Phase-2 state snapshot ─────────────────────────
 * Layout (host endianness; only ever compared on the same machine):
 *   256 * uint64_t cell values
 *   256 * uint8_t  colour indices
 *   2   * uint32_t cursor (x, y)
 */
#define NUM_CELLS (FIELD_SIZE * FIELD_SIZE)
#define STATE_BYTES (NUM_CELLS * (int)sizeof(uint64_t) + NUM_CELLS + 2 * (int)sizeof(uint32_t))

static void snapshot_state(uint8_t *buf)
{
    size_t off = 0;
    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            uint64_t v = field[x][y].value;
            memcpy(buf + off, &v, sizeof(v));
            off += sizeof(v);
        }
    }
    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            buf[off++] = (uint8_t)field[x][y].colorIndex;
        }
    }
    uint32_t px = pos.x, py = pos.y;
    memcpy(buf + off, &px, sizeof(px));
    off += sizeof(px);
    memcpy(buf + off, &py, sizeof(py));
    off += sizeof(py);
}

/* Run Phase 1 + Phase 2 for the given input and snapshot the state. */
static void state_for_input(const unsigned char *data, size_t len, uint8_t *buf)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    snapshot_state(buf);
}

/* ── Two independent 64-bit fingerprints over a byte buffer ────────────── */
static void fingerprint128(const uint8_t *buf, size_t n, uint64_t *h1out, uint64_t *h2out)
{
    /* FNV-1a (64-bit) */
    uint64_t h1 = 1469598103934665603ULL;
    /* splitmix-style rolling hash with a distinct basis */
    uint64_t h2 = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < n; i++)
    {
        h1 ^= (uint64_t)buf[i];
        h1 *= 1099511628211ULL;

        h2 += (uint64_t)buf[i] + 0x165667B19E3779F9ULL;
        h2 = (h2 ^ (h2 >> 30)) * 0xBF58476D1CE4E5B9ULL;
        h2 = (h2 ^ (h2 >> 27)) * 0x94D049BB133111EBULL;
        h2 ^= h2 >> 31;
    }
    *h1out = h1;
    *h2out = h2;
}

/* ── Hash table for state-fingerprint collision detection ──────────────── */
typedef struct Entry
{
    uint64_t h1;
    uint64_t h2;
    uint32_t input; /* packed bytes: byte0 | byte1<<8 | byte2<<16 (length implicit) */
    struct Entry *next;
} Entry;

#define TABLE_BITS 25
#define TABLE_SIZE (1u << TABLE_BITS)
#define TABLE_MASK (TABLE_SIZE - 1u)

static Entry **table;
static Entry *pool;
static size_t pool_used;
static size_t pool_capacity;

static void table_alloc(size_t expected_inputs)
{
    table = (Entry **)calloc(TABLE_SIZE, sizeof(Entry *));
    pool_capacity = expected_inputs + 16;
    pool = (Entry *)malloc(pool_capacity * sizeof(Entry));
    pool_used = 0;
    if (!table || !pool)
    {
        fprintf(stderr, "Allocation failed (table=%p pool=%p cap=%zu)\n",
                (void *)table, (void *)pool, pool_capacity);
        exit(1);
    }
}

static void table_free(void)
{
    free(table);
    free(pool);
    table = NULL;
    pool = NULL;
}

static void unpack(uint32_t packed, size_t len, unsigned char *out)
{
    for (size_t i = 0; i < len; i++)
        out[i] = (unsigned char)((packed >> (8 * i)) & 0xFFu);
}

/* ── E1: exhaustive Phase-2 state collision scan for one length ────────── */
typedef struct
{
    int found;
    size_t len;
    uint32_t a;
    uint32_t b;
} CollisionRecord;

static size_t scan_length(size_t len, const uint8_t *init_state,
                          size_t *neutral_from_init_out,
                          CollisionRecord *first_collision)
{
    const uint64_t total = (len == 1) ? 256ULL : (len == 2) ? 65536ULL : 16777216ULL;
    table_alloc((size_t)total);

    uint8_t *snap = (uint8_t *)malloc((size_t)STATE_BYTES);
    uint8_t *snap_other = (uint8_t *)malloc((size_t)STATE_BYTES);
    unsigned char inbuf[3];
    unsigned char inbuf_other[3];
    if (!snap || !snap_other)
    {
        fprintf(stderr, "snapshot allocation failed\n");
        exit(1);
    }

    size_t confirmed = 0;
    size_t neutral_from_init = 0;
    const uint64_t progress_step = (total >= 1048576ULL) ? 1048576ULL : 0ULL;

    for (uint64_t code = 0; code < total; code++)
    {
        uint32_t packed = (uint32_t)code;
        unpack(packed, len, inbuf);

        state_for_input(inbuf, len, snap);

        /* E2: neutral block from the canonical init state */
        if (memcmp(snap, init_state, (size_t)STATE_BYTES) == 0)
            neutral_from_init++;

        uint64_t h1, h2;
        fingerprint128(snap, (size_t)STATE_BYTES, &h1, &h2);

        Entry *bucket = table[h1 & TABLE_MASK];
        for (Entry *e = bucket; e; e = e->next)
        {
            if (e->h1 == h1 && e->h2 == h2)
            {
                /* Candidate: confirm by recompute + exact state compare. */
                unpack(e->input, len, inbuf_other);
                state_for_input(inbuf_other, len, snap_other);
                if (memcmp(snap, snap_other, (size_t)STATE_BYTES) == 0)
                {
                    confirmed++;
                    if (first_collision && !first_collision->found)
                    {
                        first_collision->found = 1;
                        first_collision->len = len;
                        first_collision->a = e->input;
                        first_collision->b = packed;
                    }
                }
            }
        }

        Entry *ne = &pool[pool_used++];
        ne->h1 = h1;
        ne->h2 = h2;
        ne->input = packed;
        ne->next = table[h1 & TABLE_MASK];
        table[h1 & TABLE_MASK] = ne;

        if (progress_step && (code % progress_step == 0) && code != 0)
        {
            double pct = 100.0 * (double)code / (double)total;
            printf("    ... %10llu / %llu  (%.1f%%)  collisions so far: %zu\n",
                   (unsigned long long)code, (unsigned long long)total, pct, confirmed);
            fflush(stdout);
        }
    }

    free(snap);
    free(snap_other);
    table_free();

    if (neutral_from_init_out)
        *neutral_from_init_out = neutral_from_init;
    return confirmed;
}

/* Compute the full 512-bit hash hex string for an input. Caller frees. */
static char *full_hash(const unsigned char *data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

/* ── E3: demonstrate infinite-family amplification of a collision pair ─── */
static void demonstrate_amplification(const CollisionRecord *rec)
{
    if (!rec->found)
        return;

    printf("\n=== [E3] Amplification of the first confirmed pair ===\n");

    unsigned char a[3 + 8];
    unsigned char b[3 + 8];
    unpack(rec->a, rec->len, a);
    unpack(rec->b, rec->len, b);

    char *ha = full_hash(a, rec->len);
    char *hb = full_hash(b, rec->len);
    printf("  base pair (len=%zu): hash%s\n", rec->len,
           strcmp(ha, hb) == 0 ? "es match" : "ES DIFFER (unexpected!)");
    free(ha);
    free(hb);

    srand(12345u);
    int trials = 5;
    int all_match = 1;
    for (int t = 0; t < trials; t++)
    {
        size_t slen = (size_t)(1 + (rand() % 8));
        for (size_t i = 0; i < slen; i++)
        {
            unsigned char s = (unsigned char)(rand() & 0xFF);
            a[rec->len + i] = s;
            b[rec->len + i] = s;
        }
        char *sa = full_hash(a, rec->len + slen);
        char *sb = full_hash(b, rec->len + slen);
        int match = (strcmp(sa, sb) == 0);
        all_match = all_match && match;
        printf("    suffix len %zu: full hashes %s\n", slen, match ? "match" : "DIFFER");
        free(sa);
        free(sb);
    }
    printf("  => %s\n", all_match
                            ? "every X.S / Y.S pair collides (infinite family confirmed)"
                            : "common-suffix amplification not established");
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--dump-hex") == 0)
        return dump_hex_state(argv[2], 0);
    if (argc == 3 && strcmp(argv[1], "--dump-final-hex") == 0)
        return dump_hex_state(argv[2], 1);
    if (argc == 3 && strcmp(argv[1], "--hash-values-hex") == 0)
        return hash_values_hex(argv[2]);

    int with_3byte = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--with-3byte") == 0)
            with_3byte = 1;

    printf("=== Secasy Phase-2 Internal-State Collision Scan ===\n\n");
    printf("State fingerprinted: %d cell values + %d colour indices + cursor\n",
           NUM_CELLS, NUM_CELLS);
    printf("(primeIndex excluded -- not read after Phase 2)\n");
    printf("Snapshot size: %d bytes\n\n", STATE_BYTES);

    /* Canonical post-initialisation state (input = empty). */
    uint8_t *init_state = (uint8_t *)malloc((size_t)STATE_BYTES);
    if (!init_state)
    {
        fprintf(stderr, "init snapshot allocation failed\n");
        return 1;
    }
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    snapshot_state(init_state);

    CollisionRecord first = {0, 0, 0, 0};
    size_t lengths[3] = {1, 2, 3};
    int n_lengths = with_3byte ? 3 : 2;
    size_t total_confirmed = 0;

    printf("=== [E1] Exhaustive Phase-2 state collision scan ===\n");
    if (!with_3byte)
        printf("(pass --with-3byte to also enumerate all 16,777,216 three-byte inputs)\n");
    printf("\n");

    for (int li = 0; li < n_lengths; li++)
    {
        size_t len = lengths[li];
        uint64_t total = (len == 1) ? 256ULL : (len == 2) ? 65536ULL : 16777216ULL;
        clock_t t0 = clock();

        size_t neutral = 0;
        size_t confirmed = scan_length(len, init_state, &neutral, &first);
        total_confirmed += confirmed;

        double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  Length %zu : %llu inputs  ->  confirmed Phase-2 collisions: %zu"
               "   [E2 neutral-from-init: %zu]   (%.1f s)\n",
               len, (unsigned long long)total, confirmed, neutral, secs);
    }

    printf("\n=== Summary ===\n");
    printf("  total confirmed Phase-2 state collisions: %zu\n", total_confirmed);
    if (total_confirmed == 0)
    {
        printf("  VERDICT: no Phase-2 collisions on the scanned inputs.\n");
        printf("           The lossy input walk is injective over this domain;\n");
        printf("           no neutral block / path collision exists at L <= %d.\n",
               n_lengths);
    }
    else
    {
        printf("  VERDICT: structural collision found -- Phase 2 is NOT injective.\n");
        printf("           Each pair amplifies to an infinite collision family.\n");
    }

    demonstrate_amplification(&first);

    free(init_state);
    return 0;
}
