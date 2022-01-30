#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "SieveOfEratosthenes.h"

#define ONE_MB 1048576
#define FIRST_PRIME 2

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

// Prevents the formation of squares. Circulating loops (left or right order) lead to identical results and must therefore be avoided
#define SQUARE_AVOIDANCE_VALUE 1

Position_t pos;
Tile_t field[SIZE][SIZE];
int lastPrime = 1;

static int numberOfPrimes = 0;
static int colorLen = 5;
static int primeIndex = 0;
static ColorIndex_t colorIndex = AND;
static int *primeArray;

static void calcAndSetDirections(int byte, int *directions);
static int logicalShiftRight(int a, int b);
static void addNumbersToField(int *direction);
static int nextPrimeNumber(Tile_t *tile);
static void writeNextNumberOnMove(int direction);
static int fastModulus(int dividend, int divisor);
static void clearArray(int direction[4]);
static int *initPrimeNumbers();
static void initSquareFieldWithDefaultValue(int defaultValue);
static FILE *readFile(char *filename);
static void doNotSetAnyDirections(int dir[4]);
static int updateColorAndPrimeIndexOfTile(Tile_t *tile);
static void setPrimeNumberOfLastTile();
static void createTile(int defaultValue, int posX, int posY);

void initFieldWithDefaultNumbers(unsigned int maxPrimeIndex)
{
    //first check if the size of the field is power of 2!
    assert((SIZE & (SIZE - 1)) == 0);

    initPrimeNumbers(maxPrimeIndex);
    initSquareFieldWithDefaultValue(FIRST_PRIME);
}

void readAndProcessFile(char *filename)
{
    unsigned char buffer[ONE_MB];
    size_t bytesRead;
    int directions[4]; // LEFT, RIGHT, TOP, DOWN

    FILE *file = readFile(filename);
    while ((bytesRead = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0)
    {
        int byte; // must be int
        for (int i = 0; i < bytesRead; ++i)
        {
            byte = buffer[i] & 0xFF; // byte to int conversation
            if (byte != 0)
            {
                calcAndSetDirections(byte, directions);
            }
            else
            {
                doNotSetAnyDirections(directions);
            }
            addNumbersToField(directions);
            clearArray(directions);
        }
    }
    setPrimeNumberOfLastTile();
    lastPrime = field[pos.x][pos.y].value;
}

static int *initPrimeNumbers(int maxPrimeIndex)
{
    if (primeArray == NULL)
        primeArray = generatePrimeNumbers(numberOfPrimes, maxPrimeIndex);
    return primeArray;
}

static void initSquareFieldWithDefaultValue(int defaultValue)
{
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            createTile(defaultValue, i, j);
        }
    }
}

static void createTile(int defaultValue, int posX, int posY)
{
    Tile_t tile;
    tile.posX = posX;
    tile.posY = posY;
    tile.value = defaultValue;
    tile.primeIndex = 0;
    tile.colorIndex = AND;
    field[posX][posY] = tile;
}

static FILE *readFile(char *filename)
{
    FILE *file;
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file with filename: %s\n", filename);
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
static void calcAndSetDirections(int byte, int *directions)
{
    int index = 0;
    while (byte != 0)
    {
        directions[index++] = (byte & 3); // max byte length is always 8 bits, otherwise assertion will fail.
        byte = logicalShiftRight(byte, 2); // is the same as byte >>>= 2 in Java
        assert(index <= 4 && "index is negative!");
    }

}

static int logicalShiftRight(int a, int b)
{
    return (int) ((unsigned int) a >> b);
}

static void doNotSetAnyDirections(int *dir)
{
    dir[0] = 0;
    dir[1] = 0;
    dir[2] = 0;
    dir[3] = 0;
}

static void addNumbersToField(int *direction)
{
    if (DEBUG_MODE)
        printf("start by pos[%d,%d]\n", pos.x, pos.y);
    for (int i = 0; i < 4; i++)
    {
        writeNextNumberOnMove(direction[i]);
    }
}

/*
 * This method defines the jump moves. Let's say the direction is TOP and the current tile value is 5. Now we increase
 * the tile value to 7 (because 7 is the next prim value) and jump 5 fields above. If the jumping value is greater than
 * the field range (number of tiles per horizontal or vertical direction) then we start at the opposite direction again
 * (RIGHT -> LEFT, LEFT -> RIGHT, BOTTOM -> UP, UP -> BOTTOM. We realize this with the modulu operand.
 */
static void writeNextNumberOnMove(int direction)
{
    Tile_t *tile = &field[pos.x][pos.y];
    int oldPrime = tile->value;
    int nextPrime = nextPrimeNumber(tile);
    tile->value = nextPrime;
    if (DEBUG_MODE)
    {
        printf("old prime: %d -> new prime: %d ", oldPrime, nextPrime);
        printf("dir: %d", direction);
    }

    int newPos;
    switch (direction)
    {
        case UP:
        {
            newPos = pos.y - oldPrime + SQUARE_AVOIDANCE_VALUE;
            pos.y = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" UP\n");
            break;
        }
        case DOWN:
        {
            newPos = pos.y + oldPrime;
            pos.y = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" DOWN\n");
            break;
        }
        case LEFT:
        {
            newPos = pos.x - oldPrime;
            pos.x = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" LEFT\n");
            break;
        }
        case RIGHT:
        {
            newPos = pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE;
            pos.x = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" RIGHT\n");
            break;
        }
        default:
        {
            printf("UNKNOWN POSITION !!\n");
            break;
        }
    }
}

static void clearArray(int direction[4])
{
    for (int i = 0; i < 4; ++i)
    {
        direction[i] = 0;
    }
}

static void setPrimeNumberOfLastTile()
{
    Tile_t *tile = &field[pos.x][pos.y];
    tile->value = nextPrimeNumber(tile);
}

static int nextPrimeNumber(Tile_t *tile)
{
    updateColorAndPrimeIndexOfTile(tile);
    return primeArray[tile->primeIndex];
}

static int updateColorAndPrimeIndexOfTile(Tile_t *tile)
{
    primeIndex = tile->primeIndex;
    colorIndex = tile->colorIndex;

    primeIndex = ++primeIndex < numberOfPrimes ? primeIndex : 0;
    colorIndex = (++colorIndex < colorLen) && primeIndex != 0 ? colorIndex : 0;

    tile->primeIndex = primeIndex;
    tile->colorIndex = colorIndex;
}

static int fastModulus(int dividend, int divisor)
{
    return dividend & (divisor - 1);
}
