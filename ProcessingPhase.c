#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "Defines.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

extern Position_t pos;
extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern unsigned long numberOfRounds;
extern int numberOfBits;

static void processData(ColorIndex_t colorIndex, uint32_t posX, uint32_t posY);

static char* initHashValueBuffer(void);

static bool isPartialRoundCompleted(unsigned long roundCounter, int sizeOfOneIteration);

static void setPositionsToZeroIfOutOfRange(uint32_t* posX, uint32_t* posY);

static void storeHashValueInBuffer(char* buffer);

char* calculateHashValue()
{
    uint32_t posX = pos.x;
    uint32_t posY = pos.y;
    double iterations = numberOfBits / 64.0;
    int sizeOfOneIteration = (int) ceil(numberOfRounds / iterations + 0.5);
    if (sizeOfOneIteration <= 0) sizeOfOneIteration = 1;

    char* hashValue = initHashValueBuffer();
    size_t writePos = 0;
    size_t capacity = (size_t)numberOfBits;

    unsigned long roundCounter;
    for (roundCounter = 0; roundCounter < numberOfRounds; roundCounter++)
    {
        for (uint32_t i = 0; i < FIELD_SIZE; i++)
        {
            for (uint32_t j = 0; j < FIELD_SIZE; j++)
            {
                Tile_t* tile = &field[(posX + i) & (FIELD_SIZE - 1)][(posY + j) & (FIELD_SIZE - 1)];
                processData(tile->colorIndex, i, j);
            }
        }
        setPositionsToZeroIfOutOfRange(&posX, &posY);
        if (isPartialRoundCompleted(roundCounter, sizeOfOneIteration))
        {
            LOG_DEBUG("partial segment %lu", roundCounter / (unsigned long)sizeOfOneIteration);
            char segment[64];
            storeHashValueInBuffer(segment);
            size_t segLen = strlen(segment);
            if (writePos + segLen + 1 < capacity) {
                memcpy(hashValue + writePos, segment, segLen + 1);
                writePos += segLen;
            } else {
                LOG_ERROR("hash buffer capacity exceeded (needed %zu, cap %zu)", writePos + segLen + 1, capacity);
                break;
            }
        }
    }
    unsigned long numberOfLastRound = roundCounter > 1 ? roundCounter / (unsigned long)sizeOfOneIteration + 1UL : 1UL;
    LOG_DEBUG("final partial segment %lu", numberOfLastRound);
    char finalSeg[64];
    storeHashValueInBuffer(finalSeg);
    size_t fLen = strlen(finalSeg);
    if (writePos + fLen + 1 < capacity) {
        memcpy(hashValue + writePos, finalSeg, fLen + 1);
        writePos += fLen;
    } else {
        LOG_ERROR("hash buffer capacity exceeded on final segment (needed %zu, cap %zu)", writePos + fLen + 1, capacity);
    }
    return hashValue;
}

static char* initHashValueBuffer(void)
{
    char* buffer = (char*)calloc((size_t)numberOfBits, 1);
    if (!buffer)
    {
        LOG_ERROR("Out of memory allocating hash buffer (%d bytes)", numberOfBits);
        exit(EXIT_FAILURE);
    }
    return buffer;
}

static inline uint32_t rotl32(uint32_t x, uint32_t r) {
    return (uint32_t)((x << r) | (x >> (32 - r)));
}

static inline uint64_t rotl64(uint64_t x, uint64_t r) {
    return (uint64_t)((x << r) | (x >> (64 - r)));
}

static inline uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 13;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static inline uint32_t fmix32(uint32_t h) {
    h ^= h >> 16; h *= 0x7feb352dU;
    h ^= h >> 13; h *= 0x846ca68bU;
    h ^= h >> 16; return h;
}

static inline uint64_t finalize_value(uint64_t x) {
    /* map into a better distributed 64-bit domain using 64-bit mixing */
    return mix64(x);
}

