#include "Calculations.h"
#include "Defines.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern int lastPrime;

static long long calcSumOfField(void);

long long generateHashValue()
{
    const long long checksum = calcSumOfProducts() ^ lastPrime;
    const long long fieldSum = calcSumOfField();
    return checksum ^ fieldSum;
}

void calcSumOfRows(int* rowSums)
{
    int sum = 0;
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
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
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        for (int i = 0; i < FIELD_SIZE; i++)
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

    int rowSums[FIELD_SIZE];
    int columnsSums[FIELD_SIZE];

    calcSumOfRows(rowSums);
    calcSumOfColumns(columnsSums);

    for (int i = 0; i < FIELD_SIZE; i++)
    {
        productOfSumOfRows *= rowSums[i];
    }

    for (int i = 0; i < FIELD_SIZE; i++)
    {
        productOfSumOfColumns *= columnsSums[i];
    }
    return productOfSumOfRows + productOfSumOfColumns;
}

static long long calcSumOfField(void)
{
    long long sum = 0;
    for (int i = 0; i < FIELD_SIZE; ++i)
    {
        for (int j = 0; j < FIELD_SIZE; ++j)
        {
            sum += field[i][j].value;
        }
    }
    return sum;
}
