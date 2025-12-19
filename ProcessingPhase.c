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

static char* initHashValueBuffer(size_t desiredCapacity, size_t* outCapacity);

static void setPositionsToZeroIfOutOfRange(uint32_t* posX, uint32_t* posY);

static void storeHashValueInBufferFromBase(uint64_t base, uint64_t tweak, char* buffer);

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
    const size_t outCapacityBytes = outHexChars + 1U;

    size_t capacity = 0;
    char* hashValue = initHashValueBuffer(outCapacityBytes, &capacity);
    size_t writePos = 0;

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
    }

    /* Expand from the final state into as many 128-bit blocks as needed. */
    const uint64_t base = (uint64_t)generateHashValue();
    size_t producedHex = 0;
    for (uint64_t block = 0; producedHex < outHexChars; block++)
    {
        char seg[64];
        storeHashValueInBufferFromBase(base, block, seg);
        const size_t segLen = strlen(seg); /* should be 32 */
        const size_t remaining = outHexChars - producedHex;
        const size_t take = (segLen < remaining) ? segLen : remaining;
        if (writePos + take + 1U <= capacity) {
            memcpy(hashValue + writePos, seg, take);
            writePos += take;
            hashValue[writePos] = '\0';
            producedHex += take;
        } else {
            LOG_ERROR("hash buffer capacity exceeded during expansion (needed %zu, cap %zu)", writePos + take + 1U, capacity);
            break;
        }
    }
    return hashValue;
}

static char* initHashValueBuffer(size_t desiredCapacity, size_t* outCapacity)
{
    size_t capacity = desiredCapacity;

    char* buffer = (char*)calloc(capacity, 1);
    if (!buffer)
    {
        LOG_ERROR("Out of memory allocating hash buffer (%zu bytes)", capacity);
        exit(EXIT_FAILURE);
    }

    if (outCapacity) {
        *outCapacity = capacity;
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


static void storeHashValueInBufferFromBase(uint64_t base, uint64_t tweak, char* buffer)
{
    /* Derive two additional 64-bit accumulators from base via split/mix plus a per-block tweak */
    uint64_t v = (uint64_t)base ^ (mix64(tweak + 0x9e3779b185ebca87ULL));
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
