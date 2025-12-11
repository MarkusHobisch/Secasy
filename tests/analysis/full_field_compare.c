/**
 * Full field comparison for colliding inputs
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

void print_full_field(const char* label) {
    printf("\n%s (position: %u,%u):\n", label, pos.x, pos.y);
    printf("Values:\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        printf("  Row %d: ", i);
        for (int j = 0; j < FIELD_SIZE; j++) {
            printf("%3llu ", (unsigned long long)field[i][j].value);
        }
        printf("\n");
    }
    printf("ColorIndex:\n");
    for (int i = 0; i < FIELD_SIZE; i++) {
        printf("  Row %d: ", i);
        for (int j = 0; j < FIELD_SIZE; j++) {
            printf("%d ", field[i][j].colorIndex);
        }
        printf("\n");
    }
}

int main(void) {
    printf("=== Full field comparison for 0x66 vs 0x69 ===\n");
    
    unsigned char input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    print_full_field("After 0x66 ('f')");
    
    unsigned char input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    print_full_field("After 0x69 ('i')");
    
    // Show differences
    printf("\n=== Differences ===\n");
    unsigned char input1b = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1b, 1);
    Tile_t field1[FIELD_SIZE][FIELD_SIZE];
    memcpy(field1, field, sizeof(field));
    
    unsigned char input2b = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2b, 1);
    
    int diff_count = 0;
    for (int i = 0; i < FIELD_SIZE; i++) {
        for (int j = 0; j < FIELD_SIZE; j++) {
            if (field1[i][j].value != field[i][j].value ||
                field1[i][j].colorIndex != field[i][j].colorIndex) {
                printf("  [%d,%d]: 0x66=(%llu, color=%d) vs 0x69=(%llu, color=%d)\n",
                       i, j, 
                       (unsigned long long)field1[i][j].value, field1[i][j].colorIndex,
                       (unsigned long long)field[i][j].value, field[i][j].colorIndex);
                diff_count++;
            }
        }
    }
    printf("Total differences: %d tiles\n", diff_count);
    
    return 0;
}
