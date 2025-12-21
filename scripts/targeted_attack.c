/**
 * Targeted Collision Attack for Secasy Hash
 *
 * This exploits the special handling of zero bytes in calcAndSetDirections:
 * When byte == 0, doNotSetAnyDirections() is called, which just clears the directions array.
 * This means multiple zeros in a row might have reduced entropy.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Defines.h"

/* Global variables required by Secasy core (normally defined in main.c) */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

static char* compute_hash(const unsigned char* data, size_t len) {
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

int main(void) {
    printf("=== Targeted Collision Analysis ===\n\n");

    // Test 1: Check how zeros are handled
    printf("Test 1: Zero byte sequences\n");
    printf("=========================================\n");

    for (int n = 0; n <= 8; n++) {
        unsigned char zeros[9] = {0};
        char* hash = compute_hash(zeros, n);
        printf("  %d zeros: %s\n", n, hash);
        free(hash);
    }

    // Test 2: Zero at different positions
    printf("\nTest 2: Single byte with zeros\n");
    printf("=========================================\n");

    unsigned char patterns[][4] = {
        {0x41, 0x00, 0x00, 0x00},
        {0x00, 0x41, 0x00, 0x00},
        {0x00, 0x00, 0x41, 0x00},
        {0x00, 0x00, 0x00, 0x41},
    };

    for (int i = 0; i < 4; i++) {
        char* hash = compute_hash(patterns[i], 4);
        printf("  Pattern %d: %02x %02x %02x %02x -> %s\n",
               i, patterns[i][0], patterns[i][1], patterns[i][2], patterns[i][3], hash);
        free(hash);
    }

    // Test 3: Permutations that might collide
    printf("\nTest 3: Direction symmetry check\n");
    printf("=========================================\n");

    // Bytes that produce same direction pattern
    // Direction pairs: 00=UP, 01=RIGHT, 10=LEFT, 11=DOWN
    // 0x55 = 01 01 01 01 (RIGHT RIGHT RIGHT RIGHT)
    // 0xAA = 10 10 10 10 (LEFT LEFT LEFT LEFT)
    unsigned char sym1[] = {0x55};
    unsigned char sym2[] = {0xAA};

    char* h1 = compute_hash(sym1, 1);
    char* h2 = compute_hash(sym2, 1);
    printf("  0x55 (all RIGHT): %s\n", h1);
    printf("  0xAA (all LEFT):  %s\n", h2);
    printf("  Same: %s\n", strcmp(h1, h2) == 0 ? "YES - COLLISION!" : "no");
    free(h1); free(h2);

    // Test 4: Bytes with same bit count
    printf("\nTest 4: Bytes with same popcount\n");
    printf("=========================================\n");

    unsigned char popcount2[] = {0x03, 0x05, 0x06, 0x09, 0x0A, 0x0C};  // All have 2 bits set
    for (int i = 0; i < 6; i++) {
        char* hash = compute_hash(&popcount2[i], 1);
        printf("  0x%02X: %s\n", popcount2[i], hash);
        free(hash);
    }

    // Test 5: Multi-byte patterns
    printf("\nTest 5: Multi-byte direction cancellation\n");
    printf("=========================================\n");

    // Try to find inputs where movements cancel out
    unsigned char cancel1[] = {0x01, 0x02};  // RIGHT then LEFT
    unsigned char cancel2[] = {0x02, 0x01};  // LEFT then RIGHT

    h1 = compute_hash(cancel1, 2);
    h2 = compute_hash(cancel2, 2);
    printf("  0x01,0x02 (R,L): %s\n", h1);
    printf("  0x02,0x01 (L,R): %s\n", h2);
    printf("  Same: %s\n", strcmp(h1, h2) == 0 ? "YES - COLLISION!" : "no");
    free(h1); free(h2);

    // Test 6: Modular arithmetic exploitation
    printf("\nTest 6: Field size wrap-around\n");
    printf("=========================================\n");

    // Field size is 8, so movements wrap at 8
    // Create patterns that should end up at same position
    unsigned char wrap1[8];
    unsigned char wrap2[8];

    // Fill with values that create specific movement patterns
    memset(wrap1, 0x00, 8);
    memset(wrap2, 0xFF, 8);

    wrap1[0] = 'A';
    wrap2[0] = 'A';

    h1 = compute_hash(wrap1, 8);
    h2 = compute_hash(wrap2, 8);
    printf("  A + 7 zeros: %s\n", h1);
    printf("  A + 7 x 0xFF: %s\n", h2);
    printf("  Same: %s\n", strcmp(h1, h2) == 0 ? "YES - COLLISION!" : "no");
    free(h1); free(h2);

    // Test 7: Empty vs near-empty
    printf("\nTest 7: Empty input behavior\n");
    printf("=========================================\n");

    char* empty = compute_hash(NULL, 0);
    printf("  Empty input: %s\n", empty ? empty : "(null)");
    if (empty) free(empty);

    unsigned char single_zero[] = {0x00};
    char* one_zero = compute_hash(single_zero, 1);
    printf("  Single zero: %s\n", one_zero);
    free(one_zero);

    printf("\n=== Analysis Complete ===\n");
    printf("\nNOTE: If any test shows 'COLLISION!' above, we found a vulnerability.\n");
    printf("Even without collisions, similar hash patterns suggest weak diffusion.\n");

    return 0;
}

