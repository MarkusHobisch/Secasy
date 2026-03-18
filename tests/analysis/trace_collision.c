/*
 * trace_collision.c — Systematic analysis of direction collisions
 *
 * Part 1: Enumerate all (px, py, oldPrime) where two different
 *         directions land on the same cell (structural weakness).
 *
 * Part 2: Run actual collision search with step-by-step trace
 *         of the diverging byte to confirm the mechanism.
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
#include "primes.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

static const char *dir_name(int d)
{
    static const char *names[] = {"UP", "RIGHT", "LEFT", "DOWN"};
    return (d >= 0 && d <= 3) ? names[d] : "???";
}

/*
 * Compute destination (nx,ny) for a given move from (px,py) with
 * oldPrime value op.  Pure function — no side effects.
 */
static void move_dest(int move, uint32_t px, uint32_t py, uint64_t op,
                      uint32_t *nx, uint32_t *ny)
{
    uint32_t mask = FIELD_SIZE - 1;
    switch (move)
    {
    case 0: /* UP */
        *ny = (uint32_t)((py - op + 1u) & mask);
        *nx = (px + (*ny >> 1) + 1u) & mask;
        break;
    case 1: /* RIGHT */
        *nx = (uint32_t)((px + op + 1u) & mask);
        *ny = (py + (*nx >> 1) + 4u) & mask;
        break;
    case 2: /* LEFT */
        *nx = (uint32_t)((px - op) & mask);
        *ny = (py + (*nx >> 1) + 2u) & mask;
        break;
    case 3: /* DOWN */
        *ny = (uint32_t)((py + op) & mask);
        *nx = (px + (*ny >> 1) + 3u) & mask;
        break;
    }
}

/* ── Part 1: enumerate direction collision cases ──────────────── */
static void enumerate_direction_collisions(void)
{
    printf("=== Part 1: Direction collision enumeration ===\n");
    printf("FIELD_SIZE = %d\n", FIELD_SIZE);
    printf("Checking all (px, py, op_mod16) x all direction pairs...\n\n");

    /* Count collisions per direction pair */
    int pair_counts[4][4] = {{0}};
    int total_collisions = 0;
    int total_states = FIELD_SIZE * FIELD_SIZE * FIELD_SIZE; /* 4096 */

    for (uint32_t px = 0; px < FIELD_SIZE; px++)
    {
        for (uint32_t py = 0; py < FIELD_SIZE; py++)
        {
            for (uint64_t op = 0; op < FIELD_SIZE; op++)
            {
                uint32_t dests[4][2]; /* [dir][0=x, 1=y] */
                for (int d = 0; d < 4; d++)
                    move_dest(d, px, py, op, &dests[d][0], &dests[d][1]);

                for (int d1 = 0; d1 < 4; d1++)
                {
                    for (int d2 = d1 + 1; d2 < 4; d2++)
                    {
                        if (dests[d1][0] == dests[d2][0] &&
                            dests[d1][1] == dests[d2][1])
                        {
                            pair_counts[d1][d2]++;
                            total_collisions++;

                            /* Print first 5 examples per pair */
                            if (pair_counts[d1][d2] <= 5)
                            {
                                printf("  (%s,%s) at px=%u py=%u op=%" PRIu64
                                       " -> dest=(%u,%u)\n",
                                       dir_name(d1), dir_name(d2),
                                       px, py, op,
                                       dests[d1][0], dests[d1][1]);
                            }
                        }
                    }
                }
            }
        }
    }

    printf("\n--- Direction collision summary ---\n");
    printf("%-15s  Count    Fraction\n", "Pair");
    for (int d1 = 0; d1 < 4; d1++)
        for (int d2 = d1 + 1; d2 < 4; d2++)
            if (pair_counts[d1][d2] > 0)
                printf("%-5s <-> %-5s  %5d    %5.2f%%\n",
                       dir_name(d1), dir_name(d2),
                       pair_counts[d1][d2],
                       100.0 * pair_counts[d1][d2] / total_states);

    printf("\nTotal direction-collision states: %d / %d (%.2f%%)\n",
           total_collisions, total_states * 6,
           100.0 * total_collisions / (total_states * 6.0));
    printf("(6 = number of direction pairs, C(4,2))\n\n");
}

