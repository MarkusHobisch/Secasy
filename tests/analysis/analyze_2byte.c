/**
 * Analyze the 2-byte collision
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;
extern int lastPrime;
unsigned long numberOfRounds = 100000;
int numberOfBits = 128;

void print_field_compact(void) {
    printf("  Values row 0: ");
    for (int j = 0; j < FIELD_SIZE; j++) printf("%3llu ", (unsigned long long)field[0][j].value);
    printf("\n");
}

int compare_fields(Tile_t saved[FIELD_SIZE][FIELD_SIZE], Position_t saved_pos) {
    if (pos.x != saved_pos.x || pos.y != saved_pos.y) return 0;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field[i][j].value != saved[i][j].value ||
                field[i][j].colorIndex != saved[i][j].colorIndex ||
                field[i][j].primeIndex != saved[i][j].primeIndex) {
                return 0;
            }
        }
    }
    return 1;
}

int main(void) {
    printf("=== Analyzing 2-byte collision: 0x07,0x33 vs 0x0d,0x63 ===\n\n");
    
    Tile_t saved_field[FIELD_SIZE][FIELD_SIZE];
    Position_t saved_pos;
    
    // Process first input
    unsigned char input1[] = {0x07, 0x33};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input1, 2);
    memcpy(saved_field, field, sizeof(field));
    saved_pos = pos;
    
    printf("After 0x07,0x33:\n");
    printf("  Position: (%u, %u)\n", pos.x, pos.y);
    printf("  lastPrime: %d\n", lastPrime);
    print_field_compact();
    long long gen1 = generateHashValue();
    printf("  generateHashValue: %lld\n", gen1);
    
    // Process second input
    unsigned char input2[] = {0x0d, 0x63};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input2, 2);
    
    printf("\nAfter 0x0d,0x63:\n");
    printf("  Position: (%u, %u)\n", pos.x, pos.y);
    printf("  lastPrime: %d\n", lastPrime);
    print_field_compact();
    long long gen2 = generateHashValue();
    printf("  generateHashValue: %lld\n", gen2);
    
    printf("\n=== Comparison ===\n");
    printf("Position match: %s\n", (pos.x == saved_pos.x && pos.y == saved_pos.y) ? "YES" : "NO");
    printf("Field match: %s\n", compare_fields(saved_field, saved_pos) ? "YES - IDENTICAL!" : "NO");
    printf("generateHashValue match: %s\n", gen1 == gen2 ? "YES - COLLISION!" : "NO");
    
    // Get full hashes
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input1, 2);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input2, 2);
    char* hash2 = calculateHashValue();
    
    printf("\nFull hashes:\n");
    printf("  0x07,0x33: %s\n", hash1);
    printf("  0x0d,0x63: %s\n", hash2);
    printf("  Match: %s\n", strcmp(hash1, hash2) == 0 ? "YES - FULL COLLISION!" : "NO");
    
    free(hash1);
    free(hash2);
    
    return 0;
}
