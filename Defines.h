#ifndef SECASY_DEFINES_H
#define SECASY_DEFINES_H

// Default configuration values
#include <stdint.h>
#define DEFAULT_NUMBER_OF_ROUNDS 100000
#define DEFAULT_BIT_SIZE 512
#define DEFAULT_MAX_PRIME_INDEX 16000000
#define MAX_ALLOWED_PRIME_INDEX 50000000UL

// Added readability / reuse constants
#define MIN_HASH_BITS 64
#define MIN_HASH_OUTPUT_BITS 64
#define BYTES_PER_MB 1048576.0

// Field dimension (must be power of 2 for bitmask optimization)
#define FIELD_SIZE 16 // 16 x 16 = 256 cells

// Default I/O block size
#define DEFAULT_IO_BLOCK_SIZE (4 * 1024 * 1024) // 4 MB default read chunk size

typedef struct
{
    uint32_t x;
    uint32_t y;
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
    uint32_t posX;
    uint32_t posY;
    uint64_t value;       /* unsigned 64-bit for defined wrap-around overflow behavior */
    ColorIndex_t colorIndex;
    uint32_t primeIndex;
} Tile_t;

// Debug toggles
// Default: OFF for faster runs and clean output.
// Override at compile time, e.g. -DDEBUG_MODE=1 -DDEBUG_LOG_EXTENDED=1
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

#ifndef DEBUG_LOG_EXTENDED
#define DEBUG_LOG_EXTENDED 0
#endif

#endif