/* ── Part 2: categorized collision search ─────────────────────── */

static uint64_t rng_state = 0;
static uint64_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

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

typedef struct
{
    uint64_t values[FIELD_SIZE][FIELD_SIZE];
    uint32_t primeIdx[FIELD_SIZE][FIELD_SIZE];
    uint32_t colorIdx[FIELD_SIZE][FIELD_SIZE];
    uint32_t px, py;
} GridSnap;

static void snap_grid(GridSnap *s)
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
}

static int grids_identical(const GridSnap *a, const GridSnap *b)
{
    if (a->px != b->px || a->py != b->py)
        return 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            if (a->values[i][j] != b->values[i][j] ||
                a->primeIdx[i][j] != b->primeIdx[i][j] ||
                a->colorIdx[i][j] != b->colorIdx[i][j])
                return 0;
    return 1;
}

#define MSG_LEN 32
#define N_MSGS 5000

static void categorized_collision_search(void)
{
    printf("=== Part 2: Categorized collision search (%d msgs) ===\n", N_MSGS);
    rng_state = (uint64_t)time(NULL) ^ 0xdeadbeef01234567ULL;
    if (rng_state == 0)
        rng_state = 0xabcdef1234567890ULL;

    int total_hash_coll = 0;
    int phase2_identical = 0;
    int phase2_dir_coll = 0;
    int phase2_different = 0;
    int total_trials = 0;

    for (int m = 0; m < N_MSGS; m++)
    {
        unsigned char msg[MSG_LEN];
        for (int b = 0; b < MSG_LEN; b++)
            msg[b] = (unsigned char)(rng_next() & 0xFF);

        /* Hash original + snapshot */
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, MSG_LEN);
        GridSnap snap_orig;
        snap_grid(&snap_orig);
        char *h0 = calculateHashValue();
        char *h0c = strdup(h0);
        free(h0);

        for (int bi = 0; bi < MSG_LEN; bi++)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                total_trials++;
                unsigned char flipped[MSG_LEN];
                memcpy(flipped, msg, MSG_LEN);
                flipped[bi] ^= (unsigned char)(1u << bit);

                /* Hash flipped + snapshot */
                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                processBuffer(flipped, MSG_LEN);
                GridSnap snap_flip;
                snap_grid(&snap_flip);
                char *h1 = calculateHashValue();

                if (hamming_hex(h0c, h1) == 0)
                {
                    total_hash_coll++;

                    int orig_dirs[4] = {
                        msg[bi] & 3, (msg[bi] >> 2) & 3, (msg[bi] >> 4) & 3, (msg[bi] >> 6) & 3};
                    int flip_dirs[4] = {
                        flipped[bi] & 3, (flipped[bi] >> 2) & 3, (flipped[bi] >> 4) & 3, (flipped[bi] >> 6) & 3};
                    int diff_sub = -1;
                    for (int s = 0; s < 4; s++)
                        if (orig_dirs[s] != flip_dirs[s])
                        {
                            diff_sub = s;
                            break;
                        }

                    if (grids_identical(&snap_orig, &snap_flip))
                    {
                        phase2_identical++;

                        /* Check: is this a direction collision? */
                        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                        processBuffer(msg, (size_t)bi);
                        /* Advance to the diff sub-step */
                        for (int s = 0; s < diff_sub; s++)
                        {
                            Tile_t *tile = &field[pos.x][pos.y];
                            uint64_t op = tile->value;
                            int pi = (int)tile->primeIndex + 1;
                            if (pi >= NUMBER_OF_PRIMES)
                                pi = 0;
                            tile->primeIndex = (uint32_t)pi;
                            tile->colorIndex = (ColorIndex_t)(((unsigned int)tile->colorIndex + 1u) % 6u);
                            tile->value = (uint64_t)storedPrimesArray[pi];
                            uint32_t mask = FIELD_SIZE - 1;
                            int d = (msg[bi] >> (s * 2)) & 3;
                            switch (d)
                            {
                            case 0:
                                pos.y = (uint32_t)((pos.y - op + 1u) & mask);
                                pos.x = (pos.x + (pos.y >> 1) + 1u) & mask;
                                break;
                            case 1:
                                pos.x = (uint32_t)((pos.x + op + 1u) & mask);
                                pos.y = (pos.y + (pos.x >> 1) + 4u) & mask;
                                break;
                            case 2:
                                pos.x = (uint32_t)((pos.x - op) & mask);
                                pos.y = (pos.y + (pos.x >> 1) + 2u) & mask;
                                break;
                            case 3:
                                pos.y = (uint32_t)((pos.y + op) & mask);
                                pos.x = (pos.x + (pos.y >> 1) + 3u) & mask;
                                break;
                            }
                        }
                        uint64_t op_at = field[pos.x][pos.y].value;
                        uint32_t npx = pos.x, npy = pos.y;
                        uint32_t nx1, ny1, nx2, ny2;
                        move_dest(orig_dirs[diff_sub], npx, npy, op_at, &nx1, &ny1);
                        move_dest(flip_dirs[diff_sub], npx, npy, op_at, &nx2, &ny2);

                        int is_dir_coll = (nx1 == nx2 && ny1 == ny2);
                        if (is_dir_coll)
                            phase2_dir_coll++;

                        if (total_hash_coll <= 8)
                        {
                            printf("  [P2-IDENT #%d] msg#%d byte[%d] bit%d: "
                                   "0x%02X->0x%02X  sub%d: %s->%s  "
                                   "pos=(%u,%u) op=%" PRIu64 "  %s->(%u,%u) %s->(%u,%u) %s\n",
                                   phase2_identical, m, bi, bit,
                                   msg[bi], flipped[bi], diff_sub,
                                   dir_name(orig_dirs[diff_sub]),
                                   dir_name(flip_dirs[diff_sub]),
                                   npx, npy, op_at,
                                   dir_name(orig_dirs[diff_sub]), nx1, ny1,
                                   dir_name(flip_dirs[diff_sub]), nx2, ny2,
                                   is_dir_coll ? "*** DIR COLLISION ***"
                                               : "not dir-coll (later convergence)");
                        }
                    }
                    else
                    {
                        phase2_different++;
                        if (total_hash_coll <= 8)
                        {
                            printf("  [P3-CONV  #%d] msg#%d byte[%d] bit%d: "
                                   "0x%02X->0x%02X  sub%d: %s->%s  "
                                   "(grids differ, mixing converges)\n",
                                   phase2_different, m, bi, bit,
                                   msg[bi], flipped[bi], diff_sub,
                                   dir_name(orig_dirs[diff_sub]),
                                   dir_name(flip_dirs[diff_sub]));
                        }
                    }
                }
                free(h1);
            }
        }
        free(h0c);
    }

    printf("\n--- Collision Summary ---\n");
    printf("Total trials:                 %d\n", total_trials);
    printf("Total hash collisions:        %d  (%.4f%%)\n",
           total_hash_coll, 100.0 * total_hash_coll / total_trials);
    printf("  Phase-2 grid identical:     %d\n", phase2_identical);
    printf("    of which dir-collision:   %d\n", phase2_dir_coll);
    printf("    later convergence:        %d\n", phase2_identical - phase2_dir_coll);
    printf("  Phase-3 mixing convergence: %d\n", phase2_different);
    printf("\n");
}

