#include "Calculations.h"
#include "Defines.h"

extern Tile_t field[SIZE][SIZE];
extern int lastPrime;

static long long calcSumOfField();

long long generateHashValue()
{
    const long long checksum = calcSumOfProducts() ^ lastPrime;
    const long long fieldSum = calcSumOfField();
    return checksum ^ fieldSum;
}

void calcSumOfRows(int* rowSums)
{
    int sum = 0;
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            sum += field[i][j].value;
        }
        rowSums[j] = sum;
        sum = 0;
    }
}

void calcSumOfColumns(int* columnsSums)
{
    int sum = 0;
    for (int j = 0; j < SIZE; j++)
    {
        for (int i = 0; i < SIZE; i++)
        {
            sum += field[j][i].value;
        }
        columnsSums[j] = sum;
        sum = 0;
    }
}

long long calcSumOfProducts()
{
    long long productOfSumOfRows = 1;
    long long productOfSumOfColumns = 1;

    int rowSums[SIZE];
    int columnsSums[SIZE];

    calcSumOfRows(rowSums);
    calcSumOfColumns(columnsSums);

    for (int i = 0; i < SIZE; i++)
    {
        productOfSumOfRows *= rowSums[i];
    }

    for (int i = 0; i < SIZE; i++)
    {
        productOfSumOfColumns *= columnsSums[i];
    }
    return productOfSumOfRows + productOfSumOfColumns;
}

static long long calcSumOfField()
{
    long long sum = 0;
    for (int i = 0; i < SIZE; ++i)
    {
        for (int j = 0; j < SIZE; ++j)
        {
            sum += field[i][j].value;
        }
    }
    return sum;
}
