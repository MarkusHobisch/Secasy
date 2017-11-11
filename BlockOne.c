//
// Created by Markus on 10.11.2017.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "BlockOne.h"
#include <glib.h>


int field[SIZE][SIZE];
int primeIndexes[SIZE][SIZE];
int colorIndexes[SIZE][SIZE];
int rowSum[SIZE];
int columnSum[SIZE];
int lastPrime = 0;
int colorLen = 5;
int counter = 0;
int primeIndex = 0;
int colorIndex = 0;
int *primeArray;


void generateField();

void readAndProcessFile();

void calcDirections(int ops, char *dir);

int logical_shift_right(int a, int b);

void toBinaryString(int n);

void generateField()
{

    //first check if field size is power of 2!
    assert((SIZE & (SIZE - 1)) == 0); // SIZE is not the power of 2!

    if (primeArray == NULL)
        primeArray = generatePrimeNumbers(prime_index_);

    // init field
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            field[i][j] = 2;
        }
    }

    // process file
    readAndProcessFile();
}

void readAndProcessFile()
{
    FILE *filePtr;
    char *buffer;
    long fileLen;

    filePtr = fopen(input_filename_, "rb");  // Open the file in binary mode
    if (!filePtr)
    {
        printf("File could not be opened! Check your path name!\n");
        return;
    }

    fseek(filePtr, 0, SEEK_END);          // Jump to the end of the file
    fileLen = ftell(filePtr);             // Get the current byte offset in the file
    rewind(filePtr);                      // Jump back to the beginning of the file

    buffer = (char *) malloc((fileLen + 1) * sizeof(char)); // Enough memory for file + \0
    fread(buffer, fileLen, 1, filePtr); // Read in the entire file
    fclose(filePtr); // Close the file
    printf("File length: %d bytes\n", fileLen);

    //iterate through buffer
    char dir[4];
    int byte; // must be int!
    for (int i = 0; i < fileLen; ++i)
    {
        byte = buffer[i];
        if (byte != 0)
        {
            int block = byte & 0xFF; // byte to int conversation
            calcDirections(block, dir);
        } else
        {
            dir[0] = 0;
            dir[1] = 0;
            dir[2] = 0;
            dir[3] = 0;
        }
        //fillField(direction_list);
    }

    //finally
    // update last position
    /* field[Postition.posX][Postition.posY] = nextPrime();
     lastPrime = field[Postition.posX][Postition.posY];*/
}

void calcDirections(int ops, char *dir)
{
    int index = 4;
    while (ops != 0)
    {
        dir[--index] = ops & 3;
        ops = logical_shift_right(ops, 2); // is the same as ops >>>= 2 in Java
        // toBinaryString(ops);
        assert(index >= 0);
    }
}

int logical_shift_right(int a, int b)
{
    return (int) ((unsigned int) a >> b);
}

void toBinaryString(int n)
{
    while (n)
    {
        if (n & 1)
            printf("1");
        else
            printf("0");

        n >>= 1;
    }
    printf("\n");
}