/* ── Part 3: verify multiset hypothesis ──────────────────────── */
/*
 * Hypothesis: two different cursor walks produce identical grids if and
 * only if they visit the same MULTISET of cells (each cell visited the
 * same number of times).
 *
 * Reason: nextPrimeNumber(tile) advances tile->primeIndex by 1 each
 * call, cycling through storedPrimesArray.  The final tile state
 * depends ONLY on how many times it was visited, not when.
 *
 * Verify: replay Phase 2 for both ORIG and FLIP, count visits per cell,
 * check if visit-count matrices match for all Phase-2-identical cases.
 */
static void verify_multiset_hypothesis(void)
{
    printf("=== Part 3: Multiset visit-count verification ===\n");
    /* Re-seed to get the same messages as Part 2 */
    rng_state = (uint64_t)time(NULL) ^ 0xdeadbeef01234567ULL;
    if (rng_state == 0)
        rng_state = 0xabcdef1234567890ULL;

    int p2_identical_count = 0;
    int multiset_match = 0;
    int multiset_mismatch = 0;

    for (int m = 0; m < N_MSGS; m++)
    {
        unsigned char msg[MSG_LEN];
        for (int b = 0; b < MSG_LEN; b++)
            msg[b] = (unsigned char)(rng_next() & 0xFF);

        /* Hash original + snapshot */
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, MSG_LEN);
        GridSnap snap_orig;
        snap_grid(&snap_orig);
        char *h0 = calculateHashValue();
        char *h0c = strdup(h0);
        free(h0);

        for (int bi = 0; bi < MSG_LEN; bi++)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                unsigned char flipped[MSG_LEN];
                memcpy(flipped, msg, MSG_LEN);
                flipped[bi] ^= (unsigned char)(1u << bit);

                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                processBuffer(flipped, MSG_LEN);
                GridSnap snap_flip;
                snap_grid(&snap_flip);
                char *h1 = calculateHashValue();

                if (hamming_hex(h0c, h1) == 0 &&
                    grids_identical(&snap_orig, &snap_flip))
                {
                    p2_identical_count++;

                    /* Compare visit counts via primeIndex.
                     * After Phase 2, tile.primeIndex = (initial + visits) mod N.
                     * Initial primeIndex is 0, so primeIndex ≈ visits mod N.
                     * Since N >> typical visits, primeIndex IS the visit count.
                     */
                    int visit_match = 1;
                    for (int i = 0; i < FIELD_SIZE && visit_match; i++)
                        for (int j = 0; j < FIELD_SIZE && visit_match; j++)
                            if (snap_orig.primeIdx[i][j] != snap_flip.primeIdx[i][j])
                                visit_match = 0;

                    if (visit_match)
                    {
                        multiset_match++;
                    }
                    else
                    {
                        multiset_mismatch++;
                        if (multiset_mismatch <= 3)
                        {
                            printf("  MISMATCH: msg#%d byte[%d] bit%d\n", m, bi, bit);
                            /* Show first few differing cells */
                            int shown = 0;
                            for (int i = 0; i < FIELD_SIZE && shown < 5; i++)
                                for (int j = 0; j < FIELD_SIZE && shown < 5; j++)
                                    if (snap_orig.primeIdx[i][j] !=
                                        snap_flip.primeIdx[i][j])
                                    {
                                        printf("    cell[%d][%d]: visits %u vs %u\n",
                                               i, j,
                                               snap_orig.primeIdx[i][j],
                                               snap_flip.primeIdx[i][j]);
                                        shown++;
                                    }
                        }
                    }
                }
                free(h1);
            }
        }
        free(h0c);
    }

    printf("Phase-2 identical cases checked: %d\n", p2_identical_count);
    printf("  Visit-count multiset MATCHES: %d\n", multiset_match);
    printf("  Visit-count multiset MISMATCHES: %d\n", multiset_mismatch);
    if (p2_identical_count > 0 && multiset_mismatch == 0)
        printf("  *** HYPOTHESIS CONFIRMED: identical grids <=> same visit multiset ***\n");
    else if (multiset_mismatch > 0)
        printf("  *** HYPOTHESIS PARTIALLY REFUTED: %d cases with different visit counts ***\n",
               multiset_mismatch);
    printf("\n");
}

