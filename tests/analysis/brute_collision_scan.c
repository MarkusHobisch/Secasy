/*
 * brute_collision_scan.c
 *
 * Systematic brute-force collision scan on short inputs (1, 2, 3 bytes).
 * For each length L we enumerate ALL 256^L possible byte sequences,
 * compute the 64-bit Secasy hash, and count exact collisions plus
 * truncated-output collisions (32 / 48 bit) for context.
 *
 * Reference expectations (ideal hash, no structural bias):
 *   - L=1: 256 inputs   -> ~0 collisions in 64-bit and 32-bit space
 *   - L=2: 65,536       -> ~0 collisions in 64-bit, ~0.5 in 32-bit
 *   - L=3: 16,777,216   -> ~0.02 in 64-bit, ~32,768 in 32-bit (birthday)
 *
 * Any observed collision count substantially higher than the ideal
 * expectation indicates structural weakness.
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

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

/* Parse the first 16 hex chars of the Secasy hash string into a uint64_t. */
static uint64_t parse_first_block(const char *hex)
{
    uint64_t v = 0;
    for (int i = 0; i < 16; i++)
    {
        char c = hex[i];
        int d = (c >= '0' && c <= '9') ? c - '0'
              : (c >= 'a' && c <= 'f') ? c - 'a' + 10
              : (c >= 'A' && c <= 'F') ? c - 'A' + 10
              : 0;
        v = (v << 4) | (uint64_t)d;
    }
    return v;
}

static uint64_t hash64(const unsigned char *data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    char *h = calculateHashValue();
    uint64_t v = parse_first_block(h);
    free(h);
    return v;
}

/* ---------------------- Hash table for collision detection -------------- */
typedef struct Entry
{
    uint64_t key;
    uint32_t input; /* packed: byte0 | byte1<<8 | byte2<<16, length implicit */
    struct Entry *next;
} Entry;

#define TABLE_BITS 25                  /* 33,554,432 buckets -> ~256 MB for L=3 */
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

/* Insert and return colliding input if key already present, else (uint32_t)-1 */
static uint32_t table_insert_or_find(uint64_t key, uint32_t input, uint64_t mask)
{
    uint64_t masked = key & mask;
    uint32_t bucket = (uint32_t)((masked ^ (masked >> 25) ^ (masked >> 50)) & TABLE_MASK);
    for (Entry *e = table[bucket]; e; e = e->next)
    {
        if ((e->key & mask) == masked)
            return e->input;
    }
    Entry *ne = &pool[pool_used++];
    ne->key = key;
    ne->input = input;
    ne->next = table[bucket];
    table[bucket] = ne;
    return (uint32_t)-1;
}

static void reset_buckets(void)
{
    memset(table, 0, TABLE_SIZE * sizeof(Entry *));
    pool_used = 0;
}

/* ----------------------------------- Scan ------------------------------- */
static void scan_length(int length, size_t total, int max_examples_to_print)
{
    printf("\n=== Length %d : %zu inputs, 64-bit output ===\n", length, total);
    reset_buckets();

    const uint64_t mask64 = ~(uint64_t)0;
    const uint64_t mask48 = ((uint64_t)1 << 48) - 1;
    const uint64_t mask32 = ((uint64_t)1 << 32) - 1;

    /* We scan once at 64 bit, separately at 48 and 32 bit (independent passes) */
    long collisions64 = 0;
    int  examples_printed = 0;
    time_t t0 = time(NULL);

    for (size_t i = 0; i < total; i++)
    {
        unsigned char buf[3];
        buf[0] = (unsigned char)(i & 0xFF);
        buf[1] = (unsigned char)((i >> 8) & 0xFF);
        buf[2] = (unsigned char)((i >> 16) & 0xFF);

        uint64_t h = hash64(buf, (size_t)length);

        uint32_t prev = table_insert_or_find(h, (uint32_t)i, mask64);
        if (prev != (uint32_t)-1)
        {
            collisions64++;
            if (examples_printed < max_examples_to_print)
            {
                unsigned char p[3];
                p[0] = (unsigned char)(prev & 0xFF);
                p[1] = (unsigned char)((prev >> 8) & 0xFF);
                p[2] = (unsigned char)((prev >> 16) & 0xFF);
                printf("  COLLISION  h64=%016llx\n", (unsigned long long)h);
                printf("     input A:");
                for (int k = 0; k < length; k++) printf(" %02X", p[k]);
                printf("\n     input B:");
                for (int k = 0; k < length; k++) printf(" %02X", buf[k]);
                printf("\n");
                examples_printed++;
            }
        }

        if ((i & ((1u << 20) - 1)) == 0 && i > 0)
        {
            printf("  ... %zu / %zu  (%.1f%%)  collisions so far: %ld\n",
                   i, total, 100.0 * (double)i / (double)total, collisions64);
            fflush(stdout);
        }
    }
    time_t t1 = time(NULL);

    /* Truncated 48 / 32 bit scans */
    reset_buckets();
    long collisions48 = 0;
    for (size_t i = 0; i < total; i++)
    {
        unsigned char buf[3];
        buf[0] = (unsigned char)(i & 0xFF);
        buf[1] = (unsigned char)((i >> 8) & 0xFF);
        buf[2] = (unsigned char)((i >> 16) & 0xFF);
        uint64_t h = hash64(buf, (size_t)length);
        if (table_insert_or_find(h, (uint32_t)i, mask48) != (uint32_t)-1)
            collisions48++;
    }

    reset_buckets();
    long collisions32 = 0;
    for (size_t i = 0; i < total; i++)
    {
        unsigned char buf[3];
        buf[0] = (unsigned char)(i & 0xFF);
        buf[1] = (unsigned char)((i >> 8) & 0xFF);
        buf[2] = (unsigned char)((i >> 16) & 0xFF);
        uint64_t h = hash64(buf, (size_t)length);
        if (table_insert_or_find(h, (uint32_t)i, mask32) != (uint32_t)-1)
            collisions32++;
    }

    double n = (double)total;
    double exp64 = (n * (n - 1.0)) / (2.0 * 18446744073709551616.0);
    double exp48 = (n * (n - 1.0)) / (2.0 * 281474976710656.0);
    double exp32 = (n * (n - 1.0)) / (2.0 * 4294967296.0);

    printf("\n  Length %d results (%lld s):\n", length, (long long)(t1 - t0));
    printf("    64-bit collisions: %8ld  (expected for ideal hash: %.6f)\n",
           collisions64, exp64);
    printf("    48-bit collisions: %8ld  (expected for ideal hash: %.3f)\n",
           collisions48, exp48);
    printf("    32-bit collisions: %8ld  (expected for ideal hash: %.1f)\n",
           collisions32, exp32);
}

int main(int argc, char **argv)
{
    int do_l3 = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--with-3byte") == 0) do_l3 = 1;

    printf("Secasy brute-force collision scan\n");
    printf("Rounds=%lu  HashBits=%d  MaxPrimeIndex=%d\n",
           numberOfRounds, hashLengthInBits, DEFAULT_MAX_PRIME_INDEX);

    /* Length 1 */
    table_alloc(256);
    scan_length(1, 256, 5);
    table_free();

    /* Length 2 */
    table_alloc(65536);
    scan_length(2, 65536, 5);
    table_free();

    if (do_l3)
    {
        table_alloc(16777216);
        scan_length(3, 16777216, 5);
        table_free();
    }
    else
    {
        printf("\n(Skipping length 3 -- pass --with-3byte to enable, ~10-30 min.)\n");
    }

    return 0;
}
