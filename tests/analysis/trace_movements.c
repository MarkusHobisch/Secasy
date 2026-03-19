/**
 * Trace the exact movements for colliding inputs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FIELD_SIZE 8
#define FIELD_SIZE_MASK (FIELD_SIZE - 1)
#define UP 0
#define RIGHT 1
#define LEFT 2
#define DOWN 3
#define DIRECTIONS 4
#define SQUARE_AVOIDANCE_VALUE 1
#define NUM_COLOR_OPERATIONS 6

// Primes: 2, 3, 5, 7, 11, 13, 17, 19, ...
static int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};
#define NUM_PRIMES 16

typedef struct
{
    uint64_t value;
    uint32_t primeIndex;
    uint32_t colorIndex;
} Tile;

typedef struct
{
    uint32_t x, y;
} Position;

const char *dirName(int d)
{
    switch (d)
    {
    case UP:
        return "UP";
    case RIGHT:
        return "RIGHT";
    case LEFT:
        return "LEFT";
    case DOWN:
        return "DOWN";
    default:
        return "?";
    }
}

void traceInput(const char *label, unsigned char *input, size_t len)
{
    printf("\n========== %s ==========\n", label);
    printf("Input bytes: ");
    for (size_t i = 0; i < len; i++)
        printf("0x%02X ", input[i]);
    printf("\n\n");

    Tile field[FIELD_SIZE][FIELD_SIZE];
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            field[i][j].value = 2;
            field[i][j].primeIndex = 0;
            field[i][j].colorIndex = 0;
        }

    Position pos = {0, 0};
    int step = 0;

    for (size_t b = 0; b < len; b++)
    {
        int byte = input[b];
        printf("--- Byte %zu: 0x%02X (binary: ", b, byte);
        for (int i = 7; i >= 0; i--)
            printf("%d", (byte >> i) & 1);
        printf(") ---\n");

        // Always extract 4 directions (matching real processByteDirections)
        int dirs[DIRECTIONS];
        for (int i = 0; i < DIRECTIONS; i++)
            dirs[i] = (byte >> (i * 2)) & 0x3;

        printf("Directions: %s %s %s %s (4 total)\n\n",
               dirName(dirs[0]), dirName(dirs[1]),
               dirName(dirs[2]), dirName(dirs[3]));

        for (int d = 0; d < DIRECTIONS; d++)
        {
            Tile *tile = &field[pos.x][pos.y];
            uint64_t oldPrime = tile->value;

            // Advance per-tile prime and color (matching real nextPrimeNumber)
            tile->primeIndex = (tile->primeIndex + 1 + dirs[d]) % NUM_PRIMES;
            tile->colorIndex = (tile->colorIndex + 1) % NUM_COLOR_OPERATIONS;
            int newPrime = primes[tile->primeIndex];
            tile->value = (uint64_t)newPrime;

            printf("Step %d: At (%u,%u), value=%llu\n", step, pos.x, pos.y, (unsigned long long)oldPrime);
            printf("        Update tile to prime[%u]=%d\n", tile->primeIndex, newPrime);

            uint32_t oldX = pos.x, oldY = pos.y;

            // Cursor walk (matching real processDirectionStep: SAV on DOWN/RIGHT)
            switch (dirs[d])
            {
            case UP:
                pos.y = (pos.y - (uint32_t)oldPrime) & FIELD_SIZE_MASK;
                break;
            case DOWN:
                pos.y = (pos.y + (uint32_t)oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
                break;
            case LEFT:
                pos.x = (pos.x - (uint32_t)oldPrime) & FIELD_SIZE_MASK;
                break;
            case RIGHT:
                pos.x = (pos.x + (uint32_t)oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
                break;
            }

            printf("        Move %s by %llu: (%u,%u) -> (%u,%u)\n\n",
                   dirName(dirs[d]), (unsigned long long)oldPrime, oldX, oldY, pos.x, pos.y);
            step++;
        }
    }

    // Final tile update (direction = 0)
    Tile *tile = &field[pos.x][pos.y];
    tile->primeIndex = (tile->primeIndex + 1) % NUM_PRIMES;
    tile->colorIndex = (tile->colorIndex + 1) % NUM_COLOR_OPERATIONS;
    int finalPrime = primes[tile->primeIndex];
    tile->value = (uint64_t)finalPrime;

    printf("Final: At (%u,%u), update to prime[%u]=%d\n", pos.x, pos.y, tile->primeIndex, finalPrime);
    printf("\nFinal Position: (%u, %u)\n", pos.x, pos.y);
    printf("Final lastPrime: %d\n", finalPrime);

    printf("\nFinal Field (non-2 values only):\n");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            if (field[i][j].value != 2)
            {
                printf("  [%d][%d] = %llu\n", i, j, (unsigned long long)field[i][j].value);
            }
        }
    }
}

int main(void)
{
    unsigned char input1[] = {0x07, 0x33};
    traceInput("Input A: [0x07, 0x33]", input1, 2);

    unsigned char input2[] = {0x0d, 0x63};
    traceInput("Input B: [0x0D, 0x63]", input2, 2);

    return 0;
}