/* ── Part 4: collision distribution profiling ────────────────── */
static void collision_distribution(void)
{
    printf("=== Part 4: Collision distribution (10000 msgs) ===\n");
    uint64_t rng4 = 42ULL;
#define RNG4() (rng4 ^= rng4 << 13, rng4 ^= rng4 >> 7, rng4 ^= rng4 << 17, rng4)

    int by_byte[MSG_LEN] = {0};
    int by_sub[4] = {0};
    int by_dir_pair[4][4] = {{0}};
    int by_remaining[MSG_LEN + 1] = {0}; /* remaining bytes after diff */
    int total_coll = 0;
    int N4 = 10000;

    for (int m = 0; m < N4; m++)
    {
        unsigned char msg[MSG_LEN];
        for (int b = 0; b < MSG_LEN; b++)
            msg[b] = (unsigned char)(RNG4() & 0xFF);

        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, MSG_LEN);
        GridSnap snap_orig;
        snap_grid(&snap_orig);
        char *h0 = calculateHashValue();
        char *h0c = strdup(h0);
        free(h0);

        for (int bi = 0; bi < MSG_LEN; bi++)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                unsigned char flipped[MSG_LEN];
                memcpy(flipped, msg, MSG_LEN);
                flipped[bi] ^= (unsigned char)(1u << bit);

                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                processBuffer(flipped, MSG_LEN);
                GridSnap snap_flip;
                snap_grid(&snap_flip);
                char *h1 = calculateHashValue();

                if (hamming_hex(h0c, h1) == 0 &&
                    grids_identical(&snap_orig, &snap_flip))
                {
                    total_coll++;
                    by_byte[bi]++;
                    int sub = (bit < 2) ? 0 : (bit < 4) ? 1
                                          : (bit < 6)   ? 2
                                                        : 3;
                    by_sub[sub]++;
                    int d_orig = (msg[bi] >> (sub * 2)) & 3;
                    int d_flip = (flipped[bi] >> (sub * 2)) & 3;
                    by_dir_pair[d_orig][d_flip]++;
                    by_remaining[MSG_LEN - 1 - bi]++;
                }
                free(h1);
            }
        }
        free(h0c);
    }

    printf("Total collisions: %d / %d trials (%.4f%%)\n\n",
           total_coll, N4 * MSG_LEN * 8,
           100.0 * total_coll / (N4 * MSG_LEN * 8));

    printf("By byte position (remaining bytes to process):\n");
    for (int i = 0; i < MSG_LEN; i++)
        if (by_byte[i] > 0)
            printf("  byte[%2d] (%2d remaining): %3d\n", i, MSG_LEN - 1 - i, by_byte[i]);

    printf("\nBy sub-step (0-3 within byte):\n");
    for (int s = 0; s < 4; s++)
        printf("  sub-step %d: %d\n", s, by_sub[s]);

    printf("\nBy direction pair (orig -> flip):\n");
    for (int d1 = 0; d1 < 4; d1++)
        for (int d2 = 0; d2 < 4; d2++)
            if (d1 != d2 && by_dir_pair[d1][d2] > 0)
                printf("  %s -> %s: %d\n", dir_name(d1), dir_name(d2),
                       by_dir_pair[d1][d2]);

    printf("\nBy remaining bytes:\n");
    for (int r = 0; r <= MSG_LEN; r++)
        if (by_remaining[r] > 0)
            printf("  %2d remaining: %3d collisions\n", r, by_remaining[r]);
    printf("\n");
}

