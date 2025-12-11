/**
 * Show full 8x8 field for colliding inputs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;
extern int lastPrime;
unsigned long numberOfRounds = 100000;
int numberOfBits = 128;

void print_full_field(const char* label, unsigned char* input, size_t len) {
    printf("\n%s: [", label);
    for (size_t i = 0; i < len; i++) {
        printf("0x%02X%s", input[i], i < len-1 ? ", " : "");
    }
    printf("]\n");
    printf("Position after init: (%u, %u), lastPrime: %d\n", pos.x, pos.y, lastPrime);
    printf("========================================\n");
    
    // Print header
    printf("     ");
    for (int j = 0; j < FIELD_SIZE; j++) {
        printf("  Col%d ", j);
    }
    printf("\n");
    
    // Print values
    printf("\nVALUES:\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        printf("Row%d:", i);
        for (int j = 0; j < FIELD_SIZE; j++) {
            printf(" %5llu ", (unsigned long long)field[i][j].value);
        }
        printf("\n");
    }
    
    // Print colorIndex
    printf("\nCOLOR INDEX (0=ADD, 1=SUB, 2=XOR, 3=AND, 4=OR):\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        printf("Row%d:", i);
        for (int j = 0; j < FIELD_SIZE; j++) {
            printf("    %d  ", field[i][j].colorIndex);
        }
        printf("\n");
    }
    
    // Print primeIndex
    printf("\nPRIME INDEX:\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        printf("Row%d:", i);
        for (int j = 0; j < FIELD_SIZE; j++) {
            printf(" %5u ", field[i][j].primeIndex);
        }
        printf("\n");
    }
    printf("========================================\n");
}

int main(void) {
    printf("=== COMPARING FIELD STATES FOR COLLIDING INPUTS ===\n");
    
    // First: the 1-byte collisions we fixed
    printf("\n\n########## 1-BYTE COLLISION (now FIXED) ##########\n");
    
    unsigned char in1a[] = {0x66};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in1a, 1);
    print_full_field("Input 'f'", in1a, 1);
    
    unsigned char in1b[] = {0x69};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in1b, 1);
    print_full_field("Input 'i'", in1b, 1);
    
    // Second: the 2-byte collision still present
    printf("\n\n########## 2-BYTE COLLISION (STILL EXISTS) ##########\n");
    
    unsigned char in2a[] = {0x07, 0x33};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in2a, 2);
    print_full_field("Input A", in2a, 2);
    
    unsigned char in2b[] = {0x0d, 0x63};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in2b, 2);
    print_full_field("Input B", in2b, 2);
    
    // Show if they're identical
    printf("\n=== DIFFERENCE ANALYSIS ===\n");
    
    // Reload both and compare
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in2a, 2);
    Tile_t field1[FIELD_SIZE][FIELD_SIZE];
    memcpy(field1, field, sizeof(field));
    Position_t pos1 = pos;
    int lastPrime1 = lastPrime;
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(in2b, 2);
    
    int diffs = 0;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field1[i][j].value != field[i][j].value ||
                field1[i][j].colorIndex != field[i][j].colorIndex ||
                field1[i][j].primeIndex != field[i][j].primeIndex) {
                printf("DIFF at [%d][%d]: A=(%llu,c%d,p%u) vs B=(%llu,c%d,p%u)\n",
                       i, j,
                       (unsigned long long)field1[i][j].value, field1[i][j].colorIndex, field1[i][j].primeIndex,
                       (unsigned long long)field[i][j].value, field[i][j].colorIndex, field[i][j].primeIndex);
                diffs++;
            }
        }
    }
    
    if (diffs == 0) {
        printf(">>> FIELDS ARE 100%% IDENTICAL! <<<\n");
        printf(">>> This means different inputs produce the SAME internal state! <<<\n");
    }
    
    printf("\nPosition: A=(%u,%u) vs B=(%u,%u) -> %s\n", 
           pos1.x, pos1.y, pos.x, pos.y,
           (pos1.x == pos.x && pos1.y == pos.y) ? "SAME" : "DIFFERENT");
    printf("lastPrime: A=%d vs B=%d -> %s\n",
           lastPrime1, lastPrime,
           lastPrime1 == lastPrime ? "SAME" : "DIFFERENT");
    
    return 0;
}
