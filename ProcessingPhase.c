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
    const uint32_t mask = FIELD_SIZE - 1U;
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
                Tile_t* tile = &field[(posX + i) & mask][(posY + j) & mask];
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

static inline uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 13;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline int finalize_value(uint32_t x) {
    return (int)(x & 0x7fffffff); /* keep positive */
}

static void processData(const ColorIndex_t colorIndex, const uint32_t posX, const uint32_t posY)
{
    Tile_t* tile = &field[posX][posY];
    uint32_t neighbourSample = 0U; /* will store one neighbour value to fold into post-mix */
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
                neighbourSample = (uint32_t)neighbourTileAbove->value;
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
                neighbourSample = (uint32_t)neighbourTileBelow->value;
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
                neighbourSample = (uint32_t)neighbourTileLeft->value;
            }
            break;
        }
        case BITWISE_AND:
        {

            if (posX != (FIELD_SIZE - 1))
            {
                Tile_t *neighbourTileRight = &field[posX + 1][posY];
                tile->value &= neighbourTileRight->value;
                neighbourSample = (uint32_t)neighbourTileRight->value;
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
                neighbourSample = (uint32_t)neighbourTileLeft->value;
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

    /* increase diffusion: Nonlinear post-mix */
    {
    uint32_t base = (uint32_t)tile->value;
    uint32_t spice = ((uint32_t)colorIndex << 25) ^ ((uint32_t)tile->primeIndex << 11);
    uint32_t v = base ^ spice ^ neighbourSample;
    v = rotl32(v + 0x9E3779B1u, (uint32_t)((colorIndex * 5) + 7) & 31u);
        v ^= (v >> 15);
        v *= 0x85EBCA77u;
        v ^= (v >> 13);
        v = mix32(rotl32(v, 13));
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
    long long partialHashValue = generateHashValue();
    snprintf(buffer, 64, "%llx", partialHashValue);
}
