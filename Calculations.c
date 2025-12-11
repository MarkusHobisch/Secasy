#include "Calculations.h"
#include "Defines.h"
#include <stdint.h>

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern int lastPrime;

static long long calcSumOfField(void);

/* 64-bit mixing function (MurmurHash3-style) */
static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

/* Non-commutative accumulator: order matters! */
static inline uint64_t accumulate(uint64_t acc, uint64_t val, int pos) {
    /* Rotate accumulator by position-dependent amount */
    int rot = (pos * 7 + 3) % 64;
    uint64_t rotated = (acc << rot) | (acc >> (64 - rot));
    /* Mix in the value with position encoding */
    return mix64(rotated ^ val ^ ((uint64_t)(pos + 1) * 0x9E3779B97F4A7C15ULL));
}

long long generateHashValue()
{
    const long long checksum = calcSumOfProducts() ^ lastPrime;
    const long long fieldSum = calcSumOfField();
    return checksum ^ fieldSum;
}

void calcSumOfRows(long long* rowSums)
{
    long long sum = 0;
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            sum += field[i][j].value;
        }
        rowSums[j] = sum;
        sum = 0;
    }
}

void calcSumOfColumns(long long* columnsSums)
{
    long long sum = 0;
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            sum += field[j][i].value;
        }
        columnsSums[j] = sum;
        sum = 0;
    }
}

long long calcSumOfProducts()
{
    long long rowSums[FIELD_SIZE];
    long long columnsSums[FIELD_SIZE];

    calcSumOfRows(rowSums);
    calcSumOfColumns(columnsSums);

    /*
     * FIX v2: Use non-commutative accumulation instead of multiplication.
     * 
     * Problem: product = a * b * c is commutative (order doesn't matter)
     * Solution: Use position-dependent rotation + mixing
     * 
     * This ensures:
     * - [17,17,17,16,16,16,17,17] produces different hash than
     * - [17,17,17,17,16,16,16,17] even though they have same product
     */
    uint64_t accRow = 0x243F6A8885A308D3ULL;  /* Seed: first digits of Pi */
    uint64_t accCol = 0x13198A2E03707344ULL;  /* Seed: more Pi digits */

    for (int i = 0; i < FIELD_SIZE; i++)
    {
        accRow = accumulate(accRow, (uint64_t)rowSums[i], i);
    }

    for (int i = 0; i < FIELD_SIZE; i++)
    {
        accCol = accumulate(accCol, (uint64_t)columnsSums[i], i);
    }
    
    /* Combine the two accumulators */
    return (long long)(mix64(accRow ^ accCol));
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
