#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];

uint64_t hashValue(void)
{
    /*
     * Simple position-dependent accumulation.
     * The position (x,y) is incorporated directly, making the
     * order of values relevant (non-commutative).
     */
    uint64_t accumulation = 0;

    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            /* Position as unique index */
            uint64_t pos = (uint64_t)(x * FIELD_SIZE + y + 1);

            /* XOR with position-weighted value */
            accumulation ^= field[x][y].value * pos;

            /* Simple 7-bit rotation */
            accumulation = (accumulation << 7) | (accumulation >> 57);
        }
    }

    return accumulation;
}
