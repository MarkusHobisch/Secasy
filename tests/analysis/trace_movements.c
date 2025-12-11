/**
 * Trace the exact movements for colliding inputs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FIELD_SIZE 8
#define UP 0
#define RIGHT 1
#define LEFT 2
#define DOWN 3
#define DIRECTIONS 4

// Primes: 2, 3, 5, 7, 11, 13, 17, 19, ...
static int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};

typedef struct {
    int x, y;
    int value;
    int primeIndex;
} State;

static inline int logicalShiftRight(int a, int b) {
    return (int)((unsigned int)a >> b);
}

void calcDirections(int byte, int* dirs, int* count) {
    *count = 0;
    while (byte != 0 && *count < DIRECTIONS) {
        dirs[(*count)++] = byte & 3;
        byte = logicalShiftRight(byte, 2);
    }
}

const char* dirName(int d) {
    switch(d) {
        case UP: return "UP";
        case RIGHT: return "RIGHT";
        case LEFT: return "LEFT";
        case DOWN: return "DOWN";
        default: return "?";
    }
}

void traceInput(const char* label, unsigned char* input, size_t len) {
    printf("\n========== %s ==========\n", label);
    printf("Input bytes: ");
    for (size_t i = 0; i < len; i++) printf("0x%02X ", input[i]);
    printf("\n\n");
    
    // Initial state
    State s = {0, 0, 2, 0};  // Start at (0,0), value=2 (first prime), primeIndex=0
    
    int field[FIELD_SIZE][FIELD_SIZE];
    for (int i = 0; i < FIELD_SIZE; i++)
        for (int j = 0; j < FIELD_SIZE; j++)
            field[i][j] = 2;  // All start with prime 2
    
    int step = 0;
    
    for (size_t b = 0; b < len; b++) {
        int byte = input[b];
        printf("--- Byte %zu: 0x%02X (binary: ", b, byte);
        for (int i = 7; i >= 0; i--) printf("%d", (byte >> i) & 1);
        printf(") ---\n");
        
        int dirs[4], dirCount;
        calcDirections(byte, dirs, &dirCount);
        
        printf("Directions extracted: ");
        for (int i = 0; i < dirCount; i++) printf("%s ", dirName(dirs[i]));
        printf("(%d total)\n\n", dirCount);
        
        for (int d = 0; d < dirCount; d++) {
            int oldPrime = field[s.x][s.y];
            int newPrimeIndex = s.primeIndex + 1;
            int newPrime = primes[newPrimeIndex];
            
            printf("Step %d: At (%d,%d), value=%d\n", step, s.x, s.y, oldPrime);
            printf("        Update tile to prime[%d]=%d\n", newPrimeIndex, newPrime);
            field[s.x][s.y] = newPrime;
            s.primeIndex = newPrimeIndex;
            
            int oldX = s.x, oldY = s.y;
            
            switch(dirs[d]) {
                case UP:
                    // y = (y - oldPrime + 1) mod 8
                    s.y = ((s.y - oldPrime + 1) % FIELD_SIZE + FIELD_SIZE) % FIELD_SIZE;
                    break;
                case DOWN:
                    // y = (y + oldPrime) mod 8
                    s.y = (s.y + oldPrime) % FIELD_SIZE;
                    break;
                case LEFT:
                    // x = (x - oldPrime) mod 8
                    s.x = ((s.x - oldPrime) % FIELD_SIZE + FIELD_SIZE) % FIELD_SIZE;
                    break;
                case RIGHT:
                    // x = (x + oldPrime + 1) mod 8
                    s.x = (s.x + oldPrime + 1) % FIELD_SIZE;
                    break;
            }
            
            printf("        Move %s by %d: (%d,%d) -> (%d,%d)\n\n", 
                   dirName(dirs[d]), oldPrime, oldX, oldY, s.x, s.y);
            step++;
        }
    }
    
    // Final update
    int newPrimeIndex = s.primeIndex + 1;
    int newPrime = primes[newPrimeIndex];
    printf("Final: At (%d,%d), update to prime[%d]=%d\n", s.x, s.y, newPrimeIndex, newPrime);
    field[s.x][s.y] = newPrime;
    
    printf("\nFinal Position: (%d, %d)\n", s.x, s.y);
    printf("Final lastPrime: %d\n", newPrime);
    
    printf("\nFinal Field (non-2 values only):\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field[i][j] != 2) {
                printf("  [%d][%d] = %d\n", i, j, field[i][j]);
            }
        }
    }
}

int main(void) {
    unsigned char input1[] = {0x07, 0x33};
    traceInput("Input A: [0x07, 0x33]", input1, 2);
    
    unsigned char input2[] = {0x0d, 0x63};
    traceInput("Input B: [0x0D, 0x63]", input2, 2);
    
    return 0;
}
