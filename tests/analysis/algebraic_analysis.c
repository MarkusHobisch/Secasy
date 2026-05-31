/*
 * algebraic_analysis.c
 *
 * Algebraic cryptanalysis of a ROUND-REDUCED, SCALED-DOWN model of the Secasy
 * Phase-3 mixing function. This harness answers three questions that require
 * exhaustive evaluation over the full input space and are therefore only
 * tractable on a toy variant:
 *
 *   F1  Algebraic degree growth.
 *       What is the exact GF(2) polynomial degree of each output bit after
 *       r = 1..R rounds, and does it grow exponentially or linearly per round?
 *       Method: build the full 2^n truth table of every output bit and apply
 *       the Moebius (ANF) transform; the degree is the largest Hamming weight
 *       of a monomial with non-zero coefficient.
 *
 *   F2  Low-degree subpolynomials (cube attack).
 *       Do there exist cubes (subsets of input bits) whose superpoly is of low
 *       degree (constant or linear), and from which round do such cubes vanish?
 *       Method: for a range of cube dimensions, XOR-sum the output over the cube
 *       and compute the ANF degree of the resulting superpoly.
 *
 *   F3  Linear approximations.
 *       Is there a linear combination of output bits that is approximated by a
 *       linear combination of input bits with non-negligible bias?
 *       Method: fast Walsh-Hadamard transform of each output bit; the largest
 *       non-trivial Walsh coefficient gives the best linear approximation and
 *       its correlation / bias.
 *
 * IMPORTANT (honesty): this is a REDUCED model. It preserves the six Secasy
 * column operations (ADD, SUB, XOR, ROTATE_LEFT_XOR, ROTATE_RIGHT_ADD, INVERT)
 * and the modular-addition carry structure that is the construction's only
 * nonlinearity source, but it runs on G cells of W bits each (n = G*W input
 * bits) instead of 256 cells of 64 bits. Conclusions about degree growth and
 * linear/cube resistance are empirical statements about this reduced model;
 * they are an indication, not a proof, for the full 16,384-bit construction.
 *
 * Build (CMakeLists.txt):
 *     add_secasy_test(SecasyAlgebraicAnalysis tests/analysis/algebraic_analysis.c)
 *
 * Run:
 *     ./SecasyAlgebraicAnalysis              # default toy: G=4, W=5, n=20, R=10
 *     ./SecasyAlgebraicAnalysis --quick      # G=4, W=4, n=16 (fast smoke test)
 *     ./SecasyAlgebraicAnalysis G W R seed   # explicit override
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

/* The production translation units (linked in) expect these as externs. The
 * reduced model below does not call into them, but the linker needs them. */
unsigned long numberOfRounds = 1;
int hashLengthInBits = 512;

/* ----------------------------- Toy model ------------------------------ */

#define NUM_COLOR_OPS        6          /* ADD SUB XOR RLX RRA INVERT      */
#define MAX_INPUT_BITS       24u        /* enumeration cap (2^24 entries)  */
#define DEFAULT_CELLS        4u
#define DEFAULT_CELL_BITS    5u
#define DEFAULT_ROUNDS       10u
#define DEFAULT_SCHED_SEED   0x9E3779B97F4A7C15ULL
#define QUICK_CELL_BITS      4u
#define ROT_LEFT_FRACTION    2          /* rotate-left amount inside W bits */
#define ROT_RIGHT_FRACTION   1          /* rotate-right amount inside W bits */
#define CUBE_RANDOM_SAMPLES  16u        /* random cubes tested per dimension */
#define CUBE_MAX_DIMENSION   4u         /* largest cube dimension probed     */

typedef enum
{
    OP_ADD = 0,
    OP_SUB = 1,
    OP_XOR = 2,
    OP_ROTATE_LEFT_XOR = 3,
    OP_ROTATE_RIGHT_ADD = 4,
    OP_INVERT = 5
} ColorOp_t;

typedef struct
{
    unsigned cells;        /* G : number of grid cells          */
    unsigned cellBits;     /* W : bits per cell                 */
    unsigned inputBits;    /* n = G * W                         */
    unsigned rounds;       /* R : maximum number of mix rounds  */
    unsigned rotLeft;      /* rotate-left amount  (< W)         */
    unsigned rotRight;     /* rotate-right amount (< W)         */
    uint64_t schedSeed;    /* deterministic colour-schedule seed*/
} ToyConfig_t;

/* The colour schedule is value-independent in Secasy (it is fixed by the
 * Phase-2 layout). We mirror that property with a deterministic per-(round,
 * cell) schedule, derived from a seed so the experiment is reproducible. */
