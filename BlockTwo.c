//
// Created by Markus on 10.11.2017.
//

#include <stdio.h>
#include "BlockOne.h"

int colorIndex;

void meltingPot();

void processData(int colorIndex, int posX, int posY);

void meltingPot()
{
    printf("-------------- Processing Data ------------\n");
    printf("ROUNDS = %7d \n", rounds_);

    int posX = pos.x;
    int posY = pos.y;
    for (long k = 0; k < rounds_; k++)
    {
        // printf("current pos: [%d,%d]\n", posX, posY);
        for (int i = 0; i < SIZE; i++)
        {
            for (int j = 0; j < SIZE; j++)
            {
                colorIndex = colorIndexes_[(posX + i) % SIZE][(posY + j) % SIZE];
                // printf("%d : %d\n", (posX + i) % SIZE, (posY + j) % SIZE);
                processData(colorIndex, i, j);
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
    }
    calcSum();
}

void processData(int colorIndex, int posX, int posY)
{
    switch (colorIndex)
    {
        case 0: // add
            if (posY == 0)
                field_[posX][posY] += 1;
            else
                field_[posX][posY] += field_[posX][posY - 1];
            break;
        case 1: // sub
            if (posY == (SIZE - 1))
                field_[posX][posY] -= 1;
            else
                field_[posX][posY] -= field_[posX][posY + 1];
            break;
        case 2: // Xor
            if (posX == 0)
                field_[posX][posY] ^= 1;
            else
                field_[posX][posY] ^= field_[posX - 1][posY];
            break;
        case 3: // |
            if (posX == (SIZE - 1))
                field_[posX][posY] |= 1;
            else
                field_[posX][posY] |= field_[posX + 1][posY];
            break;
        case 4: // |
            if (posX == 0)
                field_[posX][posY] |= 1;
            else
                field_[posX][posY] |= field_[posX - 1][posY];
            break;
        case 5: // ~
            field_[posX][posY] = ~field_[posX][posY];
            break;
        default:
            printf("function not found! %d\n", colorIndex);
    }
}