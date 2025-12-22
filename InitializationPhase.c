#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "SieveOfEratosthenes.h"
#include "primes.h"
#include "string.h"
#include "util.h"

// Field size must be power of 2 for bitmask optimization
_Static_assert((FIELD_SIZE & (FIELD_SIZE - 1)) == 0, "FIELD_SIZE must be a power of 2");

/* Directions:
 00 -> up   --> state 0 ; state == value
 11 -> down  --> state 3
 10 -> left  --> state 2
 01 -> right --> state 1
 */
#define UP 0
#define RIGHT 1
#define LEFT 2
#define DOWN 3
#define DIRECTIONS 4

#define FIRST_PRIME 2

// Prevents the formation of squares. Circulating loops (left or right order) lead to identical results and must therefore be avoided
#define SQUARE_AVOIDANCE_VALUE 1

Position_t pos;
Tile_t field[FIELD_SIZE][FIELD_SIZE];
int lastPrime = 1;

static int numberOfPrimes = NUMBER_OF_PRIMES;
static unsigned int colorLen = 5U;
static int primeIndex = 0;
static ColorIndex_t colorIndex = ADD;
static int *primeArray = storedPrimesArray;

static void calcAndSetDirections(int byte);

static int nextPrimeNumber(Tile_t *tile);

static void writeNextNumber(int direction);

static void initPrimeNumbers(unsigned long maxPrimeIndex);

static void initSquareFieldWithDefaultValue(void);

static FILE *readFile(const char *filename);

static void updateColorAndPrimeIndexOfTile(Tile_t *tile);

static void setPrimeNumberOfLastTile(void);

static void createTile(uint32_t posX, uint32_t posY);

void initFieldWithDefaultNumbers(const unsigned long maxPrimeIndex)
{
    // Field size must be at least 8
    assert(FIELD_SIZE >= 8);

    // Reset global state (needed when hashing many buffers in one process, e.g. avalanche test)
    pos.x = 0;
    pos.y = 0;
    lastPrime = FIRST_PRIME;
    primeIndex = 0;
    colorIndex = ADD;

    initPrimeNumbers(maxPrimeIndex);
    initSquareFieldWithDefaultValue();
}

void readAndProcessFile(const char *filename)
{
    /* Allocate large buffer (4MB) on heap to avoid stack overflow on some platforms */
    unsigned char *buffer = (unsigned char *)malloc(DEFAULT_IO_BLOCK_SIZE);
    if (!buffer)
    {
        LOG_ERROR("Failed to allocate %u bytes buffer", (unsigned)DEFAULT_IO_BLOCK_SIZE);
        exit(EXIT_FAILURE);
    }
    size_t bytesRead;

    FILE *file = readFile(filename);
    while ((bytesRead = fread(buffer, 1, DEFAULT_IO_BLOCK_SIZE, file)) > 0)
    {
        int byte; // must be int
        for (size_t i = 0; i < bytesRead; ++i)
        {
            byte = buffer[i] & 0xFF; // byte to int conversion
            calcAndSetDirections(byte);
        }
    }
    if (ferror(file))
    {
        LOG_ERROR("I/O error while reading file '%s'", filename ? filename : "<null>");
        free(buffer);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    free(buffer);
    setPrimeNumberOfLastTile();
    lastPrime = field[pos.x][pos.y].value;
}

// New: process an in-memory buffer (used for avalanche tests)
void processBuffer(const unsigned char *data, size_t len)
{
    if (!data || len == 0)
    {
        return; // treat empty as no-op
    }

    for (size_t i = 0; i < len; ++i)
    {
        int byte = data[i] & 0xFF;
        calcAndSetDirections(byte);
    }
    setPrimeNumberOfLastTile();
    lastPrime = field[pos.x][pos.y].value;
}

static void initPrimeNumbers(const unsigned long maxPrimeIndex)
{
    if (maxPrimeIndex > DEFAULT_MAX_PRIME_INDEX)
    {
        primeArray = generatePrimeNumbers(&numberOfPrimes, maxPrimeIndex);
        if (!primeArray || numberOfPrimes <= 0)
        {
            LOG_ERROR("Prime generation failed for maxPrimeIndex=%lu", maxPrimeIndex);
            exit(EXIT_FAILURE);
        }
    }
}

static void initSquareFieldWithDefaultValue(void)
{
    for (uint32_t i = 0; i < FIELD_SIZE; i++)
    {
        for (uint32_t j = 0; j < FIELD_SIZE; j++)
        {
            createTile(i, j);
        }
    }
}

static void createTile(const uint32_t posX, const uint32_t posY)
{
    if (posX >= FIELD_SIZE || posY >= FIELD_SIZE)
        return;
    Tile_t tile;
    tile.posX = (uint32_t)posX;
    tile.posY = (uint32_t)posY;
    tile.value = FIRST_PRIME;
    tile.primeIndex = 0;
    tile.colorIndex = ADD;
    field[posX][posY] = tile;
}

static FILE *readFile(const char *filename)
{
    if (filename == NULL)
    {
        LOG_ERROR("Input file not provided (-f <file> required)");
        exit(EXIT_FAILURE);
    }
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        LOG_ERROR("Could not open file: %s", filename);
        exit(EXIT_FAILURE);
    }
    return file;
}

