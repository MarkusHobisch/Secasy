#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "SieveOfEratosthenes.h"
#include "primes.h"
#include "string.h"
#include "util.h"
/* Mirror all printf output to debug.txt when g_debug_fp is set */
#define printf debug_tee_printf

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
static int primeIndex = 0;
static ColorIndex_t colorIndex = ADD;
static int *primeArray = storedPrimesArray;
static int primeArrayDynamic = 0;

/* Path recording: store each step of the byte walk for the grid overlay */
#define MAX_PATH_STEPS 4096
typedef struct
{
    uint32_t fromX, fromY;
    uint32_t toX, toY;
    int direction; /* UP=0 RIGHT=1 LEFT=2 DOWN=3 */
    uint64_t oldPrime;
    int newPrime;
} PathStep_t;

static PathStep_t g_pathSteps[MAX_PATH_STEPS];
static int g_pathStepCount = 0;

static void processByteDirections(int byte);

static int nextPrimeNumber(Tile_t *tile, int direction);

static void processDirectionStep(int direction);

static void initPrimeNumbers(unsigned long maxPrimeIndex);

static void initSquareFieldWithDefaultValue(void);

static FILE *readFile(const char *filename);

static void updateColorAndPrimeIndexOfTile(Tile_t *tile, int direction);

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

    g_pathStepCount = 0;

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
            byte = buffer[i] & 0xFF;
            processByteDirections(byte);
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
    lastPrime = (int)field[pos.x][pos.y].value;
    if (g_debug_mode)
        printf("  +--------------------------------------------------------+\n\n");
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
        processByteDirections(byte);
    }
    setPrimeNumberOfLastTile();
    lastPrime = (int)field[pos.x][pos.y].value;
    if (g_debug_mode)
        printf("  +--------------------------------------------------------+\n\n");
}

