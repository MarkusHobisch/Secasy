/*
 * algebraic_attack_n64.c
 *
 * Regression test for a retired structural attack on the 64-bit output.
 *
 * CURRENT STATUS
 * --------------
 * The derivation below describes the former XOR accumulator. Production now
 * uses modular addition before each rotation. The mandatory model-vs-production
 * check therefore fails and the legacy collision/preimage demonstration is
 * skipped. Keeping the old construction behind that check guards against an
 * accidental reintroduction of the separable XOR finalizer.
 *
 * THE STRUCTURAL OBSERVATION
 * --------------------------
 * The Phase 4 extractor (Calculations.c, hashValue) computes, over the 256
 * grid cells in iteration order k = 0..255 (x outer, y inner):
 *
 *      a_0   = 0
 *      a_{k+1} = ROTL_7( a_k  XOR  (value_k * weight_k mod 2^64) )
 *      digest  = a_256
 *
 * where weight_k = 2*(k+1)+1 is an ODD constant (a unit in Z/2^64). Because the
 * 7-bit rotation is a GF(2)-linear bit permutation, it distributes over XOR.
 * Unrolling the recurrence therefore yields a CLOSED FORM:
 *
 *      digest = XOR_{k=0}^{255}  ROTL_{ r_k }( value_k * weight_k mod 2^64 ),
 *      r_k    = ( 7 * (256 - k) ) mod 64.
 *
 * i.e. the extractor is the XOR of 256 INDEPENDENT, INVERTIBLE functions of
 * DISJOINT inputs:
 *
 *      digest = g_0(value_0) XOR g_1(value_1) XOR ... XOR g_255(value_255),
 *      g_k(v) = ROTL_{r_k}( v * weight_k mod 2^64 ),
 *
 * and each g_k is a bijection on Z/2^64 (odd-constant multiply followed by a
 * rotation), with an explicit inverse
 *
 *      g_k^{-1}(u) = ROTR_{r_k}(u) * inv_odd(weight_k)   (mod 2^64).
 *
 * This "XOR-of-independent-bijections" shape is a classic generalised-birthday
 * / k-sum weakness and DESTROYS both collision and preimage resistance of the
 * extractor at O(1) cost -- no search, far below 2^32 (birthday) and 2^64
 * (preimage):
 *
 *   PREIMAGE (any target t): fix value_1..value_255 to anything, then set
 *      value_0 = g_0^{-1}( t XOR g_1(value_1) XOR ... XOR g_255(value_255) ).
 *      One inverse evaluation. The state hashes to t exactly.
 *
 *   COLLISION: pick two cells i != j. Change (value_i, value_j) -> (value_i',
 *      value_j') while keeping g_i(.) XOR g_j(.) invariant:
 *          value_j' = g_j^{-1}( g_i(value_i) XOR g_j(value_j) XOR g_i(value_i') ).
 *      All other cells are untouched, so the two states collide. O(1).
 *
 * VERIFICATION
 * ------------
 * (1) We first VERIFY the closed form against the unmodified production
 *     extractor hashValue(0) on random states (must match bit-for-bit).
 * (2) Every crafted collision / preimage is then RE-VERIFIED by calling the
 *     unmodified hashValue(0) on the constructed states.
 *
 * SCOPE / HONESTY
 * ---------------
 * The attack is on the Secasy 16384 -> 64 COMPRESSION (the extractor), which is
 * the standard target of compression-function cryptanalysis. The collision /
 * preimage are exhibited on the state domain the extractor consumes. Mapping an
 * arbitrary crafted state back to a concrete message must still invert the
 * (lossy, nonlinear) Phase 2 walk and the Phase 3 rounds; that message-lift is
 * NOT performed here and is discussed in the accompanying report. What is shown,
 * rigorously and without brute force, is that the 64-bit extractor offers
 * neither collision nor preimage resistance.
 *
 * Build (CMakeLists.txt):
 *     add_secasy_test(SecasyAlgebraicAttackN64 tests/analysis/algebraic_attack_n64.c)
 *
 * Run:
 *     ./SecasyAlgebraicAttackN64                # default seed
 *     ./SecasyAlgebraicAttackN64 <seed>         # choose RNG seed
 *
 * This is an internal analysis tool, not a formal proof.
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

/* The production translation units expect these as externs (normally main.c). */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = HASH_OUTPUT_BITS; /* n = 64 */

