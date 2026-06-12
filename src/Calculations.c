#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];

uint64_t hashValue(unsigned long blockIndex)
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
     *   1. WHITENING. Each cell is multiplied by an odd, position-dependent
     *      weight. Odd numbers are units in Z/2^64, so value -> value*weight is
     *      a bijection that spreads even small values across all 64 bits and
     *      can never annihilate a high-order bit.
     *
     *   2. NON-SEPARABILITY. The running value is combined with modular ADDITION
     *      (not XOR) before each rotation. Because ROTL distributes over XOR but
     *      NOT over addition (carries cross the rotation boundary), the digest
     *      cannot be factored into a XOR of independent per-cell maps. This
     *      removes the generalized-XOR / k-sum structure that an XOR
     *      accumulator would expose.
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
            uint64_t pos = (uint64_t)(x * FIELD_SIZE + y + 1) + (uint64_t)blockIndex * FIELD_SIZE * FIELD_SIZE;

            /* Odd weight => unit in Z/2^64 => bijective, full-width whitening. */
            uint64_t weight = 2u * pos + 1u;

            /* Additive (carry-coupling) combine, then rotate to mix across the
             * word. ADD instead of XOR breaks the separable-XOR closed form. */
            accumulation += field[x][y].value * weight;
            accumulation = (accumulation << 7) | (accumulation >> 57);
        }
    }

    return accumulation;
}
