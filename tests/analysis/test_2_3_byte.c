/**
 * Test for 2-byte and 3-byte collisions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;
extern int lastPrime;
unsigned long numberOfRounds = 100000;
int hashLengthInBits = 128;

void test_2byte_collision(void) {
    printf("\n=== Testing 2-byte collision: 0x07,0x33 vs 0x0d,0x63 ===\n");
    
    // Test first input
    unsigned char input1[] = {0x07, 0x33};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input1, 2);
    char* hash1 = calculateHashValue();
    
    // Test second input
    unsigned char input2[] = {0x0d, 0x63};
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input2, 2);
    char* hash2 = calculateHashValue();
    
    printf("Input 1 (0x07 0x33): %s\n", hash1);
    printf("Input 2 (0x0d 0x63): %s\n", hash2);
    printf("Result: %s\n", strcmp(hash1, hash2) == 0 ? "COLLISION!" : "Different (No collision)");
    
    free(hash1);
    free(hash2);
}

void test_3byte_patterns(void) {
    printf("\n=== Testing 3-byte patterns ===\n");
    
    // Test verschiedene 3-Byte-Muster
    unsigned char patterns[][3] = {
        {0x01, 0x02, 0x03},
        {0x01, 0x02, 0x04},
        {0x10, 0x20, 0x30},
        {0x11, 0x22, 0x33},
        {0xFF, 0xFE, 0xFD},
        {0x00, 0x00, 0x01},
        {0x00, 0x01, 0x00},
        {0x01, 0x00, 0x00}
    };
    
    int num_patterns = 8;
    char* hashes[8];
    
    // Berechne alle Hashes
    for (int i = 0; i < num_patterns; i++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(patterns[i], 3);
        hashes[i] = calculateHashValue();
        printf("Pattern %d (0x%02X 0x%02X 0x%02X): %s\n", 
               i+1, patterns[i][0], patterns[i][1], patterns[i][2], hashes[i]);
    }
    
    // Prüfe auf Kollisionen
    printf("\n--- Checking for collisions ---\n");
    int collisions_found = 0;
    for (int i = 0; i < num_patterns; i++) {
        for (int j = i + 1; j < num_patterns; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                printf("COLLISION: Pattern %d (0x%02X 0x%02X 0x%02X) == Pattern %d (0x%02X 0x%02X 0x%02X)\n",
                       i+1, patterns[i][0], patterns[i][1], patterns[i][2],
                       j+1, patterns[j][0], patterns[j][1], patterns[j][2]);
                collisions_found++;
            }
        }
    }
    
    if (collisions_found == 0) {
        printf("No collisions found among %d 3-byte patterns.\n", num_patterns);
    } else {
        printf("Found %d collisions!\n", collisions_found);
    }
    
    // Cleanup
    for (int i = 0; i < num_patterns; i++) {
        free(hashes[i]);
    }
}

void test_3byte_systematic(void) {
    printf("\n=== Systematic 3-byte test (sampling) ===\n");
    printf("Testing 1000 random 3-byte combinations...\n");
    
    srand((unsigned)time(NULL));
    int num_samples = 1000;
    char** hashes = malloc(num_samples * sizeof(char*));
    unsigned char (*samples)[3] = malloc(num_samples * sizeof(unsigned char[3]));
    
    // Generiere und hashe zufällige Samples
    for (int i = 0; i < num_samples; i++) {
        samples[i][0] = rand() % 256;
        samples[i][1] = rand() % 256;
        samples[i][2] = rand() % 256;
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(samples[i], 3);
        hashes[i] = calculateHashValue();
        
        if ((i + 1) % 100 == 0) {
            printf("Progress: %d/%d\n", i + 1, num_samples);
        }
    }
    
    // Prüfe auf Kollisionen
    printf("\n--- Checking for collisions ---\n");
    int collisions_found = 0;
    for (int i = 0; i < num_samples; i++) {
        for (int j = i + 1; j < num_samples; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                printf("COLLISION: (0x%02X 0x%02X 0x%02X) == (0x%02X 0x%02X 0x%02X)\n",
                       samples[i][0], samples[i][1], samples[i][2],
                       samples[j][0], samples[j][1], samples[j][2]);
                printf("  Hash: %s\n", hashes[i]);
                collisions_found++;
            }
        }
    }
    
    printf("\n=== Results ===\n");
    printf("Tested: %d random 3-byte combinations\n", num_samples);
    printf("Collisions found: %d\n", collisions_found);
    printf("Collision rate: %.4f%%\n", (collisions_found * 100.0) / num_samples);
    
    // Cleanup
    for (int i = 0; i < num_samples; i++) {
        free(hashes[i]);
    }
    free(hashes);
    free(samples);
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  2-Byte and 3-Byte Collision Test            ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    test_2byte_collision();
    test_3byte_patterns();
    test_3byte_systematic();
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Test Complete                                ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    return 0;
}
