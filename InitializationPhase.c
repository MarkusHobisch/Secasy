#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "SieveOfEratosthenes.h"
#include "primes.h"
#include "string.h"

#define ONE_MB 1048576

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
static int colorLen = 5;
static int primeIndex = 0;
static ColorIndex_t colorIndex = AND;
static int* primeArray = storedPrimesArray;

static void calcAndSetDirections(int byte, int* directions);

static int logicalShiftRight(int a, int b);

static void addNumbersToField(const int* directions);

static int nextPrimeNumber(Tile_t* tile);

static void writeNextNumberOnMove(int direction);

static void initPrimeNumbers(unsigned long maxPrimeIndex);

static void initSquareFieldWithDefaultValue();

static FILE* readFile(const char* filename);

static void doNotSetAnyDirections(int* directions);

static void updateColorAndPrimeIndexOfTile(Tile_t* tile);

static void setPrimeNumberOfLastTile();

static void createTile(int posX, int posY);

void initFieldWithDefaultNumbers(const unsigned long maxPrimeIndex)
{
    //first check if the size of the field is power of 2!
    assert((FIELD_SIZE & (FIELD_SIZE - 1)) == 0);

    initPrimeNumbers(maxPrimeIndex);
    initSquareFieldWithDefaultValue();
}

void readAndProcessFile(const char* filename)
{
    unsigned char buffer[ONE_MB];
    size_t bytesRead;
    int directions[DIRECTIONS]; // LEFT, RIGHT, TOP, DOWN

    FILE* file = readFile(filename);
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
        }
    }

    fclose(file);
    setPrimeNumberOfLastTile();
    lastPrime = field[pos.x][pos.y].value;
}

static void initPrimeNumbers(const unsigned long maxPrimeIndex)
{
    if (maxPrimeIndex > DEFAULT_MAX_PRIME_INDEX)
    {
        primeArray = generatePrimeNumbers(&numberOfPrimes, maxPrimeIndex);
    }
}

static void initSquareFieldWithDefaultValue()
{
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            createTile(i, j);
        }
    }
}

static void createTile(const int posX, const int posY)
{
    if (posX >= FIELD_SIZE || posY >= FIELD_SIZE)
        return;
    Tile_t tile;
    tile.posX = posX;
    tile.posY = posY;
    tile.value = FIRST_PRIME;
    tile.primeIndex = 0;
    tile.colorIndex = AND;
    field[posX][posY] = tile;
}

static FILE* readFile(const char* filename)
{
    if(filename == NULL){
        fprintf(stderr, "<Input Error> File not found. Providing a file is mandatory. You can provide a file with option -f <inputFile>.\n Use option -h to see all supported usage arguments. \n");
        exit(EXIT_FAILURE); 
    }

    FILE* file;
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
static void calcAndSetDirections(int byte, int* directions)
{
    int index = 0;
    while (byte != 0)
    {
        directions[index++] = (byte & 3); // max byte length is always 8 bits, otherwise assertion will fail.
        byte = logicalShiftRight(byte, 2); // is the same as byte >>>= 2 in Java
        assert(index <= 4 && "index is negative!");
    }

}

static int logicalShiftRight(const int a, const int b)
{
    return (int) ((unsigned int) a >> b);
}

static void doNotSetAnyDirections(int* directions)
{
   memset(directions, 0, DIRECTIONS * sizeof(int));
}

static void addNumbersToField(const int* directions)
{
#if DEBUG_MODE
    printf("start by pos[%d,%d]\n", pos.x, pos.y);
#endif
    for (int i = 0; i < DIRECTIONS; i++)
    {
        writeNextNumberOnMove(directions[i]);
    }
}

/*
 * This method defines the jump moves. Let's say the direction is TOP and the current tile value is 5. Now we increase
 * the tile value to 7 (because 7 is the next prim value) and jump 5 fields above. If the jumping value is greater than
 * the field range (number of tiles per horizontal or vertical direction) then we start at the opposite direction again
 * (RIGHT -> LEFT, LEFT -> RIGHT, BOTTOM -> UP, UP -> BOTTOM. We realize this with the modulus operand.
 */
static void writeNextNumberOnMove(const int move)
{
    Tile_t* tile = &field[pos.x][pos.y];
    int oldPrime = tile->value;
    int nextPrime = nextPrimeNumber(tile);
    tile->value = nextPrime;
#if DEBUG_MODE
    printf("old prime: %d -> new prime: %d ", oldPrime, nextPrime);
    printf("dir: %d", move);
#endif

    int newPos;
    switch (move)
    {
        case UP:
        {
            // calculates new position and keeps it in range via bit mask (FIELD_SIZE - 1)
            pos.y = (pos.y - oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1);
#if DEBUG_MODE
            printf(" UP\n");
#endif
            break;
        }
        case DOWN:
        {
            pos.y = (pos.y + oldPrime) & (FIELD_SIZE - 1);
#if DEBUG_MODE
            printf(" DOWN\n");
#endif
            break;
        }
        case LEFT:
        {
            pos.x = (pos.x - oldPrime) & (FIELD_SIZE - 1);
#if DEBUG_MODE
            printf(" LEFT\n");
#endif
            break;
        }
        case RIGHT:
        {
            pos.x = (pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1);
#if DEBUG_MODE
            printf(" RIGHT\n");
#endif
            break;
        }
        default:
        {
            printf("UNKNOWN POSITION !!\n");
            break;
        }
    }
}

static void setPrimeNumberOfLastTile()
{
    Tile_t* tile = &field[pos.x][pos.y];
    tile->value = nextPrimeNumber(tile);
}

static int nextPrimeNumber(Tile_t* tile)
{
    updateColorAndPrimeIndexOfTile(tile);
    return primeArray[tile->primeIndex];
}

static void updateColorAndPrimeIndexOfTile(Tile_t* tile)
{
    primeIndex = tile->primeIndex;
    colorIndex = tile->colorIndex;

    primeIndex = ++primeIndex < numberOfPrimes ? primeIndex : 0;
    colorIndex = (++colorIndex < colorLen) && primeIndex != 0 ? colorIndex : 0;

    tile->primeIndex = primeIndex;
    tile->colorIndex = colorIndex;
}
