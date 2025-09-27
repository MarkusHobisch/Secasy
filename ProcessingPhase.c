#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Defines.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

extern Position_t pos;
extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern unsigned long numberOfRounds;
extern int numberOfBits;

static void processData(ColorIndex_t colorIndex, int posX, int posY);

static char* initHashValueBuffer();

static bool isPartialRoundCompleted(int roundCounter, int sizeOfOneIteration);

static void setPositionsToZeroIfOutOfRange(int* posX, int* posY);

static void storeHashValueInBuffer(char* buffer);

char* calculateHashValue()
{
    int posX = pos.x;
    int posY = pos.y;
    const int mask = FIELD_SIZE - 1;
    double iterations = numberOfBits / 64.0;
    int sizeOfOneIteration = (int) ceil(numberOfRounds / iterations + 0.5);
    if (sizeOfOneIteration <= 0) sizeOfOneIteration = 1;

    char* hashValue = initHashValueBuffer();
    size_t writePos = 0;
    size_t capacity = (size_t)numberOfBits;

    int roundCounter;
    for (roundCounter = 0; roundCounter < numberOfRounds; roundCounter++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            for (int j = 0; j < FIELD_SIZE; j++)
            {
                Tile_t* tile = &field[(posX + i) & mask][(posY + j) & mask];
                processData(tile->colorIndex, i, j);
            }
        }
        setPositionsToZeroIfOutOfRange(&posX, &posY);
        if (isPartialRoundCompleted(roundCounter, sizeOfOneIteration))
        {
            LOG_DEBUG("partial segment %d", roundCounter / sizeOfOneIteration);
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
    int numberOfLastRound = roundCounter > 1 ? roundCounter / sizeOfOneIteration + 1 : 1;
    LOG_DEBUG("final partial segment %d", numberOfLastRound);
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

static char* initHashValueBuffer()
{
    char* buffer = (char*)calloc((size_t)numberOfBits, 1);
    if (!buffer)
    {
        LOG_ERROR("Out of memory allocating hash buffer (%d bytes)", numberOfBits);
        exit(EXIT_FAILURE);
    }
    return buffer;
}

static void processData(const ColorIndex_t colorIndex, const int posX, const int posY)
{
    Tile_t* tile = &field[posX][posY];
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
            }
            break;
        }
        case BITWISE_AND:
        {

            if (posX != (FIELD_SIZE - 1))
            {
                Tile_t *neighbourTileRight = &field[posX + 1][posY];
                tile->value &= neighbourTileRight->value;
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
}

static void setPositionsToZeroIfOutOfRange(int* posX, int* posY)
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

static bool isPartialRoundCompleted(const int roundCounter, const int sizeOfOneIteration)
{
    return roundCounter > 0 && roundCounter % sizeOfOneIteration == 0;
}

static void storeHashValueInBuffer(char* buffer)
{
    long long partialHashValue = generateHashValue();
    snprintf(buffer, 64, "%llx", partialHashValue);
}