/* ── Part 5: full cursor path comparison for one collision ──── */
/*
 * Replay Phase 2 step by step, recording every cell visited.
 * Show exactly where paths diverge and how they reconverge
 * to the same visit multiset.
 */
static void replay_cursor_path(const unsigned char *data, size_t len,
                               uint32_t path[][2], int *path_len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    *path_len = 0;

    for (size_t i = 0; i < len; i++)
    {
        int byte = data[i] & 0xFF;
        int dirs[4] = {byte & 3, (byte >> 2) & 3, (byte >> 4) & 3, (byte >> 6) & 3};
        for (int s = 0; s < 4; s++)
        {
            /* Record current position (visited cell) */
            path[*path_len][0] = pos.x;
            path[*path_len][1] = pos.y;
            (*path_len)++;

            /* Replicate writeNextNumber */
            Tile_t *tile = &field[pos.x][pos.y];
            uint64_t oldPrime = tile->value;
            int pi = (int)tile->primeIndex + 1;
            if (pi >= NUMBER_OF_PRIMES)
                pi = 0;
            tile->primeIndex = (uint32_t)pi;
            tile->colorIndex = (ColorIndex_t)(((unsigned int)tile->colorIndex + 1u) % 6u);
            tile->value = (uint64_t)storedPrimesArray[pi];

            uint32_t mask = FIELD_SIZE - 1;
            switch (dirs[s])
            {
            case 0:
                pos.y = (uint32_t)((pos.y - oldPrime + 1u) & mask);
                pos.x = (pos.x + (pos.y >> 1) + 1u) & mask;
                break;
            case 1:
                pos.x = (uint32_t)((pos.x + oldPrime + 1u) & mask);
                pos.y = (pos.y + (pos.x >> 1) + 4u) & mask;
                break;
            case 2:
                pos.x = (uint32_t)((pos.x - oldPrime) & mask);
                pos.y = (pos.y + (pos.x >> 1) + 2u) & mask;
                break;
            case 3:
                pos.y = (uint32_t)((pos.y + oldPrime) & mask);
                pos.x = (pos.x + (pos.y >> 1) + 3u) & mask;
                break;
            }
        }
    }
    /* setPrimeNumberOfLastTile: one more visit to final cell */
    path[*path_len][0] = pos.x;
    path[*path_len][1] = pos.y;
    (*path_len)++;
    Tile_t *last = &field[pos.x][pos.y];
    int pi = (int)last->primeIndex + 1;
    if (pi >= NUMBER_OF_PRIMES)
        pi = 0;
    last->primeIndex = (uint32_t)pi;
    last->colorIndex = (ColorIndex_t)(((unsigned int)last->colorIndex + 1u) % 6u);
    last->value = (uint64_t)storedPrimesArray[pi];
}

