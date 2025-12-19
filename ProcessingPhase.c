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
extern int hashLengthInBits;

static void processData(ColorIndex_t colorIndex, uint32_t posX, uint32_t posY);

static void setPositionsToZeroIfOutOfRange(uint32_t* posX, uint32_t* posY);

char* calculateHashValue()
{
    uint32_t posX = pos.x;
    uint32_t posY = pos.y;

    /*
     * `hashLengthInBits` defines the produced hash length in *bits*.
     * Output is returned as hex, so bits/4 hex chars.
     * Enforce minimum 64 bits; CLI already validates power-of-two.
     */
    int effectiveNumberOfBits = hashLengthInBits;
    if (effectiveNumberOfBits < MIN_HASH_OUTPUT_BITS) {
        effectiveNumberOfBits = MIN_HASH_OUTPUT_BITS;
    }

    const size_t outHexChars = (size_t)effectiveNumberOfBits / 4U;
    
    /* Calculate how many 64-bit blocks we need (each block = 16 hex chars) */
    size_t blocksNeeded = (outHexChars + 15) / 16;
    if (blocksNeeded < 1) blocksNeeded = 1;
    
    /* Ensure we have enough rounds to collect all blocks */
    unsigned long minRounds = (unsigned long)blocksNeeded;
    unsigned long actualRounds = numberOfRounds < minRounds ? minRounds : numberOfRounds;
    
    /* Allocate buffer for collecting hash blocks */
    char* hashBuffer = (char*)malloc(blocksNeeded * 16 + 1);
    if (!hashBuffer) {
        LOG_ERROR("Out of memory allocating hash buffer");
        exit(EXIT_FAILURE);
    }
    hashBuffer[0] = '\0';
    
    size_t blocksCollected = 0;
    
    for (unsigned long roundCounter = 0; roundCounter < actualRounds; roundCounter++)
    {
        /* Process field for this round */
        for (uint32_t i = 0; i < FIELD_SIZE; i++)
        {
            for (uint32_t j = 0; j < FIELD_SIZE; j++)
            {
                Tile_t* tile = &field[(posX + i) & (FIELD_SIZE - 1)][(posY + j) & (FIELD_SIZE - 1)];
                processData(tile->colorIndex, i, j);
            }
        }
        setPositionsToZeroIfOutOfRange(&posX, &posY);
        
        /* After each round, collect a 64-bit block if we still need more */
        if (blocksCollected < blocksNeeded) {
            long long hashValue = generateHashValue();
            char block[17];
            snprintf(block, 17, "%016llx", (unsigned long long)hashValue);
            strcat(hashBuffer, block);
            blocksCollected++;
        }
    }
    
    /* Truncate to exact desired length */
    hashBuffer[outHexChars] = '\0';
    
    return hashBuffer;
}

static void processData(const ColorIndex_t colorIndex, const uint32_t posX, const uint32_t posY)
{
    Tile_t* tile = &field[posX][posY];
    
    switch (colorIndex)
    {
        case ADD:
            if (posY == 0)
                tile->value += 1;
            else
                tile->value += field[posX][posY - 1].value;
            break;
        
        case SUB:
            if (posY == (FIELD_SIZE - 1))
                tile->value -= 1;
            else
                tile->value -= field[posX][posY + 1].value;
            break;
        
        case XOR:
            if (posX == 0)
                tile->value ^= 1;
            else
                tile->value ^= field[posX - 1][posY].value;
            break;
        
        case BITWISE_AND:
            if (posX != (FIELD_SIZE - 1))
                tile->value &= field[posX + 1][posY].value;
            break;
        
        case BITWISE_OR:
            if (posX == 0)
                tile->value |= 1;
            else
                tile->value |= field[posX - 1][posY].value;
            break;
        
        case INVERT:
            tile->value = ~tile->value;
            break;
        
        default:
            LOG_ERROR("unknown color index %d", (int)colorIndex);
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
