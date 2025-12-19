/**
 * Find ALL 1-byte collisions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
unsigned long numberOfRounds = 100000;
int hashLengthInBits = 128;

int main(void) {
    char hashes[256][256];
    unsigned char inputs[256];
    
    printf("Computing all 256 single-byte hashes...\n");
    
    for (int i = 0; i < 256; i++) {
        unsigned char input = (unsigned char)i;
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(&input, 1);
        char* hash = calculateHashValue();
        strcpy(hashes[i], hash);
        free(hash);
    }
    
    printf("\nSearching for collisions...\n\n");
    
    int collision_count = 0;
    for (int i = 0; i < 256; i++) {
        for (int j = i + 1; j < 256; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                printf("COLLISION: 0x%02X ('%c') = 0x%02X ('%c')\n", 
                       i, (i >= 32 && i < 127) ? i : '?',
                       j, (j >= 32 && j < 127) ? j : '?');
                printf("  Hash: %s\n", hashes[i]);
                collision_count++;
            }
        }
    }
    
    printf("\n=== Summary ===\n");
    printf("Total 1-byte collision pairs: %d\n", collision_count);
    printf("Unique hash values: %d (expected: 256)\n", 256 - collision_count);
    
    return 0;
}
