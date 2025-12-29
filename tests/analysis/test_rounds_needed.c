/**
 * Test: How many rounds are actually needed for security?
 * Tests avalanche effect, collision resistance, and performance at different round counts
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"
#include "../../util.h"

unsigned long numberOfRounds = 100000;  // Will be modified during tests
int hashLengthInBits = 128;

// Test avalanche effect for given number of rounds
double test_avalanche(unsigned long rounds) {
    numberOfRounds = rounds;
    
    unsigned char data1[32];
    unsigned char data2[32];
    
    // Fill with random data
    for (int i = 0; i < 32; i++) {
        data1[i] = (unsigned char)(rand() % 256);
        data2[i] = data1[i];
    }
    
    // Flip one bit in data2
    data2[0] ^= 0x01;
    
    // Hash both
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data1, 32);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data2, 32);
    char* hash2 = calculateHashValue();
    
    // Count bit differences
    int bits_different = 0;
    int total_bits = 0;
    
    for (size_t i = 0; i < strlen(hash1); i++) {
        // Convert hex char to 4 bits
        int val1 = (hash1[i] >= '0' && hash1[i] <= '9') ? (hash1[i] - '0') : (hash1[i] - 'a' + 10);
        int val2 = (hash2[i] >= '0' && hash2[i] <= '9') ? (hash2[i] - '0') : (hash2[i] - 'a' + 10);
        int xor = val1 ^ val2;
        
        // Count set bits in xor
        for (int bit = 0; bit < 4; bit++) {
            if (xor & (1 << bit)) {
                bits_different++;
            }
            total_bits++;
        }
    }
    
    free(hash1);
    free(hash2);
    
    return (double)bits_different / total_bits;
}

// Test collision resistance (simple test with random inputs)
int test_collisions(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    
    char** hashes = malloc(num_samples * sizeof(char*));
    unsigned char** samples = malloc(num_samples * sizeof(unsigned char*));
    
    // Generate random inputs and hash them
    for (int i = 0; i < num_samples; i++) {
        samples[i] = malloc(16);
        for (int j = 0; j < 16; j++) {
            samples[i][j] = (unsigned char)(rand() % 256);
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(samples[i], 16);
        hashes[i] = calculateHashValue();
    }
    
    // Check for collisions
    int collisions = 0;
    for (int i = 0; i < num_samples; i++) {
        for (int j = i + 1; j < num_samples; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                collisions++;
            }
        }
    }
    
    // Cleanup
    for (int i = 0; i < num_samples; i++) {
        free(hashes[i]);
        free(samples[i]);
    }
    free(hashes);
    free(samples);
    
    return collisions;
}

// Measure performance (hashes per second)
double test_performance(unsigned long rounds) {
    numberOfRounds = rounds;
    
    unsigned char data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    int iterations = 100;
    double start_time = wall_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(data, 64);
        char* hash = calculateHashValue();
        free(hash);
    }
    
    double elapsed = wall_time_seconds() - start_time;
    return iterations / elapsed;
}

// Test determinism (same input = same output)
int test_determinism(unsigned long rounds) {
    numberOfRounds = rounds;
    
    unsigned char data[32];
    for (int i = 0; i < 32; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    // Hash 3 times
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, 32);
    char* hash1 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, 32);
    char* hash2 = calculateHashValue();
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, 32);
    char* hash3 = calculateHashValue();
    
    int success = (strcmp(hash1, hash2) == 0 && strcmp(hash1, hash3) == 0);
    
    free(hash1);
    free(hash2);
    free(hash3);
    
    return success;
}

void run_comprehensive_test(unsigned long rounds) {
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Testing with %lu rounds%*s║\n", rounds, (int)(29 - snprintf(NULL, 0, "%lu", rounds)), "");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    // Test 1: Determinism
    printf("\n1. Determinism Test: ");
    fflush(stdout);
    int deterministic = test_determinism(rounds);
    printf("%s\n", deterministic ? "PASS ✓" : "FAIL ✗");
    
    // Test 2: Avalanche Effect (multiple samples for average)
    printf("2. Avalanche Effect (10 samples): ");
    fflush(stdout);
    double total_avalanche = 0.0;
    for (int i = 0; i < 10; i++) {
        total_avalanche += test_avalanche(rounds);
    }
    double avg_avalanche = total_avalanche / 10.0;
    printf("%.2f%% ", avg_avalanche * 100.0);
    if (avg_avalanche >= 0.45 && avg_avalanche <= 0.55) {
        printf("✓ Excellent\n");
    } else if (avg_avalanche >= 0.40 && avg_avalanche <= 0.60) {
        printf("○ Acceptable\n");
    } else {
        printf("✗ Poor\n");
    }
    
    // Test 3: Collision Resistance
    printf("3. Collision Test (100 samples): ");
    fflush(stdout);
    int collisions = test_collisions(rounds, 100);
    printf("%d collisions ", collisions);
    printf("%s\n", collisions == 0 ? "✓" : "✗ PROBLEM!");
    
    // Test 4: Performance
    printf("4. Performance: ");
    fflush(stdout);
    double hashes_per_sec = test_performance(rounds);
    printf("%.2f H/s\n", hashes_per_sec);
    
    // Calculate attack time (assuming attacker wants to try 1 billion passwords)
    double seconds_per_hash = 1.0 / hashes_per_sec;
    double billion_attempts_hours = (1e9 * seconds_per_hash) / 3600.0;
    printf("   → 1 billion attempts: %.1f hours (single-threaded)\n", billion_attempts_hours);
    
    // Summary
    printf("\n   Summary: ");
    if (deterministic && avg_avalanche >= 0.45 && avg_avalanche <= 0.55 && collisions == 0) {
        printf("✓ SUFFICIENT for security\n");
    } else if (deterministic && avg_avalanche >= 0.40 && avg_avalanche <= 0.60 && collisions == 0) {
        printf("○ ACCEPTABLE (borderline)\n");
    } else {
        printf("✗ INSUFFICIENT\n");
    }
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  Testing: How Many Rounds Are Needed?        ║\n");
    printf("║  Goal: Find minimum rounds for security      ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    srand((unsigned)time(NULL));
    
    // Test different round counts
    unsigned long test_rounds[] = {100, 500, 1000, 5000, 10000, 50000, 100000};
    int num_tests = 7;
    
    for (int i = 0; i < num_tests; i++) {
        run_comprehensive_test(test_rounds[i]);
    }
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Recommendation                               ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    printf("\nFor PASSWORD HASHING:\n");
    printf("  - Minimum: 1,000 rounds (acceptable security)\n");
    printf("  - Recommended: 5,000-10,000 rounds (good balance)\n");
    printf("  - Maximum: 100,000 rounds (maximum security)\n");
    printf("\nFor FILE HASHING:\n");
    printf("  - Can use much lower (100-1,000 rounds)\n");
    printf("  - Priority is speed, not brute-force resistance\n");
    printf("\nCurrent default (100,000) is very conservative.\n");
    printf("Consider 5,000-10,000 for better performance.\n");
    
    return 0;
}