/*
 * I think this method needs more explanation. Here we want to calculate the direction for the next move. The possible
 *  directions are LEFT, TOP, RIGHT, DOWN. One Byte has 8 bits and with this 8 bits we only take 2 bits for each round.
 *  So the maximum number of rounds is 4. To be more precisely the number of rounds is always 4.
 *  This is due to the fact that the MSB is always 1, since we only work with positive numbers.
 *
 *  e.g. lets say we have this kind of byte: 11 00 10 01
 *  First round: 01
 *  Second round: 10
 *  Third round: 00
 *  Fourth round: 11
 */
static void calcAndSetDirections(int byte)
{
    writeNextNumber(byte & 3);        // Bits 0-1
    writeNextNumber((byte >> 2) & 3); // Bits 2-3
    writeNextNumber((byte >> 4) & 3); // Bits 4-5
    writeNextNumber((byte >> 6) & 3); // Bits 6-7
}

/*
 * This method defines the jump moves. Let's say the direction is TOP and the current tile value is 5. Now we increase
 * the tile value to 7 (because 7 is the next prim value) and jump 5 fields above. If the jumping value is greater than
 * the field range (number of tiles per horizontal or vertical direction) then we start at the opposite direction again
 * (RIGHT -> LEFT, LEFT -> RIGHT, BOTTOM -> UP, UP -> BOTTOM. We realize this with the modulus operand.
 */
static void writeNextNumber(const int move)
{
    Tile_t *tile = &field[pos.x][pos.y];
    const int oldPrime = tile->value;
    const int nextPrime = nextPrimeNumber(tile);
    tile->value = nextPrime;
#if DEBUG_MODE
    printf("old prime: %d -> new prime: %d dir: %d", oldPrime, nextPrime, move);
#endif
    switch (move)
    {
    case UP:
        // Break commutativity: vertical movement also affects X based on current Y
        pos.y = (pos.y - oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1);
        pos.x = (pos.x + (pos.y >> 1) + 1) & (FIELD_SIZE - 1);
#if DEBUG_MODE
        printf(" UP\n");
#endif
        break;
    case DOWN:
        // Break commutativity: vertical movement also affects X based on current Y
        pos.y = (pos.y + oldPrime) & (FIELD_SIZE - 1);
        pos.x = (pos.x + (pos.y >> 1) + 3) & (FIELD_SIZE - 1);
#if DEBUG_MODE
        printf(" DOWN\n");
#endif
        break;
    case LEFT:
        // Break commutativity: horizontal movement also affects Y based on current X
        pos.x = (pos.x - oldPrime) & (FIELD_SIZE - 1);
        pos.y = (pos.y + (pos.x >> 1) + 2) & (FIELD_SIZE - 1);
#if DEBUG_MODE
        printf(" LEFT\n");
#endif
        break;
    case RIGHT:
        // Break commutativity: horizontal movement also affects Y based on current X
        pos.x = (pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1);
        pos.y = (pos.y + (pos.x >> 1) + 4) & (FIELD_SIZE - 1);
#if DEBUG_MODE
        printf(" RIGHT\n");
#endif
        break;
    default:
        printf("UNKNOWN POSITION !!\n");
        break;
    }
}

static void setPrimeNumberOfLastTile(void)
{
    Tile_t *tile = &field[pos.x][pos.y];
    tile->value = nextPrimeNumber(tile);
}

static int nextPrimeNumber(Tile_t *tile)
{
    updateColorAndPrimeIndexOfTile(tile);
    return primeArray[tile->primeIndex];
}

static void updateColorAndPrimeIndexOfTile(Tile_t *tile)
{
    primeIndex = (int)tile->primeIndex;
    colorIndex = tile->colorIndex;

    primeIndex = ++primeIndex < numberOfPrimes ? primeIndex : 0;
    colorIndex = (primeIndex != 0 && (unsigned int)(colorIndex + 1) < colorLen)
                     ? (ColorIndex_t)(colorIndex + 1)
                     : (ColorIndex_t)0;

    tile->primeIndex = (uint32_t)primeIndex;
    tile->colorIndex = colorIndex;
}
