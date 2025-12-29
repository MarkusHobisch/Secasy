#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include "Calculations.h"
#include "Defines.h"
#include "Printing.h"

extern Position_t pos;
extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern int lastPrime;

void printField()
{
    printf("\n------------------------------------ Prime Field ---------------------------------------\n\t --------------- Origin matrix - last position: [%u,%u] ---------------\n", pos.x, pos.y);

    for (int j = 0; j < FIELD_SIZE; j++)
    {
        long long rowSum = 0;
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            printf("%16" PRIu64 " ", (uint64_t)field[i][j].value);
            rowSum += field[i][j].value;
        }
        printf("\t sum row: %" PRId64 "\n", (int64_t)rowSum);
    }

    printf("\n\t --------------- Transposed matrix --------------- \n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        long long colSum = 0;
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            printf("%16" PRIu64 " ", (uint64_t)field[j][i].value);
            colSum += field[j][i].value;
        }
        printf("\t sum column: %" PRId64 "\n", (int64_t)colSum);
    }
    printf("\n\n");
}

void printColorIndexes()
{
    printf("-------------- ColorIndexes ------------\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            printf("%4d ", field[i][j].colorIndex);
        }
        printf("\n");
    }
}

void printPrimeIndexes()
{
    printf("-------------- PrimeIndexes ------------\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            printf("%4d ", field[i][j].primeIndex);
        }
        printf("\n");
    }
}

void printSumsAndValues()
{
    printf("\n");
    printf("- Print row sums: \n");

    for (int j = 0; j < FIELD_SIZE; ++j)
    {
        long long rowSum = 0;
        for (int i = 0; i < FIELD_SIZE; ++i)
        {
            rowSum += field[i][j].value;
        }
        printf("  Row: %" PRId64 "\n", (int64_t)rowSum);
    }
    printf("\n");
    printf("- Print column sums: \n");
    for (int j = 0; j < FIELD_SIZE; ++j)
    {
        long long colSum = 0;
        for (int i = 0; i < FIELD_SIZE; ++i)
        {
            colSum += field[j][i].value;
        }
        printf("  Column: %" PRId64 "\n", (int64_t)colSum);
    }
    printf("\n");
    printf("- Last prime was %d\n", lastPrime);
    printf("- Last position was [%u,%u]\n", pos.x, pos.y);
    printf("- Hash value is %lli\n", hashValue());
}

void printDatatypeMaxValues()
{
    printf("\n\n**************/////// MAX VALUES OF DATATYPES ///////**************\n");
    printf("+ LONG_MAX                 %ld\n", LONG_MAX);
    printf("+ INT_MAX                  %i\n", INT_MAX);
    printf("+ LONG_LONG_MAX            %lld\n", (long long)LLONG_MAX); // corrected format for signed long long
    printf("*******************************************************************\n\n");
}
