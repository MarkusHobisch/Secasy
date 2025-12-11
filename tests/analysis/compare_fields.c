/**
 * Compare field states after processing colliding inputs
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
unsigned long numberOfRounds = 100000;
int numberOfBits = 128;

void print_field_state(const char* label) {
    printf("\n%s:\n", label);
    printf("Position: (%u, %u)\n", pos.x, pos.y);
    printf("Field values (first row):\n  ");
    for (int j = 0; j < FIELD_SIZE; j++) {
        printf("%4llu ", (unsigned long long)field[0][j].value);
    }
    printf("\nField colorIndex (first row):\n  ");
    for (int j = 0; j < FIELD_SIZE; j++) {
        printf("%d ", field[0][j].colorIndex);
    }
    printf("\n");
    
    // Compute simple checksum of entire field
    uint64_t sum = 0;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            sum += field[i][j].value;
            sum += field[i][j].colorIndex;
        }
    }
    printf("Field checksum: %llu\n", (unsigned long long)sum);
}

int compare_fields(Tile_t saved[FIELD_SIZE][FIELD_SIZE]) {
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field[i][j].value != saved[i][j].value ||
                field[i][j].colorIndex != saved[i][j].colorIndex) {
                return 0;  // Different
            }
        }
    }
    return 1;  // Identical
}

int main(void) {
    Tile_t saved_field[FIELD_SIZE][FIELD_SIZE];
    Position_t saved_pos;
    
    printf("=== Comparing field states for colliding inputs ===\n");
    
    // Process 0x66
    unsigned char input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    memcpy(saved_field, field, sizeof(field));
    saved_pos = pos;
    print_field_state("After 0x66 ('f')");
    
    // Process 0x69
    unsigned char input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    print_field_state("After 0x69 ('i')");
    
    printf("\n=== Comparison ===\n");
    printf("Position match: %s\n", (pos.x == saved_pos.x && pos.y == saved_pos.y) ? "YES" : "NO");
    printf("Field match: %s\n", compare_fields(saved_field) ? "YES - IDENTICAL!" : "NO");
    
    // Process 0x99
    unsigned char input3 = 0x99;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input3, 1);
    print_field_state("After 0x99");
    printf("Field match with 0x66: %s\n", compare_fields(saved_field) ? "YES - IDENTICAL!" : "NO");
    
    return 0;
}
