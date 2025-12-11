/**
 * EXACT trace matching InitializationPhase.c logic
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
#define SQUARE_AVOIDANCE_VALUE 1

// First primes
static int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};

typedef struct {
    uint32_t x, y;
    int primeIndex;
    int colorIndex;
} State;

int field[FIELD_SIZE][FIELD_SIZE];
int fieldPrimeIndex[FIELD_SIZE][FIELD_SIZE];
int fieldColorIndex[FIELD_SIZE][FIELD_SIZE];

State state;

static inline int logicalShiftRight(int a, int b) {
    return (int)((unsigned int)a >> b);
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

void calcAndSetDirections(int byte, int* directions) {
    memset(directions, 0, DIRECTIONS * sizeof(int));
    int index = 0;
    while (byte != 0 && index < DIRECTIONS) {
        directions[index++] = (byte & 3);
        byte = logicalShiftRight(byte, 2);
    }
    // Rest stays 0 (= UP)
}

int nextPrimeNumber(int x, int y) {
    int pi = fieldPrimeIndex[x][y];
    int ci = fieldColorIndex[x][y];
    int numPrimes = 16;
    int colorLen = 5;
    
    pi = (pi + 1) < numPrimes ? (pi + 1) : 0;
    ci = (pi != 0 && (ci + 1) < colorLen) ? (ci + 1) : 0;
    
    fieldPrimeIndex[x][y] = pi;
    fieldColorIndex[x][y] = ci;
    
    return primes[pi];
}

void writeNextNumberOnMove(int move, int step) {
    int oldPrime = field[state.x][state.y];
    int newPrime = nextPrimeNumber(state.x, state.y);
    
    printf("  Step %d: pos=(%u,%u) oldValue=%d -> newValue=%d\n", 
           step, state.x, state.y, oldPrime, newPrime);
    
    field[state.x][state.y] = newPrime;
    
    uint32_t oldX = state.x, oldY = state.y;
    
    switch (move) {
        case UP:
            state.y = (uint32_t)(((int)state.y - oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1));
            break;
        case DOWN:
            state.y = (uint32_t)(((int)state.y + oldPrime) & (FIELD_SIZE - 1));
            break;
        case LEFT:
            state.x = (uint32_t)(((int)state.x - oldPrime) & (FIELD_SIZE - 1));
            break;
        case RIGHT:
            state.x = (uint32_t)(((int)state.x + oldPrime + SQUARE_AVOIDANCE_VALUE) & (FIELD_SIZE - 1));
            break;
    }
    
    printf("          Move %s (jump by %d): (%u,%u) -> (%u,%u)\n", 
           dirName(move), oldPrime, oldX, oldY, state.x, state.y);
}

void initField() {
    state.x = 0;
    state.y = 0;
    state.primeIndex = 0;
    state.colorIndex = 0;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            field[i][j] = 2;  // First prime
            fieldPrimeIndex[i][j] = 0;
            fieldColorIndex[i][j] = 0;
        }
    }
}

void processBuffer(unsigned char* data, size_t len) {
    int step = 0;
    int directions[DIRECTIONS];
    
    for (size_t i = 0; i < len; i++) {
        int byte = data[i] & 0xFF;
        
        printf("\n--- Byte %zu: 0x%02X (binary: ", i, byte);
        for (int b = 7; b >= 0; b--) printf("%d", (byte >> b) & 1);
        printf(") ---\n");
        
        if (byte != 0) {
            calcAndSetDirections(byte, directions);
        } else {
            memset(directions, 0, DIRECTIONS * sizeof(int));
        }
        
        printf("Directions: [%s, %s, %s, %s]\n",
               dirName(directions[0]), dirName(directions[1]),
               dirName(directions[2]), dirName(directions[3]));
        
        // ALWAYS execute 4 directions!
        for (int d = 0; d < DIRECTIONS; d++) {
            writeNextNumberOnMove(directions[d], step++);
        }
    }
    
    // Final tile update
    printf("\n--- Final tile update ---\n");
    int finalPrime = nextPrimeNumber(state.x, state.y);
    printf("  At (%u,%u): update to %d\n", state.x, state.y, finalPrime);
    field[state.x][state.y] = finalPrime;
}

void printField() {
    printf("\nField (values != 2):\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field[i][j] != 2) {
                printf("  field[%d][%d] = %d\n", i, j, field[i][j]);
            }
        }
    }
    printf("\nFinal pos: (%u, %u)\n", state.x, state.y);
}

int main(void) {
    printf("========================================\n");
    printf("INPUT A: [0x07, 0x33]\n");
    printf("========================================\n");
    initField();
    unsigned char input1[] = {0x07, 0x33};
    processBuffer(input1, 2);
    printField();
    
    printf("\n\n========================================\n");
    printf("INPUT B: [0x0D, 0x63]\n");
    printf("========================================\n");
    initField();
    unsigned char input2[] = {0x0d, 0x63};
    processBuffer(input2, 2);
    printField();
    
    return 0;
}