static void initPrimeNumbers(const unsigned long maxPrimeIndex)
{
    /* Free previously generated primes if called again (e.g. avalanche tests) */
    if (primeArrayDynamic && primeArray)
    {
        free(primeArray);
        primeArray = NULL;
        primeArrayDynamic = 0;
    }

    if (maxPrimeIndex > DEFAULT_MAX_PRIME_INDEX)
    {
        primeArray = generatePrimeNumbers(&numberOfPrimes, maxPrimeIndex);
        if (!primeArray || numberOfPrimes <= 0)
        {
            LOG_ERROR("Prime generation failed for maxPrimeIndex=%lu", maxPrimeIndex);
            exit(EXIT_FAILURE);
        }
        primeArrayDynamic = 1;
    }
    else
    {
        primeArray = storedPrimesArray;
        numberOfPrimes = NUMBER_OF_PRIMES;
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
 * Decompose a single byte into four 2-bit direction codes and execute each step.
 * Bits are consumed LSB-first:
 *   Bits 0-1  -> step 1
 *   Bits 2-3  -> step 2
 *   Bits 4-5  -> step 3
 *   Bits 6-7  -> step 4
 *
 * Example: byte 0xC9 = 11 00 10 01
 *   Step 1: 01 (RIGHT), Step 2: 10 (LEFT), Step 3: 00 (UP), Step 4: 11 (DOWN)
 */
static void processByteDirections(int byte)
{
    if (g_debug_mode)
    {
        static size_t byteCounter = 0;
        if (byteCounter == 0)
        {
            printf("\n  +--------------------------------------------------------+\n");
            printf("  |  Init Phase  (Byte Walk)                               |\n");
            printf("  +--------------------------------------------------------+\n");
        }
        char ch = (byte >= 0x20 && byte <= 0x7E) ? (char)byte : '.';
        printf("  |  Byte %3zu  '%c' (0x%02X = %d%d%d%d%d%d%d%d):                      |\n",
               byteCounter++, ch, (unsigned char)byte,
               (byte >> 7) & 1, (byte >> 6) & 1, (byte >> 5) & 1, (byte >> 4) & 1,
               (byte >> 3) & 1, (byte >> 2) & 1, (byte >> 1) & 1, byte & 1);
        printf("  |   From       OldPrime      NewPrime  Dir       To      |\n");
    }
    processDirectionStep((byte >> (0 * BITS_PER_DIRECTION)) & DIRECTION_MASK);
    processDirectionStep((byte >> (1 * BITS_PER_DIRECTION)) & DIRECTION_MASK);
    processDirectionStep((byte >> (2 * BITS_PER_DIRECTION)) & DIRECTION_MASK);
    processDirectionStep((byte >> (3 * BITS_PER_DIRECTION)) & DIRECTION_MASK);
}

/*
 * Execute a single direction step: write the next prime to the current tile,
 * then jump by the old tile value in the given direction (with wraparound).
 * SAV (Square Avoidance Value) is added to DOWN and RIGHT jumps.
 */
static void processDirectionStep(const int direction)
{
    Tile_t *tile = &field[pos.x][pos.y];
    const uint64_t oldPrime = tile->value;
    const int nextPrime = nextPrimeNumber(tile, direction);
    tile->value = (uint64_t)nextPrime;
    const uint32_t fromX = pos.x, fromY = pos.y;
    switch (direction)
    {
    case UP:
        pos.y = (pos.y - oldPrime) & FIELD_SIZE_MASK;
        break;
    case DOWN:
        pos.y = (pos.y + oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
        break;
    case LEFT:
        pos.x = (pos.x - oldPrime) & FIELD_SIZE_MASK;
        break;
    case RIGHT:
        pos.x = (pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
        break;
    default:
        printf("UNKNOWN POSITION !!\n");
        return;
    }
    if (g_debug_mode)
    {
        static const char *dirName[] = {"UP   ", "RIGHT", "LEFT ", "DOWN "};
        printf("  |  [%2u,%2u]  %10" PRIu64 " -> %10d  %s  -> [%2u,%2u]  |\n",
               fromX, fromY, oldPrime, nextPrime,
               direction < DIRECTIONS_PER_BYTE ? dirName[direction] : "?????",
               pos.x, pos.y);
               
        if (g_pathStepCount < MAX_PATH_STEPS)
        {
            g_pathSteps[g_pathStepCount].fromX = fromX;
            g_pathSteps[g_pathStepCount].fromY = fromY;
            g_pathSteps[g_pathStepCount].toX = pos.x;
            g_pathSteps[g_pathStepCount].toY = pos.y;
            g_pathSteps[g_pathStepCount].direction = direction;
            g_pathSteps[g_pathStepCount].oldPrime = oldPrime;
            g_pathSteps[g_pathStepCount].newPrime = nextPrime;
            g_pathStepCount++;
        }
    }
}

static void setPrimeNumberOfLastTile(void)
{
    Tile_t *tile = &field[pos.x][pos.y];
    tile->value = (uint64_t)nextPrimeNumber(tile, 0);
}

static int nextPrimeNumber(Tile_t *tile, const int direction)
{
    updateColorAndPrimeIndexOfTile(tile, direction);
    return primeArray[tile->primeIndex];
}

static void updateColorAndPrimeIndexOfTile(Tile_t *tile, const int direction)
{
    primeIndex = (int)tile->primeIndex;
    colorIndex = tile->colorIndex;

    primeIndex = (primeIndex + 1 + direction) % numberOfPrimes;
    colorIndex = (ColorIndex_t)((colorIndex + 1) % NUM_COLOR_OPERATIONS);

    tile->primeIndex = (uint32_t)primeIndex;
    tile->colorIndex = colorIndex;
}

int getPathStepCount(void) { return g_pathStepCount; }

void getPathStep(int idx, uint32_t *fX, uint32_t *fY, uint32_t *tX, uint32_t *tY, int *dir)
{
    if (idx >= 0 && idx < g_pathStepCount)
    {
        *fX = g_pathSteps[idx].fromX;
        *fY = g_pathSteps[idx].fromY;
        *tX = g_pathSteps[idx].toX;
        *tY = g_pathSteps[idx].toY;
        *dir = g_pathSteps[idx].direction;
    }
}
