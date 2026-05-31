#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];

uint64_t hashValue(unsigned long blockIndex)
{
    /*
     * Position-dependent accumulation with block-index offset.
     * Each blockIndex produces a distinct 64-bit value from the
     * same grid state by shifting the position weight.
     */
    uint64_t accumulation = 0;

    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            /* Position as unique index, offset by blockIndex */
            uint64_t pos = (uint64_t)(x * FIELD_SIZE + y + 1) + (uint64_t)blockIndex * FIELD_SIZE * FIELD_SIZE;

            /*
             * Force the weight odd. Odd numbers are units in Z/2^64, so
             * value -> value * weight is a bijection and no high-order bit of
             * any cell can be annihilated by the multiplication. Using the
             * even index pos directly would silently discard the top
             * v2(pos) bits of each cell (255 dead state bits in total);
             * 2*pos+1 keeps every weight distinct and invertible.
             */
            uint64_t weight = 2u * pos + 1u;

            /* XOR with position-weighted value */
            accumulation ^= field[x][y].value * weight;

            /* Simple 7-bit rotation */
            accumulation = (accumulation << 7) | (accumulation >> 57);
        }
    }

    return accumulation;
}
