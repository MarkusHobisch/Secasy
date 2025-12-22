/**
 * Simple functional tests to verify basic hash behavior
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"

unsigned long numberOfRounds = 1000;
int hashLengthInBits = 128;

void test_deterministic(void) {
    printf("\n=== Test 1: Deterministic (same input = same hash) ===\n");
    
    unsigned char data[] = "Hello, World!";
    size_t len = strlen((char*)data);
    
    char* hash1;
    char* hash2;
    char* hash3;
    
    // First hash
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    hash1 = calculateHashValue();
    
    // Second hash (should be identical)
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    hash2 = calculateHashValue();
    
    // Third hash (should be identical)
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    hash3 = calculateHashValue();
    
    printf("Hash 1: %s\n", hash1);
    printf("Hash 2: %s\n", hash2);
    printf("Hash 3: %s\n", hash3);
    
    int success = (strcmp(hash1, hash2) == 0 && strcmp(hash1, hash3) == 0);
    printf("Result: %s\n", success ? "PASS ✓ - Deterministic" : "FAIL ✗");
    
    free(hash1);
    free(hash2);
    free(hash3);
}

void test_different_inputs(void) {
    printf("\n=== Test 2: Different inputs = different hashes ===\n");
    
    unsigned char data1[] = "test";
    unsigned char data2[] = "Test";  // Different case
    unsigned char data3[] = "test1"; // Extra char
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data1, strlen((char*)data1));
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data2, strlen((char*)data2));
    char* hash2 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data3, strlen((char*)data3));
    char* hash3 = calculateHashValue();
    
    printf("'test':  %s\n", hash1);
    printf("'Test':  %s\n", hash2);
    printf("'test1': %s\n", hash3);
    
    int all_different = (strcmp(hash1, hash2) != 0 && 
                         strcmp(hash1, hash3) != 0 && 
                         strcmp(hash2, hash3) != 0);
    printf("Result: %s\n", all_different ? "PASS ✓ - All different" : "FAIL ✗");
    
    free(hash1);
    free(hash2);
    free(hash3);
}

void test_empty_input(void) {
    printf("\n=== Test 3: Empty input handling ===\n");
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(NULL, 0);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer((unsigned char*)"", 0);
    char* hash2 = calculateHashValue();
    
    printf("Empty (NULL): %s\n", hash1);
    printf("Empty (\"\"):   %s\n", hash2);
    printf("Result: %s\n", strcmp(hash1, hash2) == 0 ? "PASS ✓ - Consistent empty handling" : "FAIL ✗");
    
    free(hash1);
    free(hash2);
}

void test_length_sensitivity(void) {
    printf("\n=== Test 4: Length sensitivity ===\n");
    
    unsigned char data1[] = "a";
    unsigned char data2[] = "aa";
    unsigned char data3[] = "aaa";
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data1, 1);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data2, 2);
    char* hash2 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data3, 3);
    char* hash3 = calculateHashValue();
    
    printf("'a':   %s\n", hash1);
    printf("'aa':  %s\n", hash2);
    printf("'aaa': %s\n", hash3);
    
    int all_different = (strcmp(hash1, hash2) != 0 && 
                         strcmp(hash1, hash3) != 0 && 
                         strcmp(hash2, hash3) != 0);
    printf("Result: %s\n", all_different ? "PASS ✓ - Length sensitive" : "FAIL ✗");
    
    free(hash1);
    free(hash2);
    free(hash3);
}

void test_binary_data(void) {
    printf("\n=== Test 5: Binary data (all byte values) ===\n");
    
    unsigned char binary[256];
    for (int i = 0; i < 256; i++) {
        binary[i] = (unsigned char)i;
    }
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(binary, 256);
    char* hash = calculateHashValue();
    
    printf("All bytes 0x00-0xFF: %s\n", hash);
    printf("Result: %s\n", hash != NULL && strlen(hash) == 32 ? "PASS ✓ - Binary data handled" : "FAIL ✗");
    
    free(hash);
}

void test_hash_length(void) {
    printf("\n=== Test 6: Hash length verification ===\n");
    
    unsigned char data[] = "test data";
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, strlen((char*)data));
    char* hash = calculateHashValue();
    
    size_t hash_len = strlen(hash);
    int expected_len = hashLengthInBits / 4; // 4 bits per hex char
    
    printf("Hash: %s\n", hash);
    printf("Expected length: %d hex chars (%d bits)\n", expected_len, hashLengthInBits);
    printf("Actual length: %zu hex chars\n", hash_len);
    printf("Result: %s\n", hash_len == (size_t)expected_len ? "PASS ✓ - Correct length" : "FAIL ✗");
    
    free(hash);
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  Simple Functional Tests                      ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    test_deterministic();
    test_different_inputs();
    test_empty_input();
    test_length_sensitivity();
    test_binary_data();
    test_hash_length();
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  All Basic Tests Complete                     ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    return 0;
}
