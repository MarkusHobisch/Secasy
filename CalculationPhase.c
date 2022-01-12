#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "InitializationPhase.h"
#include <string.h>

int colorIndex;

void processData(int colorIndex, int posX, int posY);

void initBuffer(char buffer[]);

char *meltingPot() {
    printf("\n-------------- Processing Data --------------------\n");

    int posX = pos.x;
    int posY = pos.y;
    int iterations = numberOfBits / 64;
    int limit = (int) ceil(numberOfRounds / iterations + 0.5);
    long long hash_val;
    int newLimit = limit;

    char *finalHashValue = calloc(numberOfBits, sizeof (char));
    if (!finalHashValue) {
        printf("Not enough memory!\n");
        return "ERROR";
    }

    initBuffer(finalHashValue);

    for (long k = 0; k < numberOfRounds; k++) {
        // printf("current pos: [%d,%d]\n", posX, posY);
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                colorIndex = colorIndexes[(posX + i) % SIZE][(posY + j) % SIZE];
                // printf("%d : %d\n", (posX + i) % SIZE, (posY + j) % SIZE);
                processData(colorIndex, i, j);
            }
        }
        // printf("Round completed!");

        if (++posX == SIZE) {
            posX = 0;
            if (++posY == SIZE) {
                posY = 0;
                //printf("Completely surrounded once!\n");
            }
        }

        if (k == newLimit) {
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

void initBuffer(char buffer[]) {
    for (int i = 0; i < (numberOfBits + 1); ++i) {
        buffer[i] = 0;
    }
}

void processData(int colorIndex, int posX, int posY) {
    switch (colorIndex) {
        case 0: // add
            if (posY == 0)
                field[posX][posY] += 1;
            else
                field[posX][posY] += field[posX][posY - 1];
            break;
        case 1: // sub
            if (posY == (SIZE - 1))
                field[posX][posY] -= 1;
            else
                field[posX][posY] -= field[posX][posY + 1];
            break;
        case 2: // Xor
            if (posX == 0)
                field[posX][posY] ^= 1;
            else
                field[posX][posY] ^= field[posX - 1][posY];
            break;
        case 3: // |
            if (posX == (SIZE - 1))
                field[posX][posY] |= 1;
            else
                field[posX][posY] |= field[posX + 1][posY];
            break;
        case 4: // |
            if (posX == 0)
                field[posX][posY] |= 1;
            else
                field[posX][posY] |= field[posX - 1][posY];
            break;
        case 5: // ~
            field[posX][posY] = ~field[posX][posY];
            break;
        default:
            printf("function not found! %d\n", colorIndex);
    }
}