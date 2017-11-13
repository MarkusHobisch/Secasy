//
// Created by Markus on 10.11.2017.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "BlockOne.h"


int field_[SIZE][SIZE];
int primeIndexes[SIZE][SIZE];
int colorIndexes_[SIZE][SIZE];
int rowSum[SIZE];
int columnSum[SIZE];

int lastPrime = 0;
int colorLen = 5;
int counter = 0;
int primeIndex = 0;
int colorIndex = 0;
int *primeArray;


void readAndProcessFile();

void calcDirections(int ops, char *dir);

int logical_shift_right(int a, int b);

void fillField(char dir[4]);

int nextPrimeNumber();

void writeOnMove(char i);

int fastMod(int param, int i);

long long getFieldSum();

long long getMultipliedSums();

void printField();

void printColorIndexes();

void printSumsAndValues();

long long int getHashValue();


void generateField()
{

    //first check if field_ size is power of 2!
    assert((SIZE & (SIZE - 1)) == 0); // SIZE is not the power of 2!

    if (primeArray == NULL)
        primeArray = generatePrimeNumbers(prime_index_);

    // init field_
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            field_[i][j] = 2;
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

    fseek(filePtr, 0, SEEK_END);
    fileLen = ftell(filePtr);
    rewind(filePtr);

    buffer = (char *) malloc((fileLen + 1) * sizeof(char));
    if (!buffer)
    {
        printf("Could not allocate buffer!");
        return;
    }
    fread(buffer, (size_t) fileLen, 1, filePtr);
    fclose(filePtr);
    printf("File length: %lu bytes\n", fileLen);

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
        fillField(dir);
    }

    // update last position
    field_[pos.x][pos.y] = nextPrimeNumber();
    lastPrime = field_[pos.x][pos.y];
}

void fillField(char dir[4])
{
    //printf("start by pos[%d,%d]\n", pos.x, pos.y);
    for (int i = 0; i < 4; i++)
    {
        writeOnMove(dir[i]);
        counter++;
    }
}

void writeOnMove(char direction)
{
    int oldPrime = field_[pos.x][pos.y];
    int nextPrime = nextPrimeNumber();
    field_[pos.x][pos.y] = nextPrime;
    //printf("%d prime -> new value: %d ", oldPrime, nextPrime);

    int newPosition;
    int diff;
    switch (direction)
    {
        case UP:
            diff = pos.y - oldPrime + SQUARE_AVOIDANCE_FACTOR;
            newPosition = fastMod(fastMod(diff, SIZE) + SIZE, SIZE);

            if (newPosition == pos.y)
            {
                newPosition += 1;
                if (newPosition == SIZE)
                    newPosition = 0;
                if (newPosition < 0)
                    newPosition = SIZE - 1;
            }

            pos.y = newPosition;
            //printf(" UP\n");
            break;
        case DOWN:
            diff = pos.y + oldPrime;
            newPosition = fastMod(fastMod(diff, SIZE) + SIZE, SIZE);

            if (newPosition == pos.y)
            {
                newPosition += 1;
                if (newPosition == SIZE)
                    newPosition = 0;
                if (newPosition < 0)
                    newPosition = SIZE - 1;
            }

            pos.y = newPosition;
            // printf(" DOWN\n");
            break;
        case LEFT:
            diff = pos.x - oldPrime;
            newPosition = fastMod(fastMod(diff, SIZE) + SIZE, SIZE);

            if (newPosition == pos.x)
            {
                newPosition += 1;
                if (newPosition == SIZE)
                    newPosition = 0;
                if (newPosition < 0)
                    newPosition = SIZE - 1;
            }

            pos.x = newPosition;
            // printf(" LEFT\n");
            break;
        case RIGHT:
            diff = pos.x + oldPrime + SQUARE_AVOIDANCE_FACTOR;
            newPosition = fastMod(fastMod(diff, SIZE) + SIZE, SIZE);

            if (newPosition == pos.x)
            {
                newPosition += 1;
                if (newPosition == SIZE)
                    newPosition = 0;
                if (newPosition < 0)
                    newPosition = SIZE - 1;
            }

            pos.x = newPosition;
            // printf(" RIGHT\n");
            break;
        default:
            printf("UNKNOWN POSITION !!\n");
            break;
    }
}

int fastMod(int dividend, int divisor)
{
    return dividend & (divisor - 1);
}

int nextPrimeNumber()
{
    primeIndex = primeIndexes[pos.x][pos.y];
    colorIndex = colorIndexes_[pos.x][pos.y];

    primeIndex = ++primeIndex < primesLength_ ? primeIndex : 0;
    colorIndex = (++colorIndex < colorLen) && primeIndex != 0 ? colorIndex : 0;

    primeIndexes[pos.x][pos.y] = primeIndex;
    colorIndexes_[pos.x][pos.y] = colorIndex;
    return primeArray[primeIndex];
}

void calcDirections(int ops, char *dir)
{
    int index = 0;
    while (ops != 0)
    {
        dir[index++] = (char) (ops & 3); // max ops length is always 8 bits, otherwise assertion will fail!
        ops = logical_shift_right(ops, 2); // is the same as ops >>>= 2 in Java
        // toBinaryString(ops);
        assert(index <= 4 && "index is negative!");
    }
}

int logical_shift_right(int a, int b)
{
    return (int) ((unsigned int) a >> b);
}

void calcSum()
{
    int sum;
    for (int j = 0; j < SIZE; j++)
    {
        sum = 0;
        for (int i = 0; i < SIZE; i++)
        {
            sum += field_[i][j];
        }
        rowSum[j] = sum;
    }

    for (int j = 0; j < SIZE; j++)
    {
        sum = 0;
        for (int i = 0; i < SIZE; i++)
        {
            sum += field_[j][i];
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
            sum += field_[i][j];
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
    printf("-------------- Prime Field ------------\n");
    printf("\n\t Origin matrix - \n");
    printf("last postion: [%d,%d] \n", pos.x, pos.y);
    calcSum();
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%16d ", field_[i][j]);
        }
        printf("sum row: %d\n", rowSum[j]);
    }

    printf("\n\t Transposed matrix\n");
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%16d ", field_[j][i]);
        }
        printf("sum column: %d\n", columnSum[j]);
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
            printf("%4d ", colorIndexes_[i][j]);
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
    printf("- Get multiplied sums: %d\n", getMultipliedSums());
    printf("- Number of iterations: %4d \n", counter);
}

long long getHashValue()
{
    calcSum();
    const long long checksum = getMultipliedSums() ^lastPrime;
    printf("checksum = %lld\n", checksum);

    const long long fieldSum = getFieldSum();
    printf("fieldSum = %lld\n", fieldSum);

    return checksum ^ fieldSum; // hash value

}

