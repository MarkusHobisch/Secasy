#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "InitializationPhase.h"

#define ONE_MB 1048576

Tile field[SIZE][SIZE];
int rowSum[SIZE];
int columnSum[SIZE];

int lastPrime = 1;
int colorLen = 5;
int counter = 0;
int primeIndex = 0;
int colorIndex = 0;
int *primeArray;


void readAndProcessFile(char* filename);

void calcDirectionsForFieldTile(int byte, int *dir);

int logicalShiftRight(int a, int b);

void addNumbersToField(int *direction);

int nextPrimeNumber(Tile *tile);

void writeNextNumberOnMove(int direction);

int fastModulus(int dividend, int divisor);

long long getFieldSum();

long long getMultipliedSums();

void clearArray(int direction[4]);

int *initPrimeNumbers();

void initSquareFieldWithDefaultValue(int i);

FILE* readFile(char* filename);

void calcNoDirections(int pInt[4]);

int updateColorAndPrimeIndexOfTile(Tile *tile);

void updateLastPosition();

void initFieldWithDefaultNumbers(unsigned int maxPrimeIndex)
{
    //first check if the size of the field is power of 2!
    assert((SIZE & (SIZE - 1)) == 0);

    initPrimeNumbers(maxPrimeIndex);
    initSquareFieldWithDefaultValue(2);
}

int *initPrimeNumbers(int maxPrimeIndex)
{
    if (primeArray == NULL)
        primeArray = generatePrimeNumbers(maxPrimeIndex);
    return primeArray;
}

void initSquareFieldWithDefaultValue(int defaultValue)
{
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            Tile tile;
            tile.posX = i;
            tile.posY = j;
            tile.primeValue = defaultValue;
            tile.primeIndex = 0;
            tile.colorIndex = 0;
            field[i][j] = tile;
        }
    }
}

void readAndProcessFile(char* filename)
{
    unsigned char buffer[ONE_MB];
    size_t bytesRead;
    int dir[4]; // LEFT, RIGHT, TOP, DOWN
    int intByte;

    FILE *file = readFile(filename);
    while ((bytesRead = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0)
    {
        int byte; // must be int
        for (int i = 0; i < bytesRead; ++i)
        {
            byte = buffer[i];
            if (byte != 0)
            {
                intByte = byte & 0xFF; // byte to int conversation
                calcDirectionsForFieldTile(intByte, dir);
            } else
            {
                calcNoDirections(dir);
            }
            addNumbersToField(dir);
            clearArray(dir);
        }
    }
    updateLastPosition();
    lastPrime = field[pos.x][pos.y].primeValue;
}

FILE* readFile(char* filename)
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
void calcDirectionsForFieldTile(int byte, int *dir)
{
    int index = 0;
    while (byte != 0)
    {
        dir[index++] = (byte & 3); // max byte length is always 8 bits, otherwise assertion will fail.
        byte = logicalShiftRight(byte, 2); // is the same as byte >>>= 2 in Java
        assert(index <= 4 && "index is negative!");
        counter++;
    }

}

int logicalShiftRight(int a, int b)
{
    return (int) ((unsigned int) a >> b);
}

void calcNoDirections(int *dir){
    dir[0] = 0;
    dir[1] = 0;
    dir[2] = 0;
    dir[3] = 0;
}

void addNumbersToField(int *direction)
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
void writeNextNumberOnMove(int direction)
{
    Tile *tile = &field[pos.x][pos.y];
    int oldPrime = tile->primeValue;
    int nextPrime = nextPrimeNumber(tile);
    tile->primeValue = nextPrime;
    if (DEBUG_MODE)
    {
        printf("old prime: %d -> new prime: %d ", oldPrime, nextPrime);
        printf("dir: %d", direction);
    }

    int newPos;
    switch (direction)
    {
        case UP:
            newPos = pos.y - oldPrime + SQUARE_AVOIDANCE_VALUE;
            pos.y = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" UP\n");
            break;
        case DOWN:
            newPos = pos.y + oldPrime;
            pos.y = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" DOWN\n");
            break;
        case LEFT:
            newPos = pos.x - oldPrime;
            pos.x = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" LEFT\n");
            break;
        case RIGHT:
            newPos = pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE;
            pos.x = fastModulus(fastModulus(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" RIGHT\n");
            break;
        default:
            printf("UNKNOWN POSITION !!\n");
            break;
    }
}

