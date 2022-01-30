#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "Defines.h"
#include "ProcessingPhase.h"
#include "Calculations.h"

extern Position_t pos;
extern Tile_t field[SIZE][SIZE];
extern unsigned long numberOfRounds;
extern int numberOfBits;

static void processData(ColorIndex_t colorIndex, int posX, int posY);

static char *initHashValueBuffer();

static bool isPartialRoundCompleted(int roundCounter, int sizeOfOneIteration);

static void setPositionsToZeroIfOutOfRange(int *posX, int *posY);

static void storeHashValueInBuffer(char *buffer);

static void concatenateHashStrings(char *hashValue);

char *calculateHashValue()
{
    printf("\n-------------- Processing Data --------------------\n");

    int posX = pos.x;
    int posY = pos.y;
    double iterations = numberOfBits / 64.0;
    int sizeOfOneIteration = (int) ceil(numberOfRounds / iterations + 0.5);
    char *hashValue = initHashValueBuffer();

    int roundCounter;
    for (roundCounter = 0; roundCounter < numberOfRounds; roundCounter++)
    {
        // printf("current pos: [%d,%d]\n", posX, posY);
        for (int i = 0; i < SIZE; i++)
        {
            for (int j = 0; j < SIZE; j++)
            {
                Tile_t *tile = &field[(posX + i) % SIZE][(posY + j) % SIZE];
                // printf("%d : %d\n", (posX + i) % SIZE, (posY + j) % SIZE);
                processData(tile->colorIndex, i, j);
            }
        }

        setPositionsToZeroIfOutOfRange(&posX, &posY);

        if (isPartialRoundCompleted(roundCounter, sizeOfOneIteration))
        {
            printf("Partial hash value #%d: ", roundCounter / sizeOfOneIteration);
            concatenateHashStrings(hashValue);
        }
    }

    int numberOfLastRound = roundCounter > 1 ? roundCounter / sizeOfOneIteration + 1 : 1;
    printf("Partial hash value #%d: ", numberOfLastRound);
    concatenateHashStrings(hashValue);

    return hashValue;
}

static char *initHashValueBuffer()
{
    char *buffer = calloc(numberOfBits, sizeof(char));
    if (!buffer)
    {
        printf("Not enough memory!\n");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

static void processData(const ColorIndex_t colorIndex, const int posX, const int posY)
{
    Tile_t *tile = &field[posX][posY];
    switch (colorIndex)
    {
        case AND:
        {
            if (posY == 0)
                tile->value += 1;
            else
            {
                Tile_t neighbourTileAbove = field[posX][posY - 1];
                tile->value += neighbourTileAbove.value;
            }
            break;
        }
        case SUB:
        {
            if (posY == (SIZE - 1))
                tile->value -= 1;
            else
            {
                Tile_t neighbourTileBelow = field[posX][posY + 1];
                tile->value -= neighbourTileBelow.value;
            }
            break;
        }
        case XOR:
        {
            if (posX == 0)
                tile->value ^= 1;
            else
            {
                Tile_t neighbourTileLeft = field[posX - 1][posY];
                tile->value ^= neighbourTileLeft.value;
            }
            break;
        }
        case BITWISE_AND: // Bitwise AND (&)
        {
            if (posX == (SIZE - 1))
                tile->value |= 1;
            else
            {
                Tile_t neighbourTileRight = field[posX + 1][posY];
                tile->value |= neighbourTileRight.value;
            }
            break;
        }
        case BITWISE_OR: // Bitwise OR (|)
        {
            if (posX == 0)
                tile->value |= 1;
            else
            {
                Tile_t neighbourTileLeft = field[posX - 1][posY];
                tile->value |= neighbourTileLeft.value;
            }
            break;
        }
        case INVERT: // Invert (~)
        {
            tile->value = ~tile->value;
            break;
        }
        default:
        {
            printf("function not found! %d\n", colorIndex);
        }
    }
}

static void setPositionsToZeroIfOutOfRange(int *posX, int *posY)
{
    if (++*posX == SIZE)
    {
        *posX = 0;
        // highest position at bottom right corner
        if (++*posY == SIZE)
        {
            *posY = 0;
        }
    }
}

static bool isPartialRoundCompleted(const int roundCounter, const int sizeOfOneIteration)
{
    return roundCounter > 0 && roundCounter % sizeOfOneIteration == 0;
}

static void concatenateHashStrings(char *hashValue)
{
    char buffer[64];
    storeHashValueInBuffer(buffer);
    strcat(hashValue, buffer);
    printf("%s\n", buffer);
}

static void storeHashValueInBuffer(char *buffer)
{
    long long partialHashValue = generateHashValue();
    sprintf(buffer, "%llx", partialHashValue);
}