static void trace_one_collision(void)
{
    printf("=== Part 5: Collision path analysis ===\n");
    uint64_t rng5 = 12345ULL;
#define RNG5() (rng5 ^= rng5 << 13, rng5 ^= rng5 >> 7, rng5 ^= rng5 << 17, rng5)

    int total_coll = 0;
    int zero_diverge = 0;    /* direction collision: 0 path differences */
    int nonzero_diverge = 0; /* multiset convergence: > 0 path differences */
    int first_nonzero_shown = 0;
    int first_zero_shown = 0;
    int N5 = 50000;

    for (int m = 0; m < N5; m++)
    {
        unsigned char msg[MSG_LEN];
        for (int b = 0; b < MSG_LEN; b++)
            msg[b] = (unsigned char)(RNG5() & 0xFF);

        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, MSG_LEN);
        GridSnap snap_orig;
        snap_grid(&snap_orig);
        char *h0 = calculateHashValue();
        char *h0c = strdup(h0);
        free(h0);

        for (int bi = 0; bi < MSG_LEN; bi++)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                unsigned char flipped[MSG_LEN];
                memcpy(flipped, msg, MSG_LEN);
                flipped[bi] ^= (unsigned char)(1u << bit);

                initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
                processBuffer(flipped, MSG_LEN);
                GridSnap snap_flip;
                snap_grid(&snap_flip);
                char *h1 = calculateHashValue();

                if (hamming_hex(h0c, h1) == 0 &&
                    grids_identical(&snap_orig, &snap_flip))
                {

                    /* Replay both paths */
                    uint32_t pathA[MSG_LEN * 4 + 1][2];
                    uint32_t pathB[MSG_LEN * 4 + 1][2];
                    int lenA, lenB;
                    replay_cursor_path(msg, MSG_LEN, pathA, &lenA);
                    replay_cursor_path(flipped, MSG_LEN, pathB, &lenB);

                    /* Count divergent steps */
                    int diff_count = 0;
                    for (int s = 0; s < lenA && s < lenB; s++)
                        if (pathA[s][0] != pathB[s][0] || pathA[s][1] != pathB[s][1])
                            diff_count++;

                    total_coll++;

                    if (diff_count == 0)
                    {
                        zero_diverge++;
                        if (!first_zero_shown)
                        {
                            first_zero_shown = 1;
                            int diff_step = bi * 4 + ((bit < 2) ? 0 : (bit < 4) ? 1
                                                                  : (bit < 6)   ? 2
                                                                                : 3);
                            printf("[DIR-COLL example] msg#%d byte[%d] bit%d "
                                   "(0x%02X->0x%02X) 0 divergent steps, "
                                   "diff at step %d cell (%u,%u)\n",
                                   m, bi, bit, msg[bi], flipped[bi],
                                   diff_step, pathA[diff_step][0], pathA[diff_step][1]);
                        }
                    }
                    else
                    {
                        nonzero_diverge++;
                        if (!first_nonzero_shown)
                        {
                            first_nonzero_shown = 1;
                            int diff_step = bi * 4 + ((bit < 2) ? 0 : (bit < 4) ? 1
                                                                  : (bit < 6)   ? 2
                                                                                : 3);
                            printf("\n[MULTISET-CONV example] msg#%d byte[%d] bit%d "
                                   "(0x%02X->0x%02X) %d divergent steps\n",
                                   m, bi, bit, msg[bi], flipped[bi], diff_count);

                            /* Print path comparison */
                            int start = (diff_step > 3) ? diff_step - 3 : 0;
                            int end = (diff_step + 30 < lenA) ? diff_step + 30 : lenA - 1;
                            printf("Step  PathA        PathB        Same?\n");
                            for (int s2 = start; s2 <= end && s2 < lenA && s2 < lenB; s2++)
                            {
                                int same = (pathA[s2][0] == pathB[s2][0] && pathA[s2][1] == pathB[s2][1]);
                                printf(" %3d  (%2u,%2u)      (%2u,%2u)      %s%s\n",
                                       s2, pathA[s2][0], pathA[s2][1],
                                       pathB[s2][0], pathB[s2][1],
                                       same ? "YES" : "NO ",
                                       (s2 == diff_step) ? "  <-- DIVERGENCE" : "");
                            }
                            /* Visit multiset check */
                            int visitA[FIELD_SIZE][FIELD_SIZE] = {{0}};
                            int visitB[FIELD_SIZE][FIELD_SIZE] = {{0}};
                            for (int s2 = 0; s2 < lenA; s2++)
                                visitA[pathA[s2][0]][pathA[s2][1]]++;
                            for (int s2 = 0; s2 < lenB; s2++)
                                visitB[pathB[s2][0]][pathB[s2][1]]++;
                            int multiset_ok = 1;
                            for (int ii = 0; ii < FIELD_SIZE; ii++)
                                for (int jj = 0; jj < FIELD_SIZE; jj++)
                                    if (visitA[ii][jj] != visitB[ii][jj])
                                        multiset_ok = 0;
                            printf("Visit multisets identical: %s\n\n", multiset_ok ? "YES" : "NO");
                        }
                    }
                }
                free(h1);
            }
        }
        free(h0c);
    }

    printf("--- Part 5 Summary (%d msgs, %d trials) ---\n",
           N5, N5 * MSG_LEN * 8);
    printf("Total Phase-2-identical collisions: %d (%.4f%%)\n",
           total_coll, 100.0 * total_coll / (N5 * MSG_LEN * 8));
    printf("  Direction collision (0 divergent steps): %d (%.1f%%)\n",
           zero_diverge, total_coll ? 100.0 * zero_diverge / total_coll : 0.0);
    printf("  Multiset convergence (>0 divergent):    %d (%.1f%%)\n",
           nonzero_diverge, total_coll ? 100.0 * nonzero_diverge / total_coll : 0.0);
    printf("\n");
}

int main(void)
{
    trace_one_collision();
    return 0;
}
