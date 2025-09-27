#ifndef SECASY_DEFINES_H
#define SECASY_DEFINES_H

// Default configuration values
#define DEFAULT_NUMBER_OF_ROUNDS 100000
#define DEFAULT_BIT_SIZE 512
#define DEFAULT_MAX_PRIME_INDEX 16000000

// Added readability / reuse constants
#define MIN_HASH_BITS 64
#define BYTES_PER_MB 1048576.0

// Field dimension (must remain a power of two)
#define FIELD_SIZE 8 // 8 x 8 -> 1.000.000 over 64 = 10^294 >> 2^512

// Default I/O block size
#define DEFAULT_IO_BLOCK_SIZE (4 * 1024 * 1024) // 4 MB default read chunk size

typedef struct
{
    int x;
    int y;
} Position_t;

typedef enum
{
    ADD = 0,
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

// Debug toggles
#define DEBUG_MODE 0
#define DEBUG_LOG_EXTENDED 0

#endif
