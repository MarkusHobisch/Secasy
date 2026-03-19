#ifndef SECASY_DEFINES_H
#define SECASY_DEFINES_H

// Default configuration values
#include <stdint.h>
#define DEFAULT_NUMBER_OF_ROUNDS 10
#define DEFAULT_BIT_SIZE 512
#define DEFAULT_MAX_PRIME_INDEX 16000000
#define MAX_ALLOWED_PRIME_INDEX 50000000UL

// Added readability / reuse constants
#define HASH_BITS 64
#define HASH_OUTPUT_BITS 64
#define BYTES_PER_MB 1048576.0

// Field dimension (must be power of 2 for bitmask optimization)
#define FIELD_SIZE 16 // 16 x 16 = 256 cells
#define FIELD_SIZE_MASK (FIELD_SIZE - 1) // bitmask for fast modulo wrapping

// Direction encoding: each byte yields 4 direction steps of 2 bits each
#define BITS_PER_DIRECTION 2
#define DIRECTIONS_PER_BYTE 4
#define DIRECTION_MASK 0x3 // 2-bit mask to extract direction from a byte

// Number of color operations (ADD, SUB, XOR, RLX, RRA, INVERT)
#define NUM_COLOR_OPERATIONS 6

// Default I/O block size
#define DEFAULT_IO_BLOCK_SIZE (4 * 1024 * 1024) // 4 MB default read chunk size

// Hash extraction constants
#define HASH_HEX_CHARS_PER_BLOCK 16 // hex chars per 64-bit block

typedef struct
{
    uint32_t x;
    uint32_t y;
} Position_t;

// 64-bit bitwise rotation (portable, branchless)
#define ROTATE_LEFT_64(v, n)  (((v) << (n)) | ((v) >> (64 - (n))))
#define ROTATE_RIGHT_64(v, n) (((v) >> (n)) | ((v) << (64 - (n))))

typedef enum
{
    ADD = 0,
    SUB = 1,
    XOR = 2,
    ROTATE_LEFT_XOR = 3,
    ROTATE_RIGHT_ADD = 4,
    INVERT = 5
} ColorIndex_t;

typedef struct
{
    uint32_t posX;
    uint32_t posY;
    uint64_t value;
    ColorIndex_t colorIndex;
    uint32_t primeIndex;
} Tile_t;

// Debug toggles — enabled at runtime with -d (debug) and -e (extended debug).
// Can also override defaults at compile time: -DDEBUG_MODE_DEFAULT=1
#ifndef DEBUG_MODE_DEFAULT
#define DEBUG_MODE_DEFAULT 0
#endif

#ifndef DEBUG_LOG_EXTENDED_DEFAULT
#define DEBUG_LOG_EXTENDED_DEFAULT 0
#endif

extern int g_debug_mode;
extern int g_debug_extended;

#endif
