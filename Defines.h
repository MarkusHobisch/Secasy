#ifndef SECASY_DEFINES_H
#define SECASY_DEFINES_H

#define DEFAULT_NUMBER_OF_ROUNDS 100000
#define DEFAULT_BIT_SIZE 512
#define DEFAULT_MAX_PRIME_INDEX 16000000

typedef struct
{
    int x;
    int y;
} Position_t;

typedef enum
{
    AND = 0,
    SUB = 1,
    XOR = 2,
    BITWISE_AND = 3,
    BITWISE_OR = 4,
    INVERT = 5
} ColorIndex_t;

typedef struct
{
    int posX;
    int posY;
    int value;
    ColorIndex_t colorIndex;
    int primeIndex;
} Tile_t;

#define SIZE 8 // 8 x 8 -> 1.000.000 over 64 = 10^294 >> 2^512
#define DEBUG_MODE 0
#define DEBUG_LOG_EXTENDED 0

#endif