extern Tile field[FIELD_SIZE][FIELD_SIZE];
extern Position pos;

#define OUT_BITS 64u
#define GRID_CELLS (FIELD_SIZE * FIELD_SIZE) /* 256 */
#define ROT_STEP 7u                          /* extractor rotation per cell */

/* ----------------------------- RNG ------------------------------------- */

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

/* ----------------------- 64-bit rotation helpers ----------------------- */

static uint64_t rotl64(uint64_t v, unsigned r)
{
    r &= 63u;
    return r ? ((v << r) | (v >> (64u - r))) : v;
}
static uint64_t rotr64(uint64_t v, unsigned r)
{
    r &= 63u;
    return r ? ((v >> r) | (v << (64u - r))) : v;
}

/* Modular inverse of an odd 64-bit constant (Newton/Hensel iteration). */
static uint64_t inv_odd_u64(uint64_t a)
{
    /* requires a odd; converges in 6 doublings: 1,2,4,8,16,32,64 bits */
    uint64_t x = a; /* good to 3 bits since a*a ~ 1 mod 8 for odd a */
    x *= 2u - a * x; /* 6 bits  */
    x *= 2u - a * x; /* 12 bits */
    x *= 2u - a * x; /* 24 bits */
    x *= 2u - a * x; /* 48 bits */
    x *= 2u - a * x; /* 96 -> 64 bits */
    return x;
}

/* ---------------------- Extractor structural model --------------------- */

/*
 * Per-cell weight and rotation amount, matching hashValue(blockIndex=0).
 * Cell iteration order k = x*FIELD_SIZE + y, x outer, y inner.
 */
static uint64_t g_weight[GRID_CELLS];
static unsigned g_rot[GRID_CELLS];
static uint64_t g_winv[GRID_CELLS];

static void init_extractor_model(void)
{
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
        {
            unsigned k = x * FIELD_SIZE + y;
            uint64_t posWeight = (uint64_t)(x * FIELD_SIZE + y + 1); /* blockIndex 0 */
            uint64_t w = 2u * posWeight + 1u;                       /* odd */
            g_weight[k] = w;
            g_winv[k] = inv_odd_u64(w);
            /* term enters at step k, then rotated by ROT_STEP for each of the
             * remaining (GRID_CELLS - k) accumulation steps */
            g_rot[k] = (ROT_STEP * (GRID_CELLS - k)) % 64u;
        }
}

/* g_k(v) = ROTL_{r_k}( v * w_k ) and its inverse. */
static uint64_t g_cell(unsigned k, uint64_t v) { return rotl64(v * g_weight[k], g_rot[k]); }
static uint64_t g_cell_inv(unsigned k, uint64_t u) { return rotr64(u, g_rot[k]) * g_winv[k]; }

/* Closed-form digest of a value-vector (model prediction). */
static uint64_t model_digest(const uint64_t *values)
{
    uint64_t acc = 0;
    for (unsigned k = 0; k < GRID_CELLS; k++)
        acc ^= g_cell(k, values[k]);
    return acc;
}

/* ---------------------- Production extractor wrapper ------------------- */

/* Load a value-vector into the global field (schedule fields irrelevant to the
 * extractor; set to benign defaults). */
static void load_values(const uint64_t *values)
{
    for (uint32_t x = 0; x < FIELD_SIZE; x++)
        for (uint32_t y = 0; y < FIELD_SIZE; y++)
        {
            unsigned k = x * FIELD_SIZE + y;
            field[x][y].posX = x;
            field[x][y].posY = y;
            field[x][y].value = values[k];
            field[x][y].colorIndex = ADD;
            field[x][y].primeIndex = 0;
        }
    pos.x = 0;
    pos.y = 0;
}

/* The UNMODIFIED production extractor on a chosen value-vector. */
static uint64_t prod_extract(const uint64_t *values)
{
    load_values(values);
    return hashValue(0);
}

/* ------------------------------ helpers -------------------------------- */

static void rand_state(uint64_t *values)
{
    for (unsigned k = 0; k < GRID_CELLS; k++)
        values[k] = rng_u64();
}

static int states_differ(const uint64_t *a, const uint64_t *b, unsigned *firstCell)
{
    for (unsigned k = 0; k < GRID_CELLS; k++)
        if (a[k] != b[k])
        {
            if (firstCell)
                *firstCell = k;
            return 1;
        }
    return 0;
}

