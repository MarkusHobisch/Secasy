#include <stdio.h>
#include <limits.h>
#include "Calculations.h"
#include "Defines.h"
#include "Printing.h"

extern Position_t pos;
extern Tile_t field[SIZE][SIZE];
extern int lastPrime;

void printField()
{
    printf("\n------------------------------------ Prime Field ---------------------------------------\n");
    printf("\t --------------- Origin matrix - ");
    printf("last position: [%d,%d] ---------------\n", pos.x, pos.y);

    int rowSums[SIZE];
    int columnsSums[SIZE];

    calcSumOfRows(rowSums);
    calcSumOfColumns(columnsSums);

    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%12d ", field[i][j].value);
        }
        printf("\t sum row: %d\n", rowSums[j]);
    }

    printf("\n\t --------------- Transposed matrix --------------- \n");
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            printf("%12d ", field[j][i].value);
        }
        printf("\t sum column: %d\n", columnsSums[j]);
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
            printf("%4d ", field[i][j].colorIndex);
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
            printf("%4d ", field[i][j].primeIndex);
        }
        printf("\n");
    }
}

void printSumsAndValues()
{
    printf("\n");
    printf("- Print row sums: \n");

    int rowSums[SIZE];
    int columnsSums[SIZE];

    calcSumOfRows(rowSums);
    calcSumOfColumns(columnsSums);

    for (int i = 0; i < SIZE; ++i)
    {
        printf("  Row: %d\n", rowSums[i]);
    }
    printf("\n");
    printf("- Print column sums: \n");
    for (int j = 0; j < SIZE; ++j)
    {
        printf("  Column: %d\n", columnsSums[j]);
    }
    printf("\n");
    printf("- Last prime was %d\n", lastPrime);
    printf("- Last position was [%d,%d]\n", pos.x, pos.y);
    printf("- Sum of multiplied values is %lli\n", calcSumOfProducts());
}

void printDatatypeMaxValues()
{
    printf("\n\n**************/////// MAX VALUES OF DATATYPES ///////**************\n");
    printf("+ LONG_MAX                 %ld\n", LONG_MAX);
    printf("+ INT_MAX                  %i\n", INT_MAX);
    printf("+ LONG_LONG_MAX            %llu\n", LLONG_MAX);
    printf("*******************************************************************\n\n");
}
