#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile field[FIELD_SIZE][FIELD_SIZE];

uint64_t hashValue(uint64_t blockIndex)
{
    /*
     * Finalization extractor: multiply-add-rotate (MAR) accumulation.
     *
     * After Phase 3 the 16384-bit grid state is fully diffused, but its cell
     * VALUES are not guaranteed to be full-width 64-bit words: for short or
     * low-entropy messages the cells stay small (the grid starts at a small
     * constant and the ADD/SUB mixing only accumulates modest sums). Plain
     * truncation of such words would emit near-zero, low-entropy output, so a
     * whitening finalizer is required.
     *
     * Two design constraints are satisfied here:
     *
     *   1. POSITION-SENSITIVE WEIGHTING. Each cell is multiplied by an odd,
     *      position-dependent weight. Odd numbers are units in Z/2^64, so
     *      value -> value*weight is a bijection for a fixed weight. This avoids
     *      structurally dead input bits and prevents equal treatment of cells
     *      at different positions. Multiplication alone does not guarantee
     *      full-width diffusion of small values.
     *
     *   2. NON-SEPARABILITY. The running value is combined with modular ADDITION
     *      (not XOR) before each rotation. Because ROTL distributes over XOR but
     *      NOT over addition, carry propagation couples bit positions. The
     *      following rotation redistributes these dependencies, including
     *      wrap-around at the word boundary. Consequently, the digest cannot
     *      be factored into an XOR of independent per-cell maps. This removes
     *      the generalized-XOR / k-sum structure that an XOR accumulator would
     *      expose.
     *
     * The blockIndex offset makes every 64-bit output block a distinct mixture
     * of the same grid state, so longer digests are produced block by block.
     */
    uint64_t accumulation = 0;

    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            /* Unique position index, offset by the requested output block. */
            const uint64_t position = (uint64_t)(x * FIELD_SIZE + y + 1) + (uint64_t)blockIndex * FIELD_SIZE * FIELD_SIZE;

            /* An odd weight is invertible modulo 2^64 and preserves input differences. */
            const uint64_t weight = HASH_POSITION_WEIGHT_SCALE * position + 1U;

            /* ADD couples bits through carries; rotation redistributes the result. */
            accumulation += field[x][y].value * weight;
            accumulation = ROTATE_LEFT_64(accumulation, HASH_EXTRACTION_ROTATION_BITS);
        }
    }

    return accumulation;
}
