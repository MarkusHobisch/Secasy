/**
 * Analyze known 2-byte collision: 0x07,0x33 vs 0x0d,0x63
 * Compare field states, positions, visit patterns, and hash outputs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;
extern int lastPrime;

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

typedef struct
{
    uint64_t values[FIELD_SIZE][FIELD_SIZE];
    uint32_t primeIdx[FIELD_SIZE][FIELD_SIZE];
    uint32_t colorIdx[FIELD_SIZE][FIELD_SIZE];
    uint32_t px, py;
    int lastPrimeVal;
} Snapshot;

static void take_snapshot(Snapshot *s)
{
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            s->values[i][j] = field[i][j].value;
            s->primeIdx[i][j] = field[i][j].primeIndex;
            s->colorIdx[i][j] = (uint32_t)field[i][j].colorIndex;
        }
    s->px = pos.x;
    s->py = pos.y;
    s->lastPrimeVal = lastPrime;
}

static int snapshots_equal(const Snapshot *a, const Snapshot *b)
{
    if (a->px != b->px || a->py != b->py)
        return 0;
    if (a->lastPrimeVal != b->lastPrimeVal)
        return 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            if (a->values[i][j] != b->values[i][j])
                return 0;
            if (a->primeIdx[i][j] != b->primeIdx[i][j])
                return 0;
            if (a->colorIdx[i][j] != b->colorIdx[i][j])
                return 0;
        }
    return 1;
}

static void print_diff(const Snapshot *a, const Snapshot *b, const char *label)
{
    printf("\n--- %s ---\n", label);
    printf("  Position A: (%u,%u)  Position B: (%u,%u)  %s\n",
           a->px, a->py, b->px, b->py,
           (a->px == b->px && a->py == b->py) ? "SAME" : "DIFFER");
    printf("  lastPrime A: %d  B: %d  %s\n",
           a->lastPrimeVal, b->lastPrimeVal,
           a->lastPrimeVal == b->lastPrimeVal ? "SAME" : "DIFFER");

    int diff_cells = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (a->values[i][j] != b->values[i][j] ||
                a->primeIdx[i][j] != b->primeIdx[i][j] ||
                a->colorIdx[i][j] != b->colorIdx[i][j])
                diff_cells++;

    printf("  Differing cells: %d / %d\n", diff_cells, FIELD_SIZE * FIELD_SIZE);

    if (diff_cells > 0 && diff_cells <= 20)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
            for (int j = 0; j < FIELD_SIZE; j++)
            {
                int vd = (a->values[i][j] != b->values[i][j]);
                int pd = (a->primeIdx[i][j] != b->primeIdx[i][j]);
                int cd = (a->colorIdx[i][j] != b->colorIdx[i][j]);
                if (vd || pd || cd)
                {
                    printf("    [%2d,%2d]: val=%" PRIu64 " vs %" PRIu64,
                           i, j, a->values[i][j], b->values[i][j]);
                    if (pd)
                        printf("  pIdx=%u vs %u", a->primeIdx[i][j], b->primeIdx[i][j]);
                    if (cd)
                        printf("  cIdx=%u vs %u", a->colorIdx[i][j], b->colorIdx[i][j]);
                    printf("\n");
                }
            }
    }
}

static void print_directions(const unsigned char *data, size_t len, const char *label)
{
    printf("\n  Direction sequence for %s:\n", label);
    for (size_t i = 0; i < len; i++)
    {
        int b = data[i];
        static const char *dn[] = {"UP   ", "RIGHT", "LEFT ", "DOWN "};
        printf("    Byte %zu (0x%02X): %s %s %s %s\n",
               i, b, dn[b & 3], dn[(b >> 2) & 3], dn[(b >> 4) & 3], dn[(b >> 6) & 3]);
    }
}

int main(void)
{
    unsigned char input1[] = {0x07, 0x33};
    unsigned char input2[] = {0x0d, 0x63};

    printf("=== Analyzing known collision: {0x07,0x33} vs {0x0d,0x63} ===\n");
    printf("Rounds: %lu, Hash bits: %d\n\n", numberOfRounds, hashLengthInBits);

    /* Show direction sequences */
    print_directions(input1, 2, "{0x07,0x33}");
    print_directions(input2, 2, "{0x0d,0x63}");

    /* --- After Init Phase (processBuffer = Phase 2) --- */
    printf("\n========== PHASE 2 (Init Phase / processBuffer) ==========\n");

    Snapshot snap1_p2, snap2_p2;

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input1, 2);
    take_snapshot(&snap1_p2);
    char *hash1 = calculateHashValue();

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input2, 2);
    take_snapshot(&snap2_p2);
    char *hash2 = calculateHashValue();

    print_diff(&snap1_p2, &snap2_p2, "After Phase 2 (processBuffer)");

    printf("\n  Hash A: %s\n", hash1);
    printf("  Hash B: %s\n", hash2);
    printf("  Collision: %s\n", strcmp(hash1, hash2) == 0 ? "YES" : "NO");

    if (snapshots_equal(&snap1_p2, &snap2_p2))
    {
        printf("\n  >>> CONFIRMED: Field state + position are IDENTICAL after Phase 2.\n");
        printf("  >>> This means the Processing Phase (Phase 3) starts from the\n");
        printf("  >>> exact same state, producing the same hash.\n");
        printf("  >>> ROOT CAUSE: Different byte-walk paths converge to the same\n");
        printf("  >>> field state (same multiset of cell visits).\n");
    }

    /* --- Check byte-by-byte --- */
    printf("\n========== STEP-BY-STEP TRACE ==========\n");

    /* After byte 0 only */
    Snapshot snap1_b0, snap2_b0;

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input1, 1);
    take_snapshot(&snap1_b0);

    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input2, 1);
    take_snapshot(&snap2_b0);

    print_diff(&snap1_b0, &snap2_b0, "After byte 0 only");

    printf("\n  Input1 byte0=0x%02X dirs: %d %d %d %d\n",
           input1[0], input1[0] & 3, (input1[0] >> 2) & 3, (input1[0] >> 4) & 3, (input1[0] >> 6) & 3);
    printf("  Input2 byte0=0x%02X dirs: %d %d %d %d\n",
           input2[0], input2[0] & 3, (input2[0] >> 2) & 3, (input2[0] >> 4) & 3, (input2[0] >> 6) & 3);

    free(hash1);
    free(hash2);
    return 0;
}
