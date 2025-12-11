/**
 * Compare fields AFTER processing phase (calculateHashValue runs the processing)
 * We need to see what happens during the numberOfRounds iterations
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

int main(void) {
    printf("=== Analyzing WHY collision occurs ===\n\n");
    
    // Test 1: Check generateHashValue() output
    printf("[1] Checking generateHashValue() before processing:\n");
    
    unsigned char input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    long long hash1_pre = generateHashValue();
    printf("  0x66 generateHashValue (pre-calc): %lld\n", hash1_pre);
    
    unsigned char input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    long long hash2_pre = generateHashValue();
    printf("  0x69 generateHashValue (pre-calc): %lld\n", hash2_pre);
    
    printf("  Match: %s\n\n", hash1_pre == hash2_pre ? "YES - COLLISION AT generateHashValue!" : "NO");
    
    // Test 2: Check row/column sums
    printf("[2] Checking row and column sums:\n");
    
    long long rowSums1[FIELD_SIZE], colSums1[FIELD_SIZE];
    long long rowSums2[FIELD_SIZE], colSums2[FIELD_SIZE];
    
    input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    calcSumOfRows(rowSums1);
    calcSumOfColumns(colSums1);
    
    input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    calcSumOfRows(rowSums2);
    calcSumOfColumns(colSums2);
    
    printf("  0x66 row sums: ");
    for (int i = 0; i < FIELD_SIZE; i++) printf("%lld ", rowSums1[i]);
    printf("\n  0x69 row sums: ");
    for (int i = 0; i < FIELD_SIZE; i++) printf("%lld ", rowSums2[i]);
    printf("\n");
    
    printf("  0x66 col sums: ");
    for (int i = 0; i < FIELD_SIZE; i++) printf("%lld ", colSums1[i]);
    printf("\n  0x69 col sums: ");
    for (int i = 0; i < FIELD_SIZE; i++) printf("%lld ", colSums2[i]);
    printf("\n");
    
    // Test 3: Check products
    printf("\n[3] Checking products:\n");
    
    input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    long long prod1 = calcSumOfProducts();
    
    input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    long long prod2 = calcSumOfProducts();
    
    printf("  0x66 calcSumOfProducts: %lld\n", prod1);
    printf("  0x69 calcSumOfProducts: %lld\n", prod2);
    printf("  Match: %s\n", prod1 == prod2 ? "YES!" : "NO");
    
    // Test 4: Full hash value
    printf("\n[4] Full calculateHashValue() output:\n");
    
    input1 = 0x66;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input1, 1);
    char* hash1 = calculateHashValue();
    
    input2 = 0x69;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(&input2, 1);
    char* hash2 = calculateHashValue();
    
    printf("  0x66 hash: %s\n", hash1);
    printf("  0x69 hash: %s\n", hash2);
    printf("  Match: %s\n", strcmp(hash1, hash2) == 0 ? "YES - FULL COLLISION!" : "NO");
    
    free(hash1);
    free(hash2);
    
    return 0;
}