static void processData(const ColorIndex_t colorIndex, const uint32_t posX, const uint32_t posY)
{
    Tile_t* tile = &field[posX][posY];
    uint64_t neighbourSample = 0ULL; /* will store one neighbour value to fold into post-mix */
    switch (colorIndex)
    {
        case ADD:
        {
            if (posY == 0)
                tile->value += 1;
            else
            {
                Tile_t *neighbourTileAbove = &field[posX][posY - 1];
                tile->value += neighbourTileAbove->value;
                neighbourSample = (uint64_t)neighbourTileAbove->value;
            }
            break;
        }
        case SUB:
        {
            if (posY == (FIELD_SIZE - 1))
                tile->value -= 1;
            else
            {
                Tile_t *neighbourTileBelow = &field[posX][posY + 1];
                tile->value -= neighbourTileBelow->value;
                neighbourSample = (uint64_t)neighbourTileBelow->value;
            }
            break;
        }
        case XOR:
        {
            if (posX == 0)
                tile->value ^= 1;
            else
            {
                Tile_t *neighbourTileLeft = &field[posX - 1][posY];
                tile->value ^= neighbourTileLeft->value;
                neighbourSample = (uint64_t)neighbourTileLeft->value;
            }
            break;
        }
        case BITWISE_AND:
        {

            if (posX != (FIELD_SIZE - 1))
            {
                Tile_t *neighbourTileRight = &field[posX + 1][posY];
                tile->value &= neighbourTileRight->value;
                neighbourSample = (uint64_t)neighbourTileRight->value;
            }
            break;
        }
        case BITWISE_OR:
        {
            if (posX == 0)
                tile->value |= 1;
            else
            {
                Tile_t *neighbourTileLeft = &field[posX - 1][posY];
                tile->value |= neighbourTileLeft->value;
                neighbourSample = (uint64_t)neighbourTileLeft->value;
            }
            break;
        }
        case INVERT:
        {
            tile->value = ~tile->value;
            break;
        }
        default:
        {
            LOG_ERROR("unknown color index %d", (int)colorIndex);
        }
    }

    /* increase diffusion: Nonlinear post-mix (64-bit) */
    {
    uint64_t base = (uint64_t)tile->value;
    uint64_t spice = ((uint64_t)colorIndex << 53) ^ ((uint64_t)tile->primeIndex << 27);
    uint64_t v = base ^ spice ^ (uint64_t)neighbourSample;
    v = rotl64(v + 0x9E3779B97F4A7C15ULL, (uint64_t)((colorIndex * 7) + 13) & 63u);
        v ^= (v >> 33);
        v *= 0xff51afd7ed558ccdULL;
        v ^= (v >> 29);
        v = mix64(rotl64(v, 31));
        tile->value = finalize_value(v);
    }
}

static void setPositionsToZeroIfOutOfRange(uint32_t* posX, uint32_t* posY)
{
    if (++*posX == FIELD_SIZE)
    {
        *posX = 0;
        if (++*posY == FIELD_SIZE)
        {
            *posY = 0;
        }
    }
}

static bool isPartialRoundCompleted(const unsigned long roundCounter, const int sizeOfOneIteration)
{
    return roundCounter > 0 && roundCounter % (unsigned long)sizeOfOneIteration == 0UL;
}

static void storeHashValueInBuffer(char* buffer)
{
    /* Base value */
    long long base = generateHashValue();
    /* Derive two additional 64-bit accumulators from base via split/mix */
    uint64_t v = (uint64_t)base;
    uint64_t a = v + 0x9e3779b185ebca87ULL;
    uint64_t b = v ^ 0xc2b2ae3d27d4eb4fULL;
    /* 64-bit fmix similar to Murmur final */
    #define FMIX64(x) do { \
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; \
        x ^= x >> 27; x *= 0x94d049bb133111ebULL; \
        x ^= x >> 31; } while(0)
    FMIX64(v); FMIX64(a); FMIX64(b);
    uint64_t c = v ^ rotl32((uint32_t)a, 13) ^ (a + (b<<1));
    uint64_t d = a ^ (b + 0x632be59bd9b4e019ULL) ^ (v>>1);
    FMIX64(c); FMIX64(d);
    /* Combine into 128 bits (c||d) and format with leading zeros to 32 hex chars */
    /* (reduces structure in the highest nibble) */
    snprintf(buffer, 64, "%016llx%016llx", (unsigned long long)c, (unsigned long long)d);
}
