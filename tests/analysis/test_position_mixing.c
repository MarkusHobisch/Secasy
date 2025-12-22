/**
 * Test whether position mixing (byte ^= position) is necessary
 * Check for collisions that might occur without it
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;
extern int lastPrime;
unsigned long numberOfRounds = 100000;
int hashLengthInBits = 128;

void test_repeated_bytes(void) {
    printf("\n=== Test 1: Repeated same byte ===\n");
    
    // Test if "AAAA" differs from "AA"
    unsigned char aa[] = {0x41, 0x41};
    unsigned char aaaa[] = {0x41, 0x41, 0x41, 0x41};
    unsigned char aaaaaa[] = {0x41, 0x41, 0x41, 0x41, 0x41, 0x41};
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(aa, 2);
    char* hash_aa = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(aaaa, 4);
    char* hash_aaaa = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(aaaaaa, 6);
    char* hash_aaaaaa = calculateHashValue();
    
    printf("AA (2x):   %s\n", hash_aa);
    printf("AAAA (4x): %s\n", hash_aaaa);
    printf("AAAAAA(6x):%s\n", hash_aaaaaa);
    printf("All different: %s\n", 
           (strcmp(hash_aa, hash_aaaa) != 0 && strcmp(hash_aa, hash_aaaaaa) != 0 && strcmp(hash_aaaa, hash_aaaaaa) != 0) 
           ? "YES ✓" : "NO - PROBLEM!");
    
    free(hash_aa);
    free(hash_aaaa);
    free(hash_aaaaaa);
}

void test_byte_position_matters(void) {
    printf("\n=== Test 2: Same bytes, different positions ===\n");
    
    // Test if position matters with zero padding
    unsigned char pattern1[] = {0x42, 0x00, 0x00};
    unsigned char pattern2[] = {0x00, 0x42, 0x00};
    unsigned char pattern3[] = {0x00, 0x00, 0x42};
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(pattern1, 3);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(pattern2, 3);
    char* hash2 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(pattern3, 3);
    char* hash3 = calculateHashValue();
    
    printf("B at pos 0: %s\n", hash1);
    printf("B at pos 1: %s\n", hash2);
    printf("B at pos 2: %s\n", hash3);
    printf("All different: %s\n",
           (strcmp(hash1, hash2) != 0 && strcmp(hash1, hash3) != 0 && strcmp(hash2, hash3) != 0)
           ? "YES ✓" : "NO - PROBLEM!");
    
    free(hash1);
    free(hash2);
    free(hash3);
}

void test_permutations(void) {
    printf("\n=== Test 3: Permutations (order matters) ===\n");
    
    unsigned char abc[] = {0x41, 0x42, 0x43};
    unsigned char acb[] = {0x41, 0x43, 0x42};
    unsigned char bac[] = {0x42, 0x41, 0x43};
    unsigned char bca[] = {0x42, 0x43, 0x41};
    unsigned char cab[] = {0x43, 0x41, 0x42};
    unsigned char cba[] = {0x43, 0x42, 0x41};
    
    char* hashes[6];
    unsigned char* patterns[] = {abc, acb, bac, bca, cab, cba};
    const char* names[] = {"ABC", "ACB", "BAC", "BCA", "CAB", "CBA"};
    
    for (int i = 0; i < 6; i++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(patterns[i], 3);
        hashes[i] = calculateHashValue();
        printf("%s: %s\n", names[i], hashes[i]);
    }
    
    printf("\nChecking for collisions:\n");
    int collisions = 0;
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                printf("  COLLISION: %s == %s\n", names[i], names[j]);
                collisions++;
            }
        }
    }
    
    if (collisions == 0) {
        printf("  All different ✓\n");
    } else {
        printf("  Found %d collisions - PROBLEM!\n", collisions);
    }
    
    for (int i = 0; i < 6; i++) {
        free(hashes[i]);
    }
}

void test_alternating_patterns(void) {
    printf("\n=== Test 4: Alternating byte patterns ===\n");
    
    unsigned char ababab[] = {0x41, 0x42, 0x41, 0x42, 0x41, 0x42};
    unsigned char bababa[] = {0x42, 0x41, 0x42, 0x41, 0x42, 0x41};
    unsigned char aabbaa[] = {0x41, 0x41, 0x42, 0x42, 0x41, 0x41};
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(ababab, 6);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(bababa, 6);
    char* hash2 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(aabbaa, 6);
    char* hash3 = calculateHashValue();
    
    printf("ABABAB: %s\n", hash1);
    printf("BABABA: %s\n", hash2);
    printf("AABBAA: %s\n", hash3);
    printf("All different: %s\n",
           (strcmp(hash1, hash2) != 0 && strcmp(hash1, hash3) != 0 && strcmp(hash2, hash3) != 0)
           ? "YES ✓" : "NO - PROBLEM!");
    
    free(hash1);
    free(hash2);
    free(hash3);
}

void test_similar_patterns(void) {
    printf("\n=== Test 5: Very similar patterns ===\n");
    
    // Patterns that might collide without position mixing
    unsigned char p1[] = {0x01, 0x01, 0x01, 0x01};
    unsigned char p2[] = {0x01, 0x01, 0x01, 0x02};
    unsigned char p3[] = {0x01, 0x01, 0x02, 0x01};
    unsigned char p4[] = {0x01, 0x02, 0x01, 0x01};
    unsigned char p5[] = {0x02, 0x01, 0x01, 0x01};
    
    char* hashes[5];
    unsigned char* patterns[] = {p1, p2, p3, p4, p5};
    
    for (int i = 0; i < 5; i++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(patterns[i], 4);
        hashes[i] = calculateHashValue();
        printf("Pattern %d: %02X %02X %02X %02X -> %s\n", 
               i+1, patterns[i][0], patterns[i][1], patterns[i][2], patterns[i][3], hashes[i]);
    }
    
    printf("\nChecking for collisions:\n");
    int collisions = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                printf("  COLLISION: Pattern %d == Pattern %d\n", i+1, j+1);
                collisions++;
            }
        }
    }
    
    if (collisions == 0) {
        printf("  All different ✓\n");
    } else {
        printf("  Found %d collisions - PROBLEM!\n", collisions);
    }
    
    for (int i = 0; i < 5; i++) {
        free(hashes[i]);
    }
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  Position Mixing Necessity Test              ║\n");
    printf("║  (Testing without position XOR)              ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    test_repeated_bytes();
    test_byte_position_matters();
    test_permutations();
    test_alternating_patterns();
    test_similar_patterns();
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Test Complete                                ║\n");
    printf("║  If all tests show ✓, position mixing is     ║\n");
    printf("║  NOT needed. If problems found, it IS needed.║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    return 0;
}
