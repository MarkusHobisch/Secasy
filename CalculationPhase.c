#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "InitializationPhase.h"
#include <string.h>

int colorIndex;

void processData(int colorIndex, int posX, int posY);

void initBuffer(char buffer[]);

char *meltingPot()
{
    printf("\n-------------- Processing Data --------------------\n");

    int posX = pos.x;
    int posY = pos.y;
    int iterations = numberOfBits / 64;
    int limit = (int) ceil(numberOfRounds / iterations + 0.5);
    long long hash_val;
    int newLimit = limit;

    char *finalHashValue = calloc(numberOfBits, sizeof(char));
    if (!finalHashValue)
    {
        printf("Not enough memory!\n");
        return "ERROR";
    }

    initBuffer(finalHashValue);

    for (long k = 0; k < numberOfRounds; k++)
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
        // printf("Round completed!");

        if (++posX == SIZE)
        {
            posX = 0;
            if (++posY == SIZE)
            {
                posY = 0;
                //printf("Completely surrounded once!\n");
            }
        }

        if (k == newLimit)
        {
            hash_val = generateHashValue();
            char buffer[64];
            sprintf(buffer, "%llx", hash_val);
            strcat(finalHashValue, buffer);
            newLimit += limit;
            printf("partial hash value #%d:            %s \n", newLimit / limit - 1, buffer);
        }
    }

    // final
    hash_val = generateHashValue();
    char buffer[64];
    sprintf(buffer, "%llx", hash_val);
    strcat(finalHashValue, buffer);
    printf("partial hash value #%d:            %s \n", newLimit / limit, buffer);

    return finalHashValue;
}

void initBuffer(char buffer[])
{
    for (int i = 0; i < (numberOfBits + 1); ++i)
    {
        buffer[i] = 0;
    }
}

void processData(int colorIndex, int posX, int posY)
{
    Tile *tile = &field[posX][posY];
    switch (colorIndex)
    {
        case 0: // add
            if (posY == 0)
                tile->primeValue += 1;
            else
            {
                Tile neighbourTileAbove = field[posX][posY - 1];
                tile->primeValue += neighbourTileAbove.primeValue;
            }
            break;
        case 1: // sub
            if (posY == (SIZE - 1))
                tile->primeValue -= 1;
            else
            {
                Tile neighbourTileBelow = field[posX][posY + 1];
                tile->primeValue -= neighbourTileBelow.primeValue;
            }
            break;
        case 2: // Xor
            if (posX == 0)
                tile->primeValue ^= 1;
            else
            {
                Tile neighbourTileLeft = field[posX - 1][posY];
                tile->primeValue ^= neighbourTileLeft.primeValue;
            }
            break;
        case 3: // |
            if (posX == (SIZE - 1))
                tile->primeValue |= 1;
            else
            {
                Tile neighbourTileRight = field[posX + 1][posY];
                tile->primeValue |= neighbourTileRight.primeValue;
            }
            break;
        case 4: // |
            if (posX == 0)
                tile->primeValue |= 1;
            else
            {
                Tile neighbourTileLeft = field[posX - 1][posY];
                tile->primeValue |= neighbourTileLeft.primeValue;
            }
            break;
        case 5: // ~
            tile->primeValue = ~tile->primeValue;
            break;
        default:
        {
            printf("function not found! %d\n", colorIndex);
        }
    }
}