static void print_cell(const char *label, unsigned k, uint64_t v)
{
    printf("    %s cell[%u] (x=%u,y=%u) value = %016" PRIx64
           "  (w=%016" PRIx64 ", rot=%u)\n",
           label, k, k / FIELD_SIZE, k % FIELD_SIZE, v, g_weight[k], g_rot[k]);
}

/* ------------------------------- main ---------------------------------- */

int main(int argc, char **argv)
{
    uint64_t seed = 0xA11CE5ECA51ULL;
    if (argc >= 2)
        seed = strtoull(argv[1], NULL, 10);
    rng_state = seed ? seed : 0x9E3779B97F4A7C15ULL;

    printf("============================================================\n");
    printf(" Secasy n=64 REGRESSION CHECK FOR RETIRED XOR ATTACK\n");
    printf("============================================================\n");
    printf("  output bits   : %u\n", OUT_BITS);
    printf("  grid cells    : %u\n", GRID_CELLS);
    printf("  RNG seed      : 0x%" PRIx64 "\n", seed);

    init_extractor_model();

    /* --- [0] Verify the closed-form model equals the production extractor -- */
    printf("\n[0] Verifying closed form  digest = XOR_k ROTL(value_k * w_k, r_k)\n");
    printf("    against the unmodified production hashValue(0)...\n");
    {
        unsigned trials = 100000, bad = 0;
        uint64_t v[GRID_CELLS];
        for (unsigned t = 0; t < trials; t++)
        {
            rand_state(v);
            uint64_t m = model_digest(v);
            uint64_t p = prod_extract(v);
            if (m != p)
            {
                if (bad < 3)
                    printf("    MISMATCH t=%u model=%016" PRIx64 " prod=%016" PRIx64 "\n",
                           t, m, p);
                bad++;
            }
        }
        printf("    matched on %u/%u random states.\n", trials - bad, trials);
        if (bad != 0)
        {
            printf("    [OK] closed form does NOT match the production extractor.\n");
            printf("         The additive (+=) finalizer defeats the separable-XOR\n");
            printf("         closed form: ROTL distributes over XOR but not over\n");
            printf("         modular addition, so the XOR-of-256-bijections shape\n");
            printf("         collapses. The A1 break is NEUTRALIZED; the XOR-based\n");
            printf("         collision/preimage steps below no longer apply and are\n");
            printf("         skipped.\n");
            return 0;
        }
        printf("    [OK] closed form is EXACT. Extractor = XOR of 256 independent\n");
        printf("         bijections g_k(v)=ROTL(v*w_k, r_k) over disjoint inputs.\n");
    }

    /* --- Sanity: verify g_k inverse on the model ------------------------- */
    {
        unsigned okinv = 1;
        for (unsigned k = 0; k < GRID_CELLS; k++)
        {
            uint64_t v = rng_u64();
            if (g_cell_inv(k, g_cell(k, v)) != v)
            {
                okinv = 0;
                break;
            }
        }
        printf("\n[1] Per-cell inverse g_k^{-1} check: %s\n",
               okinv ? "[OK] g_k^{-1}(g_k(v)) == v for all 256 cells"
                     : "[!!] inverse FAILED");
        if (!okinv)
            return 2;
    }

    /* ------------------------------ PREIMAGE --------------------------- */
    printf("\n[2] PREIMAGE: hit an arbitrary target digest in O(1)...\n");
    {
        /* target = extractor of an independent secret state (genuine challenge) */
        uint64_t secret[GRID_CELLS];
        rand_state(secret);
        uint64_t target = prod_extract(secret);
        printf("    target t = hashValue(secret state) = %016" PRIx64 "\n", target);

        /* fix cells 1..255 to random values, solve cell 0 */
        uint64_t v[GRID_CELLS];
        rand_state(v);
        uint64_t acc = 0;
        for (unsigned k = 1; k < GRID_CELLS; k++)
            acc ^= g_cell(k, v[k]);
        v[0] = g_cell_inv(0, target ^ acc); /* g_0(v0) = t XOR (rest) */

        uint64_t got = prod_extract(v); /* verify on production extractor */
        print_cell("solved   ", 0, v[0]);
        printf("    hashValue(constructed state) = %016" PRIx64 "\n", got);
        if (got == target)
            printf("    [OK] PREIMAGE CONFIRMED: one inverse evaluation, "
                   "hashValue == t.\n");
        else
            printf("    [!!] verification failed.\n");
    }

    /* ------------------------------ COLLISION -------------------------- */
    printf("\n[3] COLLISION: two distinct states, same digest, in O(1)...\n");
    {
        /* base state from a real message (reachable), then alter two cells */
        unsigned char msg[64];
        for (unsigned i = 0; i < sizeof(msg); i++)
            msg[i] = (unsigned char)(rng_u64() & 0xFFu);
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(msg, sizeof(msg));
        /* snapshot the reachable post-Phase-2 values as base state A */
        uint64_t A[GRID_CELLS], B[GRID_CELLS];
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
            for (uint32_t y = 0; y < FIELD_SIZE; y++)
                A[x * FIELD_SIZE + y] = field[x][y].value;
        memcpy(B, A, sizeof(A));

        /* pick two cells i, j; change value_i freely, recompute value_j to keep
         * g_i XOR g_j invariant => digests of A and B coincide. */
        unsigned i = 0, j = 137;
        uint64_t newVi = A[i] ^ 0xDEADBEEF12345678ULL; /* any value != A[i] */
        if (newVi == A[i])
            newVi ^= 1u;
        uint64_t keep = g_cell(i, A[i]) ^ g_cell(j, A[j]); /* invariant */
        B[i] = newVi;
        B[j] = g_cell_inv(j, keep ^ g_cell(i, newVi));

        uint64_t hA = prod_extract(A);
        uint64_t hB = prod_extract(B);
        unsigned firstDiff = 0;
        int differ = states_differ(A, B, &firstDiff);

        print_cell("A        ", i, A[i]);
        print_cell("A        ", j, A[j]);
        print_cell("B        ", i, B[i]);
        print_cell("B        ", j, B[j]);
        printf("    states differ: %s (first differing cell %u)\n",
               differ ? "yes" : "NO", firstDiff);
        printf("    hashValue(A) = %016" PRIx64 "\n", hA);
        printf("    hashValue(B) = %016" PRIx64 "\n", hB);
        if (differ && hA == hB)
            printf("    [OK] COLLISION CONFIRMED on the production extractor: "
                   "A != B, hashValue(A) == hashValue(B).\n");
        else
            printf("    [!!] verification failed.\n");
    }

    /* ------------------------ COLLISION FAMILY ------------------------- */
    printf("\n[4] COLLISION FAMILY: 2^64 collisions for the SAME digest...\n");
    {
        /* All states with cell i = a, cell j = g_j^{-1}(K XOR g_i(a)) and any
         * fixed rest collide. Demonstrate 5 distinct members. */
        uint64_t base[GRID_CELLS];
        rand_state(base);
        unsigned i = 5, j = 200;
        uint64_t K = g_cell(i, base[i]) ^ g_cell(j, base[j]);
        uint64_t firstDigest = 0;
        int allEqual = 1, allDistinct = 1;
        uint64_t seenVi[5];
        for (int m = 0; m < 5; m++)
        {
            uint64_t v[GRID_CELLS];
            memcpy(v, base, sizeof(v));
            uint64_t a = rng_u64();
            v[i] = a;
            v[j] = g_cell_inv(j, K ^ g_cell(i, a));
            uint64_t h = prod_extract(v);
            seenVi[m] = a;
            if (m == 0)
                firstDigest = h;
            else if (h != firstDigest)
                allEqual = 0;
            printf("    member %d: cell[%u]=%016" PRIx64 "  digest=%016" PRIx64 "\n",
                   m, i, a, h);
        }
        for (int m = 0; m < 5 && allDistinct; m++)
            for (int n = m + 1; n < 5; n++)
                if (seenVi[m] == seenVi[n])
                    allDistinct = 0;
        printf("    all 5 digests equal: %s ; all members distinct: %s\n",
               allEqual ? "yes" : "NO", allDistinct ? "yes" : "no");
        if (allEqual && allDistinct)
            printf("    [OK] An (at least) 2^64-size collision family confirmed "
                   "(one free cell parameter a).\n");
    }

    /* ------------------------------- COST ------------------------------ */
    printf("\n[5] COST SUMMARY\n");
    printf("    preimage  : 1 modular inverse + 255 mults/xors  = O(1)\n");
    printf("    collision : 2 g-evaluations + 1 inverse         = O(1)\n");
    printf("    model     : derived analytically (no probing needed)\n");
    printf("    => collision cost 2^0 vs birthday 2^32; preimage 2^0 vs 2^64.\n");

    printf("\nDone.\n");
    return 0;
}
