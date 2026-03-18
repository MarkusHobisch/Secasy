/*
 * debug_collision.c — Diagnose 0%-Hamming (exact collision) cases
 *
 * Runs Baseline mode (full mix, no forced color) and logs every case
 * where flipping a single input bit produces the same 512-bit hash.
 * For each collision found:
 *   - Prints full original + flipped messages (hex)
 *   - Prints both full 128-char hashes
 *   - Re-hashes both messages independently as a cross-check
 *   - Compares grid states after Phase 2 (before Phase 3)
 *
 * BUILD: add_secasy_test(SecasyDebugCollision tests/analysis/debug_collision.c)
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

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

/* ── xorshift64 RNG ──────────────────────────────────────────────── */
static uint64_t rng_state = 0;
static uint64_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* ── Hamming distance between hex strings ────────────────────────── */
static int hamming_hex(const char *a, const char *b)
{
    static const int lut[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
    int dist = 0;
    for (size_t i = 0; a[i] && b[i]; i++)
    {
        int va = (a[i] >= 'a') ? a[i] - 'a' + 10 : (a[i] >= 'A') ? a[i] - 'A' + 10
                                                                 : a[i] - '0';
        int vb = (b[i] >= 'a') ? b[i] - 'a' + 10 : (b[i] >= 'A') ? b[i] - 'A' + 10
                                                                 : b[i] - '0';
        dist += lut[va ^ vb];
    }
    return dist;
}

/* ── Print message as hex ────────────────────────────────────────── */
static void print_msg(const char *label, const unsigned char *msg, int len)
{
    printf("  %s: ", label);
    for (int i = 0; i < len; i++)
        printf("%02x", msg[i]);
    printf("\n");
}

/* ── Snapshot grid state after Phase 2 ───────────────────────────── */
typedef struct
{
    uint64_t values[FIELD_SIZE][FIELD_SIZE];
    ColorIndex_t colors[FIELD_SIZE][FIELD_SIZE];
    uint32_t px, py;
} GridSnap;

static void snap_grid(GridSnap *s)
{
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            s->values[i][j] = field[i][j].value;
            s->colors[i][j] = field[i][j].colorIndex;
        }
    s->px = pos.x;
    s->py = pos.y;
}

static int compare_grids(const GridSnap *a, const GridSnap *b)
{
    int diffs = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            if (a->values[i][j] != b->values[i][j])
                diffs++;
            if (a->colors[i][j] != b->colors[i][j])
                diffs++;
        }
    if (a->px != b->px || a->py != b->py)
        diffs++;
    return diffs;
}

static void print_grid_diff(const GridSnap *a, const GridSnap *b)
{
    printf("  Cursor: orig=(%u,%u) flip=(%u,%u) %s\n",
           a->px, a->py, b->px, b->py,
           (a->px == b->px && a->py == b->py) ? "SAME" : "DIFF");
    int shown = 0;
    for (int i = 0; i < FIELD_SIZE && shown < 20; i++)
        for (int j = 0; j < FIELD_SIZE && shown < 20; j++)
        {
            if (a->values[i][j] != b->values[i][j] ||
                a->colors[i][j] != b->colors[i][j])
            {
                printf("  [%2d][%2d] val: %016" PRIx64 " vs %016" PRIx64
                       "  col: %d vs %d\n",
                       i, j, a->values[i][j], b->values[i][j],
                       (int)a->colors[i][j], (int)b->colors[i][j]);
                shown++;
            }
        }
}

/* ── Hash with grid snapshot after Phase 2 ───────────────────────── */
static char *hash_and_snap(const unsigned char *data, size_t len, GridSnap *snap)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    if (snap)
        snap_grid(snap);
    return calculateHashValue();
}

#define N_MESSAGES 200
#define MSG_LEN 32

int main(void)
{
    rng_state = (uint64_t)time(NULL) ^ 0xdeadbeef01234567ULL;
    if (rng_state == 0)
        rng_state = 0xabcdef1234567890ULL;

    printf("=== Baseline Collision Diagnosis ===\n");
    printf("Messages: %d x %d bytes = %d bits each\n",
           N_MESSAGES, MSG_LEN, MSG_LEN * 8);
    printf("Total trials: %d\n\n", N_MESSAGES * MSG_LEN * 8);

    int collision_count = 0;

    for (int m = 0; m < N_MESSAGES; m++)
    {
        unsigned char msg[MSG_LEN];
        for (int b = 0; b < MSG_LEN; b++)
            msg[b] = (unsigned char)(rng_next() & 0xFF);

        GridSnap snap_orig;
        char *h0 = hash_and_snap(msg, MSG_LEN, &snap_orig);
        char *h0c = strdup(h0);
        free(h0);

        for (int bi = 0; bi < MSG_LEN; bi++)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                unsigned char flipped[MSG_LEN];
                memcpy(flipped, msg, MSG_LEN);
                flipped[bi] ^= (unsigned char)(1u << bit);

                GridSnap snap_flip;
                char *h1 = hash_and_snap(flipped, MSG_LEN, &snap_flip);

                int dist = hamming_hex(h0c, h1);

                if (dist == 0)
                {
                    collision_count++;
                    printf("======== COLLISION #%d ========\n", collision_count);
                    printf("  Message #%d, byte %d, bit %d\n", m, bi, bit);
                    print_msg("Original", msg, MSG_LEN);
                    print_msg("Flipped ", flipped, MSG_LEN);
                    printf("  Diff: byte[%d] = 0x%02X -> 0x%02X (bit %d flipped)\n",
                           bi, msg[bi], flipped[bi], bit);
                    printf("  Hash orig: %s\n", h0c);
                    printf("  Hash flip: %s\n", h1);

                    /* Compare grid states after Phase 2 */
                    int grid_diffs = compare_grids(&snap_orig, &snap_flip);
                    printf("  Grid diffs after Phase 2: %d\n", grid_diffs);
                    if (grid_diffs > 0 && grid_diffs <= 20)
                        print_grid_diff(&snap_orig, &snap_flip);
                    else if (grid_diffs == 0)
                        printf("  *** GRIDS IDENTICAL AFTER PHASE 2 ***\n");

                    /* Cross-check: re-hash both independently */
                    char *verify_orig = hash_and_snap(msg, MSG_LEN, NULL);
                    char *verify_flip = hash_and_snap(flipped, MSG_LEN, NULL);
                    int verify_dist = hamming_hex(verify_orig, verify_flip);
                    printf("  Cross-check re-hash: hamming = %d bits (%.1f%%)\n",
                           verify_dist, (double)verify_dist * 100.0 / 512.0);
                    printf("  Verify orig: %s\n", verify_orig);
                    printf("  Verify flip: %s\n", verify_flip);
                    if (verify_dist == 0)
                        printf("  *** CONFIRMED: Real collision, not state leak ***\n");
                    else
                        printf("  *** NOT CONFIRMED: State leak suspected ***\n");
                    free(verify_orig);
                    free(verify_flip);
                    printf("\n");
                }
                free(h1);
            }
        }
        free(h0c);
    }

    printf("=== Done. Total collisions found: %d / %d trials ===\n",
           collision_count, N_MESSAGES * MSG_LEN * 8);
    return 0;
}
