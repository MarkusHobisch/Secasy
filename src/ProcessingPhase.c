#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "Defines.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

extern Position pos;
extern Tile field[FIELD_SIZE][FIELD_SIZE];
extern unsigned long numberOfRounds;
extern int hashLengthInBits;

#define ROUND_CONSTANT 0x9E3779B97F4A7C15ULL

static void processData(ColorIndex colorIndex, uint32_t posX, uint32_t posY);

static void advanceGridPosition(uint32_t *posX, uint32_t *posY);

char *calculateHashValue()
{
    uint32_t posX = pos.x;
    uint32_t posY = pos.y;

    /*
     * `hashLengthInBits` defines the produced hash length in *bits*.
     * Output is returned as hex, so bits/4 hex chars.
     * Enforce minimum 64 bits; CLI already validates power-of-two.
     */
    int effectiveNumberOfBits = hashLengthInBits;
    if (effectiveNumberOfBits < HASH_OUTPUT_BITS)
    {
        effectiveNumberOfBits = HASH_OUTPUT_BITS;
    }

    const size_t outHexChars = (size_t)effectiveNumberOfBits / 4U;

    /* Calculate how many 64-bit blocks we need (each block = 16 hex chars) */
    size_t blocksNeeded = (outHexChars + HASH_HEX_CHARS_PER_BLOCK - 1) / HASH_HEX_CHARS_PER_BLOCK;
    if (blocksNeeded < 1)
        blocksNeeded = 1;

    /* Ensure we have enough rounds to collect all blocks */
    uint64_t minRounds = (uint64_t)blocksNeeded;
    uint64_t actualRounds = numberOfRounds < minRounds ? minRounds : numberOfRounds;

    char *hashBuffer = (char *)malloc(blocksNeeded * HASH_HEX_CHARS_PER_BLOCK + 1);
    if (!hashBuffer)
    {
        LOG_ERROR("Out of memory allocating hash buffer");
        exit(EXIT_FAILURE);
    }
    size_t writePos = 0;

    /* Phase 3: Run all mixing rounds (pure diffusion, no extraction) */

    for (uint64_t roundCounter = 0; roundCounter < actualRounds; roundCounter++)
    {
        /* Per-round key: distinct for every round, so no two rounds share the
         * same mapping (defeats slide/self-similarity attacks). */
        const uint64_t roundKey = ROUND_CONSTANT * (uint64_t)(roundCounter + 1);

        /* Iterate through the whole field */
        for (uint32_t i = 0; i < FIELD_SIZE; i++)
        {
            for (uint32_t j = 0; j < FIELD_SIZE; j++)
            {
                // Intentional cross-position mixing to increase diffusion: read colorIndex from the last position of step 1
                // (init phase) and apply it to the current tile (i, j).
                uint32_t offsetX = (posX + i) & FIELD_SIZE_MASK;
                uint32_t offsetY = (posY + j) & FIELD_SIZE_MASK;
                Tile *tile = &field[offsetX][offsetY];
                processData(tile->colorIndex, i, j);

                /* Inject the round- and position-dependent constant. The
                 * position term (i, j) breaks the grid symmetry that an
                 * all-equal state would otherwise preserve across rounds. */
                field[i][j].value += roundKey + (uint64_t)(i * FIELD_SIZE + j);
            }
        }
        advanceGridPosition(&posX, &posY);
    }

    /* Phase 4: Extract all blocks from the final grid state */
    for (size_t b = 0; b < blocksNeeded; b++)
    {
        uint64_t hash = hashValue((uint64_t)b);
        snprintf(hashBuffer + writePos, HASH_HEX_CHARS_PER_BLOCK + 1, "%016" PRIx64, hash);
        writePos += HASH_HEX_CHARS_PER_BLOCK;
    }

    /* Truncate to exact desired length */
    hashBuffer[outHexChars] = '\0';

    return hashBuffer;
}

static void processData(const ColorIndex colorIndex, const uint32_t posX, const uint32_t posY)
{
    Tile *tile = &field[posX][posY];

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

    case ROTATE_LEFT_XOR:
        if (posX == (FIELD_SIZE - 1))
            tile->value = ROTATE_LEFT_64(tile->value, 13) ^ 1;
        else
            tile->value = ROTATE_LEFT_64(tile->value, 13) ^ field[posX + 1][posY].value;
        break;

    case ROTATE_RIGHT_ADD:
        if (posX == 0)
            tile->value = ROTATE_RIGHT_64(tile->value, 7) + 1;
        else
            tile->value = ROTATE_RIGHT_64(tile->value, 7) + field[posX - 1][posY].value;
        break;

    case INVERT:
        tile->value = ~tile->value;
        break;

    default:
        LOG_ERROR("unknown color index %d", (int)colorIndex);
    }
}

static void advanceGridPosition(uint32_t *posX, uint32_t *posY)
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