void clearArray(int direction[4])
{
    for (int i = 0; i < 4; ++i)
    {
        direction[i] = 0;
    }
}

void updateLastPosition()
{
    Tile *tile = &field[pos.x][pos.y];
    tile->primeValue = nextPrimeNumber(tile);
}

int nextPrimeNumber(Tile *tile)
{
    updateColorAndPrimeIndexOfTile(tile);
    return primeArray[tile->primeIndex];
}

int updateColorAndPrimeIndexOfTile(Tile *tile){
    primeIndex =  tile->primeIndex;
    colorIndex = tile->colorIndex;

    primeIndex = ++primeIndex < numberOfPrimes ? primeIndex : 0;
    colorIndex = (++colorIndex < colorLen) && primeIndex != 0 ? colorIndex : 0;

    tile->primeIndex = primeIndex;
    tile->colorIndex = colorIndex;
}

int fastModulus(int dividend, int divisor)
{
    return dividend & (divisor - 1);
}

void calcSum()
{
    int sum;
    for (int j = 0; j < SIZE; j++)
    {
        sum = 0;
        for (int i = 0; i < SIZE; i++)
        {
            sum += field[i][j].primeValue;
        }
        rowSum[j] = sum;
    }

    for (int j = 0; j < SIZE; j++)
    {
        sum = 0;
        for (int i = 0; i < SIZE; i++)
        {
            sum += field[j][i].primeValue;
        }
        columnSum[j] = sum;
    }
}

long long getFieldSum()
{
    long long sum = 0;
    for (int i = 0; i < SIZE; ++i)
    {
        for (int j = 0; j < SIZE; ++j)
        {
            sum += field[i][j].primeValue;
        }
    }
    return sum;
}

long long getMultipliedSums()
{
    long long muliply1 = 1;
    long long muliply2 = 1;
    for (int i = 0; i < SIZE; i++)
    {
        muliply1 *= rowSum[i];
    }

    for (int i = 0; i < SIZE; i++)
    {
        muliply2 *= columnSum[i];
    }
    return (muliply1 + muliply2);
}

void printField()
{
    printf("\n------------------------------------ Prime Field ---------------------------------------\n");
    printf("\t --------------- Origin matrix - ");
    printf("last position: [%d,%d] ---------------\n", pos.x, pos.y);
    calcSum();
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%12d ", field[i][j].primeValue);
        }
        printf("\t sum row: %d\n", rowSum[j]);
    }

    printf("\n\t --------------- Transposed matrix --------------- \n");
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%12d ", field[j][i].primeValue);
        }
        printf("\t sum column: %d\n", columnSum[j]);
    }
    printf("\n\n");
}

void printColorIndexes()
{
    printf("-------------- ColorIndexes ------------\n");
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            Tile *tile = &field[i][j];
            printf("%4d ", tile->colorIndex);
        }
        printf("\n");
    }
}

void printPrimeIndexes()
{
    printf("-------------- PrimeIndexes ------------\n");
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            Tile *tile = &field[i][j];
            printf("%4d ", tile->primeIndex);
        }
        printf("\n");
    }
}

void printSumsAndValues()
{
    printf("\n");
    printf("- Print row sums: \n");
    calcSum();
    for (int i = 0; i < SIZE; ++i)
    {
        printf("  Row: %d\n", rowSum[i]);
    }
    printf("\n");
    printf("- Print column sums: \n");
    for (int j = 0; j < SIZE; ++j)
    {
        printf("  Column: %d\n", columnSum[j]);
    }
    printf("\n");
    printf("- Last prime was %d\n", lastPrime);
    printf("- Last position was [%d,%d]\n", pos.x, pos.y);
    printf("- Sum of multiplied values is %lli\n", getMultipliedSums());
    printf("- Number of iterations: %4d \n", counter);
}

long long generateHashValue()
{
    calcSum();
    const long long checksum = getMultipliedSums() ^ lastPrime;
    //printf("checksum = %lld\n", checksum);

    const long long fieldSum = getFieldSum();
    //printf("fieldSum = %lld\n", fieldSum);

    return checksum ^ fieldSum; // hash value

}