static ColorOp_t g_schedule[64][MAX_INPUT_BITS]; /* [round][cell] */

static uint64_t splitmix64(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void build_schedule(const ToyConfig_t *cfg)
{
    uint64_t state = cfg->schedSeed;
    for (unsigned r = 0; r < cfg->rounds; ++r)
    {
        for (unsigned c = 0; c < cfg->cells; ++c)
        {
            g_schedule[r][c] = (ColorOp_t)(splitmix64(&state) % NUM_COLOR_OPS);
        }
    }
}

static uint32_t mask_w(const ToyConfig_t *cfg)
{
    return (cfg->cellBits >= 32u) ? 0xFFFFFFFFu
                                  : ((1u << cfg->cellBits) - 1u);
}

static uint32_t rotl_w(uint32_t x, unsigned s, unsigned w)
{
    uint32_t m = (w >= 32u) ? 0xFFFFFFFFu : ((1u << w) - 1u);
    x &= m;
    s %= w;
    return ((x << s) | (x >> (w - s))) & m;
}

static uint32_t rotr_w(uint32_t x, unsigned s, unsigned w)
{
    uint32_t m = (w >= 32u) ? 0xFFFFFFFFu : ((1u << w) - 1u);
    x &= m;
    s %= w;
    return ((x >> s) | (x << (w - s))) & m;
}

/*
 * One mixing round over the cell array, in place and left-to-right, mirroring
 * Secasy Phase 3: each cell is combined with its cyclic right neighbour using
 * the cell's scheduled colour operation. ADD/SUB are linear over Z_{2^W};
 * XOR/RLX/RRA/INVERT are the nonlinear operations.
 */
static void apply_round(uint32_t *cell, const ToyConfig_t *cfg, unsigned round)
{
    const uint32_t m = mask_w(cfg);
    for (unsigned i = 0; i < cfg->cells; ++i)
    {
        uint32_t neighbour = cell[(i + 1u) % cfg->cells] & m;
        uint32_t c = cell[i] & m;
        switch (g_schedule[round][i])
        {
            case OP_ADD:               c = (c + neighbour) & m;             break;
            case OP_SUB:               c = (c - neighbour) & m;             break;
            case OP_XOR:               c = c ^ neighbour;                   break;
            case OP_ROTATE_LEFT_XOR:   c = rotl_w(c, cfg->rotLeft, cfg->cellBits) ^ neighbour; break;
            case OP_ROTATE_RIGHT_ADD:  c = (rotr_w(c, cfg->rotRight, cfg->cellBits) + neighbour) & m; break;
            case OP_INVERT:            c = (~c) & m;                        break;
            default:                                                        break;
        }
        cell[i] = c & m;
    }
}

/* Evaluate the toy mixer on an n-bit input, returning the n-bit output state
 * after exactly `rounds` rounds. Input/output are packed little-endian: cell i
 * occupies bits [i*W .. i*W+W-1]. */
static uint32_t toy_eval(uint32_t input, const ToyConfig_t *cfg, unsigned rounds)
{
    uint32_t cell[MAX_INPUT_BITS];
    const uint32_t m = mask_w(cfg);
    for (unsigned i = 0; i < cfg->cells; ++i)
    {
        cell[i] = (input >> (i * cfg->cellBits)) & m;
    }
    for (unsigned r = 0; r < rounds; ++r)
    {
        apply_round(cell, cfg, r);
    }
    uint32_t out = 0;
    for (unsigned i = 0; i < cfg->cells; ++i)
    {
        out |= (cell[i] & m) << (i * cfg->cellBits);
    }
    return out;
}

/* --------------------------- Transforms ------------------------------- */

static unsigned popcount_u32(uint32_t x)
{
    unsigned c = 0;
    while (x) { c += (x & 1u); x >>= 1; }
    return c;
}

/*
 * In-place GF(2) Moebius (zeta) transform on a truth table of 2^n entries
 * (each 0/1). On return, tt[mask] is the ANF coefficient of the monomial whose
 * variable set is `mask`.
 */
static void moebius_transform(uint8_t *tt, unsigned n)
{
    const size_t size = (size_t)1u << n;
    for (unsigned i = 0; i < n; ++i)
    {
        const size_t step = (size_t)1u << i;
        for (size_t j = 0; j < size; ++j)
        {
            if (j & step)
            {
                tt[j] ^= tt[j ^ step];
            }
        }
    }
}

/* Algebraic degree = largest Hamming weight of a monomial with coeff 1. */
static unsigned anf_degree(const uint8_t *anf, unsigned n)
{
    const size_t size = (size_t)1u << n;
    unsigned degree = 0;
    for (size_t j = 0; j < size; ++j)
    {
        if (anf[j])
        {
            unsigned w = popcount_u32((uint32_t)j);
            if (w > degree) degree = w;
        }
    }
    return degree;
}

/*
 * Fast Walsh-Hadamard transform on a +-1 sequence of 2^n int32 entries.
 * After the transform, w[a] = sum_x (-1)^{f(x)} (-1)^{a.x}.
 */
static void walsh_hadamard(int32_t *w, unsigned n)
{
    const size_t size = (size_t)1u << n;
    for (unsigned i = 0; i < n; ++i)
    {
        const size_t step = (size_t)1u << i;
        for (size_t j = 0; j < size; j += (step << 1))
        {
            for (size_t k = j; k < j + step; ++k)
            {
                int32_t a = w[k];
                int32_t b = w[k + step];
                w[k]        = a + b;
                w[k + step] = a - b;
            }
        }
    }
}

/* ----------------------- Per-round measurements ----------------------- */

typedef struct
{
    unsigned maxDegree;       /* F1: max algebraic degree over output bits */
    double   meanDegree;      /* F1: mean algebraic degree over output bits*/
    unsigned minDegree;       /* F1: min algebraic degree over output bits */
    int32_t  maxWalshAbs;     /* F3: largest |Walsh coeff| (a != 0)        */
    double   bestCorrelation; /* F3: maxWalshAbs / 2^n                      */
    double   bestBias;        /* F3: bestCorrelation / 2                    */
    unsigned cubeMaxLinDim;   /* F2: largest cube dim with a linear superpoly */
} RoundMetrics_t;

/* Build the truth table of output bit `bit` over all 2^n inputs for a fixed
 * round count, then return its algebraic degree. Reuses the caller's scratch. */
static unsigned degree_of_output_bit(const uint32_t *outputs, unsigned n,
                                     unsigned bit, uint8_t *scratch)
{
    const size_t size = (size_t)1u << n;
    for (size_t x = 0; x < size; ++x)
    {
        scratch[x] = (uint8_t)((outputs[x] >> bit) & 1u);
    }
    moebius_transform(scratch, n);
    return anf_degree(scratch, n);
}

/* Best linear-approximation correlation for output bit `bit`. */
static int32_t walsh_max_of_output_bit(const uint32_t *outputs, unsigned n,
                                       unsigned bit, int32_t *scratch)
{
    const size_t size = (size_t)1u << n;
    for (size_t x = 0; x < size; ++x)
    {
        int32_t f = (int32_t)((outputs[x] >> bit) & 1u);
        scratch[x] = (f == 0) ? 1 : -1;
    }
    walsh_hadamard(scratch, n);
    int32_t best = 0;
    for (size_t a = 1; a < size; ++a) /* skip a = 0 (the constant bias) */
    {
        int32_t v = scratch[a] < 0 ? -scratch[a] : scratch[a];
        if (v > best) best = v;
    }
    return best;
}

/*
 * F2: for a given cube (set of variable positions) and output bit, compute the
 * superpoly by XOR-summing the output bit over all 2^d cube assignments while
 * the non-cube variables are fixed to 0, then return the superpoly's ANF
 * degree. The superpoly is a function of the (n - d) non-cube variables.
 */
static unsigned cube_superpoly_degree(const uint32_t *outputs, unsigned n,
                                      unsigned bit, const unsigned *cubeVars,
                                      unsigned cubeDim, uint8_t *scratch)
{
    const unsigned freeBits = n - cubeDim;
    const size_t freeSize = (size_t)1u << freeBits;

    /* Enumerate non-cube variable positions. */
    unsigned freeVars[MAX_INPUT_BITS];
    unsigned isCube[MAX_INPUT_BITS] = {0};
    for (unsigned i = 0; i < cubeDim; ++i) isCube[cubeVars[i]] = 1u;
    unsigned f = 0;
    for (unsigned i = 0; i < n; ++i) if (!isCube[i]) freeVars[f++] = i;

    for (size_t y = 0; y < freeSize; ++y)
    {
        /* Base input: scatter the free-variable assignment y. */
        uint32_t base = 0;
        for (unsigned i = 0; i < freeBits; ++i)
        {
            if (y & ((size_t)1u << i)) base |= (1u << freeVars[i]);
        }
        /* XOR-sum the output bit over the 2^cubeDim cube assignments. */
        uint8_t acc = 0;
        const size_t cubeSize = (size_t)1u << cubeDim;
        for (size_t c = 0; c < cubeSize; ++c)
        {
            uint32_t x = base;
            for (unsigned i = 0; i < cubeDim; ++i)
            {
                if (c & ((size_t)1u << i)) x |= (1u << cubeVars[i]);
            }
            acc ^= (uint8_t)((outputs[x] >> bit) & 1u);
        }
        scratch[y] = acc;
    }
    moebius_transform(scratch, freeBits);
    return anf_degree(scratch, freeBits);
}

/* --------------------------- Driver ----------------------------------- */

static uint64_t xs_state = 0x123456789ABCDEFULL;
static uint64_t xs_rand(void)
{
    uint64_t x = xs_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    xs_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void analyze_round(const ToyConfig_t *cfg, unsigned rounds,
                          uint32_t *outputs, uint8_t *byteScratch,
                          int32_t *intScratch, RoundMetrics_t *out)
{
    const unsigned n = cfg->inputBits;
    const size_t size = (size_t)1u << n;

    /* Evaluate the toy mixer over the entire input space once. */
    for (size_t x = 0; x < size; ++x)
    {
        outputs[x] = toy_eval((uint32_t)x, cfg, rounds);
    }

    /* F1: algebraic degree over every output bit. */
    unsigned maxDeg = 0, minDeg = n;
    double sumDeg = 0.0;
    for (unsigned b = 0; b < n; ++b)
    {
        unsigned d = degree_of_output_bit(outputs, n, b, byteScratch);
        if (d > maxDeg) maxDeg = d;
        if (d < minDeg) minDeg = d;
        sumDeg += d;
    }
    out->maxDegree = maxDeg;
    out->minDegree = minDeg;
    out->meanDegree = sumDeg / n;

    /* F3: best linear approximation over every output bit. */
    int32_t maxWalsh = 0;
    for (unsigned b = 0; b < n; ++b)
    {
        int32_t w = walsh_max_of_output_bit(outputs, n, b, intScratch);
        if (w > maxWalsh) maxWalsh = w;
    }
    out->maxWalshAbs = maxWalsh;
    out->bestCorrelation = (double)maxWalsh / (double)size;
    out->bestBias = out->bestCorrelation / 2.0;

    /* F2: probe cubes of dimensions 1..CUBE_MAX_DIMENSION for a linear
     * (degree-1) superpoly in any output bit. */
    unsigned cubeMaxLinDim = 0;
    for (unsigned d = 1; d <= CUBE_MAX_DIMENSION && d < n; ++d)
    {
        int found = 0;
        for (unsigned s = 0; s < CUBE_RANDOM_SAMPLES && !found; ++s)
        {
            unsigned cubeVars[CUBE_MAX_DIMENSION];
            unsigned chosen[MAX_INPUT_BITS] = {0};
            for (unsigned i = 0; i < d; ++i)
            {
                unsigned v;
                do { v = (unsigned)(xs_rand() % n); } while (chosen[v]);
                chosen[v] = 1u;
                cubeVars[i] = v;
            }
            for (unsigned b = 0; b < n && !found; ++b)
            {
                unsigned spDeg = cube_superpoly_degree(outputs, n, b,
                                                       cubeVars, d, byteScratch);
                if (spDeg == 1u) found = 1; /* exploitable linear superpoly */
            }
        }
        if (found) cubeMaxLinDim = d;
    }
    out->cubeMaxLinDim = cubeMaxLinDim;
}

static void print_config(const ToyConfig_t *cfg)
{
    printf("Reduced-model algebraic analysis of the Secasy Phase-3 mixer\n");
    printf("  cells G            = %u\n", cfg->cells);
    printf("  bits per cell W    = %u\n", cfg->cellBits);
    printf("  input bits n = G*W = %u  (2^n = %" PRIu64 " enumerated inputs)\n",
           cfg->inputBits, (uint64_t)1u << cfg->inputBits);
    printf("  max rounds R       = %u\n", cfg->rounds);
    printf("  rotate-left amount = %u, rotate-right amount = %u\n",
           cfg->rotLeft, cfg->rotRight);
    printf("  schedule seed      = 0x%016" PRIX64 "\n\n", cfg->schedSeed);
}

int main(int argc, char **argv)
{
    ToyConfig_t cfg;
    cfg.cells = DEFAULT_CELLS;
    cfg.cellBits = DEFAULT_CELL_BITS;
    cfg.rounds = DEFAULT_ROUNDS;
    cfg.schedSeed = DEFAULT_SCHED_SEED;

    if (argc == 2 && strcmp(argv[1], "--quick") == 0)
    {
        cfg.cellBits = QUICK_CELL_BITS;
    }
    else if (argc >= 4)
    {
        cfg.cells = (unsigned)strtoul(argv[1], NULL, 10);
        cfg.cellBits = (unsigned)strtoul(argv[2], NULL, 10);
        cfg.rounds = (unsigned)strtoul(argv[3], NULL, 10);
        if (argc >= 5) cfg.schedSeed = strtoull(argv[4], NULL, 0);
    }

    cfg.inputBits = cfg.cells * cfg.cellBits;
    cfg.rotLeft = ROT_LEFT_FRACTION;
    cfg.rotRight = ROT_RIGHT_FRACTION;

    if (cfg.cells == 0 || cfg.cellBits == 0 ||
        cfg.inputBits == 0 || cfg.inputBits > MAX_INPUT_BITS)
    {
        fprintf(stderr,
                "error: n = G*W = %u must satisfy 1..%u (G=%u, W=%u)\n",
                cfg.inputBits, MAX_INPUT_BITS, cfg.cells, cfg.cellBits);
        return EXIT_FAILURE;
    }
    if (cfg.rounds == 0 || cfg.rounds > 64)
    {
        fprintf(stderr, "error: R = %u must satisfy 1..64\n", cfg.rounds);
        return EXIT_FAILURE;
    }
    if (cfg.rotLeft >= cfg.cellBits) cfg.rotLeft = 1u;
    if (cfg.rotRight >= cfg.cellBits) cfg.rotRight = 1u;

    build_schedule(&cfg);
    print_config(&cfg);

    const unsigned n = cfg.inputBits;
    const size_t size = (size_t)1u << n;

    uint32_t *outputs = malloc(size * sizeof(uint32_t));
    uint8_t  *byteScratch = malloc(size * sizeof(uint8_t));
    int32_t  *intScratch = malloc(size * sizeof(int32_t));
    if (!outputs || !byteScratch || !intScratch)
    {
        fprintf(stderr, "error: out of memory for 2^%u entries\n", n);
        free(outputs); free(byteScratch); free(intScratch);
        return EXIT_FAILURE;
    }

    const double noiseBias = 1.0 / (2.0 * (double)(1u << (n / 2)));
    printf("Ideal/saturated degree for n=%u is n-1 = %u.\n", n, n - 1);
    printf("Random-function noise floor for the linear bias is about "
           "2^-(n/2)/2 = %.3e.\n\n", noiseBias);

    printf("  r |  F1: degree (min/mean/max) | F2: max cube dim w/ linear "
           "superpoly | F3: best |corr|  bias\n");
    printf("----+----------------------------+------------------------------"
           "------+----------------------\n");

    RoundMetrics_t prev; memset(&prev, 0, sizeof(prev));
    for (unsigned r = 1; r <= cfg.rounds; ++r)
    {
        RoundMetrics_t m;
        analyze_round(&cfg, r, outputs, byteScratch, intScratch, &m);

        char cubeCell[16];
        if (m.cubeMaxLinDim == 0) snprintf(cubeCell, sizeof(cubeCell), "none");
        else snprintf(cubeCell, sizeof(cubeCell), "%u", m.cubeMaxLinDim);

        printf("%3u | %6u / %6.2f / %6u   | %-34s | %8.5f  %9.6f\n",
               r, m.minDegree, m.meanDegree, m.maxDegree,
               cubeCell, m.bestCorrelation, m.bestBias);

        prev = m;
    }
    (void)prev;

    printf("\nReading the table:\n");
    printf("  F1 - if max degree roughly doubles per round until it saturates "
           "at n-1=%u, growth\n", n - 1);
    printf("       is exponential (desirable); if it climbs by a small constant "
           "per round it is linear.\n");
    printf("  F2 - 'none' means no cube of dimension <= %u produced a linear "
           "superpoly in any output\n", CUBE_MAX_DIMENSION);
    printf("       bit; once this reads 'none' for all later rounds, the "
           "tested cube attacks fail.\n");
    printf("  F3 - best |corr| is the largest linear-approximation correlation; "
           "values near the\n");
    printf("       noise floor %.3e indicate no exploitable linear "
           "approximation.\n", noiseBias);

    free(outputs);
    free(byteScratch);
    free(intScratch);
    return EXIT_SUCCESS;
}
