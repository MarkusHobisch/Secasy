#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern int lastPrime;

static long long calcSumOfField(void);

long long generateHashValue()
{
    const long long checksum = calcSumOfProducts() ^ lastPrime;
    const long long fieldSum = calcSumOfField();
    return checksum ^ fieldSum;
}

long long calcSumOfProducts()
{
    /*
     * Simple position-dependent accumulation.
     * The position (x,y) is incorporated directly, making the
     * order of values relevant (non-commutative).
     */
    long long acc = 0;
    
    for (int x = 0; x < FIELD_SIZE; x++)
    {
        for (int y = 0; y < FIELD_SIZE; y++)
        {
            /* Position as unique index */
            long long pos = (long long)(x * FIELD_SIZE + y + 1);
            
            /* XOR with position-weighted value */
            acc ^= field[x][y].value * pos;
            
            /* Simple 7-bit rotation */
            acc = (acc << 7) | ((unsigned long long)acc >> 57);
        }
    }
    
    return acc;
}

static long long calcSumOfField(void)
{
    long long sum = 0;
    for (int i = 0; i < FIELD_SIZE; ++i)
    {
        for (int j = 0; j < FIELD_SIZE; ++j)
        {
            sum += field[i][j].value;
        }
    }
    return sum;
}
