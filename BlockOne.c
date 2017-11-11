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
    /*char byte;
    for (int i = 0; i < fileLen; ++i)
    {
        byte = buffer[i];
        if(byte != 0){
            int block = byte & 0xFF; // byte to int conversation
            calcDirections(block, dir);
        }
        else{
            dir.add(0);
            dir.add(0);
            dir.add(0);
            dir.add(0);
        }
        fillField(dir);
        dir.clear();
    }

    //finally
    // update last position
    field[Postition.posX][Postition.posY] = nextPrime();
    lastPrime = field[Postition.posX][Postition.posY];*/
}
