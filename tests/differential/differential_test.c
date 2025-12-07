/*
 * Differential Attack Test for Secasy Hash Function
 * 
 * This test attempts to exploit potential weaknesses by:
 * 1. Testing inputs with small, structured differences
 * 2. Looking for output correlations when inputs differ by known patterns
 * 3. Testing if certain input differences produce predictable output differences
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../util.h"

// Globals required by Secasy
unsigned long numberOfRounds = 1000;
int numberOfBits = 256;

// Compute hash and return hex string
char* compute_hash(const unsigned char* data, size_t len, int maxPrimeIndex) {
    initFieldWithDefaultNumbers(maxPrimeIndex);
    processBuffer(data, len);
    return calculateHashValue();
}

// Count differing bits between two hex strings
int hamming_distance_hex(const char* h1, const char* h2) {
    int dist = 0;
    size_t len = strlen(h1) < strlen(h2) ? strlen(h1) : strlen(h2);
    
    for (size_t i = 0; i < len; i++) {
        int v1 = (h1[i] >= 'a') ? (h1[i] - 'a' + 10) : 
                 (h1[i] >= 'A') ? (h1[i] - 'A' + 10) : (h1[i] - '0');
        int v2 = (h2[i] >= 'a') ? (h2[i] - 'a' + 10) : 
                 (h2[i] >= 'A') ? (h2[i] - 'A' + 10) : (h2[i] - '0');
        int xored = v1 ^ v2;
        while (xored) {
            dist += (xored & 1);
            xored >>= 1;
        }
    }
    return dist;
}

// Test 1: Sequential counter inputs (0, 1, 2, 3, ...)
void test_sequential_inputs(int count, int inputLen, int maxPrimeIndex) {
    printf("\n=== TEST 1: Sequential Counter Inputs ===\n");
    printf("Testing %d sequential values, input length: %d bytes\n\n", count, inputLen);
    
    unsigned char* input = calloc(inputLen, 1);
    char* prev_hash = NULL;
    
    double total_hamming = 0;
    int comparisons = 0;
    int min_hamming = INT_MAX;
    int max_hamming = 0;
    
    for (int i = 0; i < count; i++) {
        // Set input as counter value
        for (int b = 0; b < inputLen && b < 8; b++) {
            input[b] = (i >> (b * 8)) & 0xFF;
        }
        
        char* hash = compute_hash(input, inputLen, maxPrimeIndex);
        
        if (prev_hash) {
            int dist = hamming_distance_hex(prev_hash, hash);
            total_hamming += dist;
            comparisons++;
            if (dist < min_hamming) min_hamming = dist;
            if (dist > max_hamming) max_hamming = dist;
            free(prev_hash);
        }
        prev_hash = hash;
    }
    
    free(input);
    if (prev_hash) free(prev_hash);
    
    double mean = total_hamming / comparisons;
    size_t hash_bits = numberOfBits * 4; // Approximate bit count
    double expected = hash_bits * 0.5;
    
    printf("Results:\n");
    printf("  Comparisons: %d\n", comparisons);
    printf("  Mean Hamming distance: %.2f bits\n", mean);
    printf("  Expected (ideal): %.2f bits\n", expected);
    printf("  Min: %d, Max: %d\n", min_hamming, max_hamming);
    printf("  Deviation from ideal: %.2f%%\n", fabs(mean - expected) / expected * 100);
    
    if (fabs(mean - expected) / expected < 0.1) {
        printf("  Status: ✓ PASS - Good diffusion for sequential inputs\n");
    } else {
        printf("  Status: ⚠ WARNING - Potential weakness detected\n");
    }
}

// Test 2: Single-bit difference pairs
void test_single_bit_pairs(int count, int inputLen, int maxPrimeIndex) {
    printf("\n=== TEST 2: Single-Bit Difference Pairs ===\n");
    printf("Testing %d pairs with single-bit differences\n\n", count);
    
    srand(42);
    unsigned char* input1 = malloc(inputLen);
    unsigned char* input2 = malloc(inputLen);
    
    double total_hamming = 0;
    int min_hamming = INT_MAX;
    int max_hamming = 0;
    
    for (int i = 0; i < count; i++) {
        // Generate random input
        for (int b = 0; b < inputLen; b++) {
            input1[b] = rand() & 0xFF;
        }
        memcpy(input2, input1, inputLen);
        
        // Flip one random bit
        int byte_pos = rand() % inputLen;
        int bit_pos = rand() % 8;
        input2[byte_pos] ^= (1 << bit_pos);
        
        char* hash1 = compute_hash(input1, inputLen, maxPrimeIndex);
        char* hash2 = compute_hash(input2, inputLen, maxPrimeIndex);
        
        int dist = hamming_distance_hex(hash1, hash2);
        total_hamming += dist;
        if (dist < min_hamming) min_hamming = dist;
        if (dist > max_hamming) max_hamming = dist;
        
        free(hash1);
        free(hash2);
    }
    
    free(input1);
    free(input2);
    
    double mean = total_hamming / count;
    size_t hash_bits = numberOfBits * 4;
    double expected = hash_bits * 0.5;
    
    printf("Results:\n");
    printf("  Pairs tested: %d\n", count);
    printf("  Mean Hamming distance: %.2f bits\n", mean);
    printf("  Expected (ideal): %.2f bits\n", expected);
    printf("  Min: %d, Max: %d\n", min_hamming, max_hamming);
    printf("  Deviation from ideal: %.2f%%\n", fabs(mean - expected) / expected * 100);
    
    if (min_hamming < expected * 0.3) {
        printf("  Status: ⚠ WARNING - Low minimum distance found!\n");
    } else {
        printf("  Status: ✓ PASS - Good minimum distance\n");
    }
}

// Test 3: Related-key style inputs (same suffix, varying prefix)
void test_related_inputs(int count, int inputLen, int maxPrimeIndex) {
    printf("\n=== TEST 3: Related Inputs (Common Suffix) ===\n");
    printf("Testing %d inputs with same 50%% suffix\n\n", count);
    
    srand(123);
    int suffix_len = inputLen / 2;
    int prefix_len = inputLen - suffix_len;
    
    unsigned char* input = malloc(inputLen);
    unsigned char* suffix = malloc(suffix_len);
    
    // Generate fixed suffix
    for (int i = 0; i < suffix_len; i++) {
        suffix[i] = rand() & 0xFF;
    }
    
    char** hashes = malloc(count * sizeof(char*));
    
    // Generate hashes with varying prefix, fixed suffix
    for (int i = 0; i < count; i++) {
        for (int b = 0; b < prefix_len; b++) {
            input[b] = rand() & 0xFF;
        }
        memcpy(input + prefix_len, suffix, suffix_len);
        hashes[i] = compute_hash(input, inputLen, maxPrimeIndex);
    }
    
    // Check for collisions or near-collisions
    int collisions = 0;
    int near_collisions = 0; // < 10% of expected distance
    size_t hash_bits = numberOfBits * 4;
    double threshold = hash_bits * 0.1;
    
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            int dist = hamming_distance_hex(hashes[i], hashes[j]);
            if (dist == 0) collisions++;
            if (dist < threshold) near_collisions++;
        }
    }
    
    for (int i = 0; i < count; i++) free(hashes[i]);
    free(hashes);
    free(input);
    free(suffix);
    
    int total_pairs = count * (count - 1) / 2;
    
    printf("Results:\n");
    printf("  Total pairs compared: %d\n", total_pairs);
    printf("  Full collisions: %d\n", collisions);
    printf("  Near-collisions (< %.0f bits): %d\n", threshold, near_collisions);
    
    if (collisions > 0) {
        printf("  Status: ✗ FAIL - Collisions found with related inputs!\n");
    } else if (near_collisions > total_pairs * 0.01) {
        printf("  Status: ⚠ WARNING - High near-collision rate\n");
    } else {
        printf("  Status: ✓ PASS - No structural weakness detected\n");
    }
}

// Test 4: All-zeros vs sparse inputs
void test_sparse_inputs(int maxPrimeIndex) {
    printf("\n=== TEST 4: Sparse Input Patterns ===\n");
    printf("Testing inputs with minimal entropy\n\n");
    
    int inputLen = 32;
    unsigned char* input = calloc(inputLen, 1);
    
    // Test various sparse patterns
    struct {
        const char* name;
        void (*setup)(unsigned char*, int);
    } patterns[] = {
        {"All zeros", NULL},
    };
    
    // All zeros
    memset(input, 0x00, inputLen);
    char* hash_zeros = compute_hash(input, inputLen, maxPrimeIndex);
    
    // All ones
    memset(input, 0xFF, inputLen);
    char* hash_ones = compute_hash(input, inputLen, maxPrimeIndex);
    
    // Single byte set
    memset(input, 0x00, inputLen);
    input[0] = 0x01;
    char* hash_single = compute_hash(input, inputLen, maxPrimeIndex);
    
    // Alternating
    for (int i = 0; i < inputLen; i++) input[i] = (i % 2) ? 0xFF : 0x00;
    char* hash_alt = compute_hash(input, inputLen, maxPrimeIndex);
    
    int dist_zeros_ones = hamming_distance_hex(hash_zeros, hash_ones);
    int dist_zeros_single = hamming_distance_hex(hash_zeros, hash_single);
    int dist_zeros_alt = hamming_distance_hex(hash_zeros, hash_alt);
    
    size_t hash_bits = numberOfBits * 4;
    double expected = hash_bits * 0.5;
    
    printf("Results:\n");
    printf("  Zeros vs Ones: %d bits (expected: %.0f)\n", dist_zeros_ones, expected);
    printf("  Zeros vs Single-byte: %d bits (expected: %.0f)\n", dist_zeros_single, expected);
    printf("  Zeros vs Alternating: %d bits (expected: %.0f)\n", dist_zeros_alt, expected);
    
    int passed = 1;
    if (dist_zeros_ones < expected * 0.4) { printf("  ⚠ Zeros-Ones distance too low!\n"); passed = 0; }
    if (dist_zeros_single < expected * 0.4) { printf("  ⚠ Zeros-Single distance too low!\n"); passed = 0; }
    if (dist_zeros_alt < expected * 0.4) { printf("  ⚠ Zeros-Alt distance too low!\n"); passed = 0; }
    
    if (passed) {
        printf("  Status: ✓ PASS - Good diffusion for sparse inputs\n");
    }
    
    free(input);
    free(hash_zeros);
    free(hash_ones);
    free(hash_single);
    free(hash_alt);
}

// Test 5: Length extension style test
void test_length_extension(int maxPrimeIndex) {
    printf("\n=== TEST 5: Length Extension Pattern ===\n");
    printf("Testing if hash(M) relates to hash(M||suffix)\n\n");
    
    unsigned char msg1[] = "Hello World!";
    unsigned char msg2[] = "Hello World!AAAA";
    unsigned char msg3[] = "Hello World!BBBB";
    
    char* hash1 = compute_hash(msg1, strlen((char*)msg1), maxPrimeIndex);
    char* hash2 = compute_hash(msg2, strlen((char*)msg2), maxPrimeIndex);
    char* hash3 = compute_hash(msg3, strlen((char*)msg3), maxPrimeIndex);
    
    int dist_1_2 = hamming_distance_hex(hash1, hash2);
    int dist_1_3 = hamming_distance_hex(hash1, hash3);
    int dist_2_3 = hamming_distance_hex(hash2, hash3);
    
    size_t hash_bits = numberOfBits * 4;
    double expected = hash_bits * 0.5;
    
    printf("Results:\n");
    printf("  hash(M) vs hash(M||AAAA): %d bits (expected: %.0f)\n", dist_1_2, expected);
    printf("  hash(M) vs hash(M||BBBB): %d bits (expected: %.0f)\n", dist_1_3, expected);
    printf("  hash(M||AAAA) vs hash(M||BBBB): %d bits (expected: %.0f)\n", dist_2_3, expected);
    
    if (dist_1_2 > expected * 0.4 && dist_1_3 > expected * 0.4 && dist_2_3 > expected * 0.4) {
        printf("  Status: ✓ PASS - No length extension weakness\n");
    } else {
        printf("  Status: ⚠ WARNING - Potential length extension issue\n");
    }
    
    free(hash1);
    free(hash2);
    free(hash3);
}

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("Secasy Differential Attack Test Suite\n");
    printf("========================================\n");
    
    int maxPrimeIndex = 500;
    numberOfRounds = 1000;
    numberOfBits = 256;
    
    if (argc > 1) maxPrimeIndex = atoi(argv[1]);
    if (argc > 2) numberOfRounds = atol(argv[2]);
    
    printf("Configuration:\n");
    printf("  Max Prime Index: %d\n", maxPrimeIndex);
    printf("  Rounds: %lu\n", numberOfRounds);
    printf("  Hash Bits: %d\n", numberOfBits);
    
    clock_t start = clock();
    
    test_sequential_inputs(1000, 16, maxPrimeIndex);
    test_single_bit_pairs(500, 16, maxPrimeIndex);
    test_related_inputs(100, 32, maxPrimeIndex);
    test_sparse_inputs(maxPrimeIndex);
    test_length_extension(maxPrimeIndex);
    
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    printf("\n========================================\n");
    printf("All tests completed in %.2f seconds\n", elapsed);
    printf("========================================\n");
    
    return 0;
}
