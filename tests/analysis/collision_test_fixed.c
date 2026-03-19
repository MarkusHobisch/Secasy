#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FIELD_SIZE 8
#define FIELD_SIZE_MASK (FIELD_SIZE - 1)
#define FIRST_PRIME 2
#define NUM_COLOR_OPERATIONS 6
#define SQUARE_AVOIDANCE_VALUE 1
#define DIRECTIONS_PER_BYTE 4
#define DIRECTION_MASK 0x3

static int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};
#define NUM_PRIMES 16

typedef struct
{
    uint64_t value;
    uint32_t primeIndex;
    uint32_t colorIndex;
} Tile;

// Simplified hash using correct cursor walk and per-tile state (8x8 grid)
uint64_t simple_hash(const unsigned char *data, size_t len)
{
    Tile field[FIELD_SIZE][FIELD_SIZE];
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
        {
            field[i][j].value = FIRST_PRIME;
            field[i][j].primeIndex = 0;
            field[i][j].colorIndex = 0;
        }

    uint32_t x = 0, y = 0;

    for (size_t idx = 0; idx < len; idx++)
    {
        int byte = data[idx] & 0xFF;

        for (int d = 0; d < DIRECTIONS_PER_BYTE; d++)
        {
            int dir = (byte >> (d * 2)) & DIRECTION_MASK;

            Tile *tile = &field[x][y];
            uint64_t oldVal = tile->value;

            // Advance prime and color per-tile (matching real nextPrimeNumber)
            tile->primeIndex = (tile->primeIndex + 1 + dir) % NUM_PRIMES;
            tile->colorIndex = (tile->colorIndex + 1) % NUM_COLOR_OPERATIONS;
            tile->value = primes[tile->primeIndex];

            // Cursor walk (matching real processDirectionStep)
            switch (dir)
            {
            case 0: // UP
                y = (y - (uint32_t)oldVal) & FIELD_SIZE_MASK;
                break;
            case 1: // RIGHT
                x = (x + (uint32_t)oldVal + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
                break;
            case 2: // LEFT
                x = (x - (uint32_t)oldVal) & FIELD_SIZE_MASK;
                break;
            case 3: // DOWN
                y = (y + (uint32_t)oldVal + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;
                break;
            }
        }
    }

    // Final tile update (direction = 0)
    Tile *tile = &field[x][y];
    tile->primeIndex = (tile->primeIndex + 1) % NUM_PRIMES;
    tile->colorIndex = (tile->colorIndex + 1) % NUM_COLOR_OPERATIONS;
    tile->value = primes[tile->primeIndex];

    // Simplified hash extraction (no Phase 3/4 mixing)
    uint64_t hash = 0;
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            hash ^= (field[i][j].value * (uint64_t)(i + 1) * (uint64_t)(j + 1) * 31);
    return hash;
}

int main()
{
    printf("Searching for collisions with fix applied...\n");
    printf("Testing all 1-byte pairs (256 x 256)...\n");

    int collisions_1byte = 0;
    for (int a = 0; a < 256; a++)
    {
        for (int b = a + 1; b < 256; b++)
        {
            unsigned char da[1] = {a};
            unsigned char db[1] = {b};
            if (simple_hash(da, 1) == simple_hash(db, 1))
            {
                printf("  1-byte collision: 0x%02X == 0x%02X\n", a, b);
                collisions_1byte++;
            }
        }
    }
    printf("1-byte collisions found: %d\n\n", collisions_1byte);

    printf("Testing all 2-byte pairs (65536 x 65536) - this takes a moment...\n");

// Use hash table for 2-byte test
#define TABLE_SIZE 1000003
    uint64_t *table = calloc(TABLE_SIZE, sizeof(uint64_t));
    uint32_t *inputs = calloc(TABLE_SIZE, sizeof(uint32_t));

    int collisions_2byte = 0;
    for (int i = 0; i < 65536 && collisions_2byte < 20; i++)
    {
        unsigned char d[2] = {i & 0xFF, (i >> 8) & 0xFF};
        uint64_t h = simple_hash(d, 2);
        uint32_t idx = h % TABLE_SIZE;

        // Linear probing
        while (table[idx] != 0)
        {
            if (table[idx] == h)
            {
                // Verify
                unsigned char d2[2] = {inputs[idx] & 0xFF, (inputs[idx] >> 8) & 0xFF};
                if (simple_hash(d2, 2) == h && inputs[idx] != (uint32_t)i)
                {
                    printf("  2-byte collision: [0x%02X,0x%02X] == [0x%02X,0x%02X]\n",
                           d[0], d[1], d2[0], d2[1]);
                    collisions_2byte++;
                }
            }
            idx = (idx + 1) % TABLE_SIZE;
        }
        table[idx] = h;
        inputs[idx] = i;

        if (i % 10000 == 0)
            printf("  Progress: %d/65536\n", i);
    }

    printf("\n2-byte collisions found: %d\n", collisions_2byte);
    printf("\nDone! Fix appears %s\n",
           (collisions_1byte == 0 && collisions_2byte == 0) ? "EFFECTIVE" : "INCOMPLETE");

    free(table);
    free(inputs);
    return 0;
}
