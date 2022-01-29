#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "InitializationPhase.h"
#include <string.h>
#include <stdbool.h>

void processData(int colorIndex, int posX, int posY);

char *initHashValueBuffer();

bool isPartialRoundCompleted(int roundCounter, int sizeOfOneIteration);

void setPositionsToZeroIfOutOfRange(int *posX, int *posY);

char *storeHashValueInBuffer(char *buffer);

void concatenateHashStrings(char *hashValue);

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
                Tile *tile = &field[(posX + i) % SIZE][(posY + j) % SIZE];
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

char *initHashValueBuffer()
{
    char *buffer = calloc(numberOfBits, sizeof(char));
    if (!buffer)
    {
        printf("Not enough memory!\n");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

void processData(int colorIndex, int posX, int posY)
{
    Tile *tile = &field[posX][posY];
    switch (colorIndex)
    {
        case 0: // Add
            if (posY == 0)
                tile->value += 1;
            else
            {
                Tile neighbourTileAbove = field[posX][posY - 1];
                tile->value += neighbourTileAbove.value;
            }
            break;
        case 1: // Sub
            if (posY == (SIZE - 1))
                tile->value -= 1;
            else
            {
                Tile neighbourTileBelow = field[posX][posY + 1];
                tile->value -= neighbourTileBelow.value;
            }
            break;
        case 2: // Xor
            if (posX == 0)
                tile->value ^= 1;
            else
            {
                Tile neighbourTileLeft = field[posX - 1][posY];
                tile->value ^= neighbourTileLeft.value;
            }
            break;
        case 3: // Bitwise AND (&)
            if (posX == (SIZE - 1))
                tile->value |= 1;
            else
            {
                Tile neighbourTileRight = field[posX + 1][posY];
                tile->value |= neighbourTileRight.value;
            }
            break;
        case 4: // Bitwise OR (|)
            if (posX == 0)
                tile->value |= 1;
            else
            {
                Tile neighbourTileLeft = field[posX - 1][posY];
                tile->value |= neighbourTileLeft.value;
            }
            break;
        case 5: // Invert (~)
            tile->value = ~tile->value;
            break;
        default:
        {
            printf("function not found! %d\n", colorIndex);
        }
    }
}

void setPositionsToZeroIfOutOfRange(int *posX, int *posY)
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

bool isPartialRoundCompleted(int roundCounter, int sizeOfOneIteration)
{
    return roundCounter > 0 && roundCounter % sizeOfOneIteration == 0;
}

void concatenateHashStrings(char *hashValue)
{
    char buffer[64];
    storeHashValueInBuffer(buffer);
    strcat(hashValue, buffer);
    printf("%s\n", buffer);
}

char *storeHashValueInBuffer(char *buffer)
{
    long long partialHashValue = generateHashValue();
    sprintf(buffer, "%llx", partialHashValue);
}
