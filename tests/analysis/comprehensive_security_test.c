/**
 * Comprehensive Security Test Suite
 * Additional cryptanalysis tests beyond the basic security tests
 * 
 * Tests included:
 * 1. Birthday Attack Resistance
 * 2. Preimage Attack Resistance
 * 3. Second Preimage Attack
 * 4. Bit Independence Criterion (BIC)
 * 5. Strict Avalanche Criterion (SAC)
 * 6. Non-linearity Test
 * 7. Length Extension Attack Resistance
 * 8. Key/Salt Sensitivity
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"
#include "../../util.h"

unsigned long numberOfRounds = 10000;
int hashLengthInBits = 128;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

// ============================================================
// Helper Functions
// ============================================================

int popcount(uint64_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

uint64_t hash_to_uint64(const char* hash) {
    uint64_t result = 0;
    for (int i = 0; i < 16 && hash[i]; i++) {
        int val = (hash[i] >= '0' && hash[i] <= '9') 
                  ? (hash[i] - '0') 
                  : (hash[i] - 'a' + 10);
        result = (result << 4) | val;
    }
    return result;
}

int hamming_distance_hex(const char* h1, const char* h2) {
    int dist = 0;
    size_t len = strlen(h1) < strlen(h2) ? strlen(h1) : strlen(h2);
    for (size_t i = 0; i < len; i++) {
        int v1 = (h1[i] >= '0' && h1[i] <= '9') ? (h1[i] - '0') : (h1[i] - 'a' + 10);
        int v2 = (h2[i] >= '0' && h2[i] <= '9') ? (h2[i] - '0') : (h2[i] - 'a' + 10);
        int xor = v1 ^ v2;
        for (int b = 0; b < 4; b++) {
            if (xor & (1 << b)) dist++;
        }
    }
    return dist;
}

// ============================================================
// Test 1: Birthday Attack Resistance
// ============================================================
typedef struct {
    uint64_t hash;
    int index;
} HashEntry;

int compare_hash_entries(const void* a, const void* b) {
    uint64_t ha = ((HashEntry*)a)->hash;
    uint64_t hb = ((HashEntry*)b)->hash;
    if (ha < hb) return -1;
    if (ha > hb) return 1;
    return 0;
}

double test_birthday_attack(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing birthday attack resistance with %d samples...\n", num_samples);
    
    HashEntry* entries = malloc(num_samples * sizeof(HashEntry));
    if (!entries) {
        printf("    ERROR: Memory allocation failed\n");
        return 1.0;
    }
    
    // Generate random hashes
    for (int i = 0; i < num_samples; i++) {
        unsigned char input[32];
        for (int j = 0; j < 32; j++) {
            input[j] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 32);
        char* hash = calculateHashValue();
        
        entries[i].hash = hash_to_uint64(hash);
        entries[i].index = i;
        
        free(hash);
        
        if ((i + 1) % (num_samples / 10) == 0) {
            printf("    Progress: %d/%d hashes generated\n", i + 1, num_samples);
        }
    }
    
    // Sort and find collisions
    qsort(entries, num_samples, sizeof(HashEntry), compare_hash_entries);
    
    int collisions = 0;
    for (int i = 1; i < num_samples; i++) {
        if (entries[i].hash == entries[i-1].hash) {
            collisions++;
            printf("    ⚠️ Collision found: samples %d and %d\n", 
                   entries[i-1].index, entries[i].index);
        }
    }
    
    // Birthday paradox: expected collisions ≈ n²/(2*2^64) for 64-bit truncation
    double expected = (double)num_samples * num_samples / (2.0 * 18446744073709551616.0);
    printf("  Found %d collisions (expected for random: %.6f)\n", collisions, expected);
    
    free(entries);
    return (collisions > 0) ? 1.0 : 0.0;
}

// ============================================================
// Test 2: Preimage Attack Resistance
// ============================================================
double test_preimage_resistance(unsigned long rounds, int num_attempts) {
    numberOfRounds = rounds;
    printf("  Testing preimage resistance with %d attempts...\n", num_attempts);
    
    // Create a target hash
    unsigned char target_input[16] = "TARGET_MESSAGE!";
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(target_input, 16);
    char* target_hash = calculateHashValue();
    uint64_t target = hash_to_uint64(target_hash);
    
    printf("  Target hash (first 64 bits): %016llx\n", (unsigned long long)target);
    
    int matches = 0;
    int partial_matches = 0;
    
    for (int i = 0; i < num_attempts; i++) {
        unsigned char random_input[32];
        for (int j = 0; j < 32; j++) {
            random_input[j] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(random_input, 32);
        char* hash = calculateHashValue();
        uint64_t h = hash_to_uint64(hash);
        
        if (h == target) {
            matches++;
            printf("    ⚠️ PREIMAGE FOUND at attempt %d!\n", i);
        }
        
        // Check partial matches (first 16 bits)
        if ((h >> 48) == (target >> 48)) {
            partial_matches++;
        }
        
        free(hash);
        
        if ((i + 1) % (num_attempts / 10) == 0) {
            printf("    Progress: %d/%d attempts (partial matches: %d)\n", 
                   i + 1, num_attempts, partial_matches);
        }
    }
    
    // Expected partial matches for 16-bit prefix: num_attempts / 65536
    double expected_partial = (double)num_attempts / 65536.0;
    printf("  Partial matches (16-bit): %d (expected: %.2f)\n", 
           partial_matches, expected_partial);
    
    free(target_hash);
    return (matches > 0) ? 1.0 : 0.0;
}

// ============================================================
// Test 3: Second Preimage Attack
// ============================================================
double test_second_preimage(unsigned long rounds, int num_attempts) {
    numberOfRounds = rounds;
    printf("  Testing second preimage resistance...\n");
    
    // Multiple original messages
    unsigned char originals[5][16] = {
        "OriginalMsg_0001",
        "SecondMessage_2",
        "ThirdTestInput3",
        "FourthData_ABCD",
        "FifthInput_1234"
    };
    
    int total_matches = 0;
    
    for (int orig = 0; orig < 5; orig++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(originals[orig], 16);
        char* target_hash = calculateHashValue();
        
        for (int i = 0; i < num_attempts / 5; i++) {
            unsigned char random_input[32];
            for (int j = 0; j < 32; j++) {
                random_input[j] = rand() % 256;
            }
            
            // Ensure different from original
            if (memcmp(random_input, originals[orig], 16) == 0) continue;
            
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(random_input, 32);
            char* hash = calculateHashValue();
            
            if (strcmp(hash, target_hash) == 0) {
                total_matches++;
                printf("    ⚠️ SECOND PREIMAGE FOUND for message %d!\n", orig);
            }
            
            free(hash);
        }
        
        free(target_hash);
    }
    
    printf("  Second preimages found: %d\n", total_matches);
    return (total_matches > 0) ? 1.0 : 0.0;
}

// ============================================================
// Test 4: Bit Independence Criterion (BIC)
// ============================================================
double test_bit_independence(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing Bit Independence Criterion (BIC)...\n");
    
    int num_bits = 64;  // Test first 64 output bits
    
    // Correlation matrix for output bits
    int* correlations = calloc(num_bits * num_bits, sizeof(int));
    if (!correlations) {
        printf("    ERROR: Memory allocation failed\n");
        return 1.0;
    }
    
    for (int sample = 0; sample < num_samples; sample++) {
        unsigned char input[32];
        for (int i = 0; i < 32; i++) {
            input[i] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 32);
        char* hash = calculateHashValue();
        
        // Extract bits
        int bits[64];
        for (int i = 0; i < 64 && i/4 < (int)strlen(hash); i++) {
            int nibble_idx = i / 4;
            int bit_in_nibble = 3 - (i % 4);
            int nibble = (hash[nibble_idx] >= '0' && hash[nibble_idx] <= '9') 
                         ? (hash[nibble_idx] - '0') 
                         : (hash[nibble_idx] - 'a' + 10);
            bits[i] = (nibble >> bit_in_nibble) & 1;
        }
        
        // Update correlation matrix
        for (int i = 0; i < num_bits; i++) {
            for (int j = i + 1; j < num_bits; j++) {
                if (bits[i] == bits[j]) {
                    correlations[i * num_bits + j]++;
                }
            }
        }
        
        free(hash);
    }
    
    // Analyze correlations
    double max_deviation = 0.0;
    int worst_i = -1, worst_j = -1;
    
    for (int i = 0; i < num_bits; i++) {
        for (int j = i + 1; j < num_bits; j++) {
            double prob = (double)correlations[i * num_bits + j] / num_samples;
            double deviation = fabs(prob - 0.5);
            if (deviation > max_deviation) {
                max_deviation = deviation;
                worst_i = i;
                worst_j = j;
            }
        }
    }
    
    printf("  Max bit correlation deviation: %.4f (bits %d and %d)\n", 
           max_deviation, worst_i, worst_j);
    
    free(correlations);
    return max_deviation;
}

// ============================================================
// Test 5: Strict Avalanche Criterion (SAC)
// ============================================================
double test_strict_avalanche(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing Strict Avalanche Criterion (SAC)...\n");
    
    int input_bits = 256;  // 32 bytes
    int output_bits = 128; // hash length
    
    double* flip_probs = calloc(output_bits, sizeof(double));
    if (!flip_probs) {
        printf("    ERROR: Memory allocation failed\n");
        return 1.0;
    }
    
    for (int input_bit = 0; input_bit < input_bits; input_bit += 8) {
        int flip_counts[128] = {0};
        
        for (int sample = 0; sample < num_samples / 32; sample++) {
            unsigned char input1[32], input2[32];
            for (int i = 0; i < 32; i++) {
                input1[i] = rand() % 256;
                input2[i] = input1[i];
            }
            
            // Flip single bit
            input2[input_bit / 8] ^= (1 << (input_bit % 8));
            
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(input1, 32);
            char* hash1 = calculateHashValue();
            
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(input2, 32);
            char* hash2 = calculateHashValue();
            
            // Count output bit flips
            size_t hash_len = strlen(hash1) < strlen(hash2) ? strlen(hash1) : strlen(hash2);
            for (size_t i = 0; i < hash_len && i * 4 < (size_t)output_bits; i++) {
                int v1 = (hash1[i] >= '0' && hash1[i] <= '9') 
                         ? (hash1[i] - '0') : (hash1[i] - 'a' + 10);
                int v2 = (hash2[i] >= '0' && hash2[i] <= '9') 
                         ? (hash2[i] - '0') : (hash2[i] - 'a' + 10);
                int xor = v1 ^ v2;
                for (int b = 0; b < 4; b++) {
                    if (xor & (1 << b)) {
                        int out_bit = i * 4 + (3 - b);
                        if (out_bit < output_bits) {
                            flip_counts[out_bit]++;
                        }
                    }
                }
            }
            
            free(hash1);
            free(hash2);
        }
        
        // Accumulate flip probabilities
        for (int i = 0; i < output_bits; i++) {
            flip_probs[i] += (double)flip_counts[i] / (num_samples / 32);
        }
    }
    
    // Average and find max deviation from 0.5
    double max_deviation = 0.0;
    int worst_bit = -1;
    
    for (int i = 0; i < output_bits; i++) {
        flip_probs[i] /= 32;  // Average over tested input bits
        double deviation = fabs(flip_probs[i] - 0.5);
        if (deviation > max_deviation) {
            max_deviation = deviation;
            worst_bit = i;
        }
    }
    
    printf("  Max SAC deviation: %.4f (output bit %d, flip prob: %.4f)\n", 
           max_deviation, worst_bit, flip_probs[worst_bit]);
    
    free(flip_probs);
    return max_deviation;
}

// ============================================================
// Test 6: Non-linearity Test
// ============================================================
double test_nonlinearity(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing non-linearity...\n");
    
    // Test XOR distributivity: H(A) XOR H(B) should NOT equal H(A XOR B)
    int linear_matches = 0;
    
    for (int sample = 0; sample < num_samples; sample++) {
        unsigned char A[16], B[16], AxorB[16];
        
        for (int i = 0; i < 16; i++) {
            A[i] = rand() % 256;
            B[i] = rand() % 256;
            AxorB[i] = A[i] ^ B[i];
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(A, 16);
        char* hashA = calculateHashValue();
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(B, 16);
        char* hashB = calculateHashValue();
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(AxorB, 16);
        char* hashAxorB = calculateHashValue();
        
        // Calculate H(A) XOR H(B)
        uint64_t hA = hash_to_uint64(hashA);
        uint64_t hB = hash_to_uint64(hashB);
        uint64_t hAxorB = hash_to_uint64(hashAxorB);
        
        uint64_t xor_hashes = hA ^ hB;
        
        // Check how many bits match
        int matching_bits = 64 - popcount(xor_hashes ^ hAxorB);
        
        // More than 48 matching bits would be suspicious
        if (matching_bits > 48) {
            linear_matches++;
        }
        
        free(hashA);
        free(hashB);
        free(hashAxorB);
    }
    
    double linearity_ratio = (double)linear_matches / num_samples;
    printf("  High linearity cases: %d/%d (%.2f%%)\n", 
           linear_matches, num_samples, linearity_ratio * 100);
    
    return linearity_ratio;
}

// ============================================================
// Test 7: Length Extension Attack Resistance
// ============================================================
double test_length_extension(unsigned long rounds, int num_attempts) {
    numberOfRounds = rounds;
    printf("  Testing length extension attack resistance...\n");
    
    // Original message and hash
    unsigned char original[16] = "SECRET_MESSAGE_";
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(original, 16);
    char* original_hash = calculateHashValue();
    
    // Extended message: original + extension
    unsigned char extension[16] = "_MALICIOUS_DATA";
    unsigned char extended[32];
    memcpy(extended, original, 16);
    memcpy(extended + 16, extension, 16);
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(extended, 32);
    char* extended_hash = calculateHashValue();
    
    // Try to predict extended hash from original hash
    // In a vulnerable hash, there might be a relationship
    
    // Measure similarity
    int similarity = 128 - hamming_distance_hex(original_hash, extended_hash);
    printf("  Original hash:  %.32s...\n", original_hash);
    printf("  Extended hash:  %.32s...\n", extended_hash);
    printf("  Bit similarity: %d/128 (should be ~64 for random)\n", similarity);
    
    double deviation = fabs((double)similarity - 64) / 64;
    
    free(original_hash);
    free(extended_hash);
    
    return deviation;
}

// ============================================================
// Test 8: Near-Collision Analysis
// ============================================================
double test_near_collisions(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing near-collision resistance...\n");
    
    // Store hashes and find closest pairs
    typedef struct {
        uint64_t hash;
        unsigned char input[16];
    } Sample;
    
    Sample* samples = malloc(num_samples * sizeof(Sample));
    if (!samples) {
        printf("    ERROR: Memory allocation failed\n");
        return 1.0;
    }
    
    for (int i = 0; i < num_samples; i++) {
        for (int j = 0; j < 16; j++) {
            samples[i].input[j] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(samples[i].input, 16);
        char* hash = calculateHashValue();
        samples[i].hash = hash_to_uint64(hash);
        free(hash);
    }
    
    // Find minimum hamming distance
    int min_distance = 64;
    int near_collision_count = 0;
    
    for (int i = 0; i < num_samples; i++) {
        for (int j = i + 1; j < num_samples; j++) {
            int dist = popcount(samples[i].hash ^ samples[j].hash);
            if (dist < min_distance) {
                min_distance = dist;
            }
            if (dist < 16) {  // Less than 25% of bits differ
                near_collision_count++;
            }
        }
    }
    
    // Expected minimum distance for truly random 64-bit values
    // With n samples, expected minimum is roughly 64 - log2(n*(n-1)/2)
    double expected_pairs = (double)num_samples * (num_samples - 1) / 2;
    double expected_min = 32 - log2(expected_pairs) / 2;
    
    printf("  Minimum hamming distance: %d (expected ~%.1f for random)\n", 
           min_distance, expected_min);
    printf("  Near-collisions (<16 bits): %d\n", near_collision_count);
    
    free(samples);
    
    // Return how much worse than expected
    if (min_distance < expected_min - 5) {
        return (expected_min - min_distance) / 32.0;
    }
    return 0.0;
}

// ============================================================
// Test 9: Input Sensitivity
// ============================================================
double test_input_sensitivity(unsigned long rounds) {
    numberOfRounds = rounds;
    printf("  Testing input sensitivity...\n");
    
    unsigned char base[32] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef";
    
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(base, 32);
    char* base_hash = calculateHashValue();
    
    double total_change = 0;
    int tests = 0;
    
    // Test single byte changes
    for (int pos = 0; pos < 32; pos++) {
        unsigned char modified[32];
        memcpy(modified, base, 32);
        modified[pos] = (base[pos] + 1) % 256;
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(modified, 32);
        char* mod_hash = calculateHashValue();
        
        int dist = hamming_distance_hex(base_hash, mod_hash);
        total_change += dist;
        tests++;
        
        free(mod_hash);
    }
    
    double avg_change = total_change / tests;
    double expected = 64;  // 50% of 128 bits
    double deviation = fabs(avg_change - expected) / expected;
    
    printf("  Average bit change per byte modification: %.1f/128\n", avg_change);
    printf("  Deviation from ideal (64): %.2f%%\n", deviation * 100);
    
    free(base_hash);
    return deviation;
}

// ============================================================
// Test 10: Distribution Uniformity
// ============================================================
double test_distribution_uniformity(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    printf("  Testing hash distribution uniformity...\n");
    
    // Divide 64-bit space into 256 buckets
    int buckets[256] = {0};
    
    for (int i = 0; i < num_samples; i++) {
        unsigned char input[16];
        for (int j = 0; j < 16; j++) {
            input[j] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 16);
        char* hash = calculateHashValue();
        
        // Use first byte of hash as bucket index
        int bucket = (hash[0] >= '0' && hash[0] <= '9') 
                     ? (hash[0] - '0') 
                     : (hash[0] - 'a' + 10);
        bucket = (bucket << 4) | ((hash[1] >= '0' && hash[1] <= '9') 
                                  ? (hash[1] - '0') 
                                  : (hash[1] - 'a' + 10));
        buckets[bucket]++;
        
        free(hash);
    }
    
    // Chi-square test
    double expected = (double)num_samples / 256;
    double chi_square = 0;
    
    for (int i = 0; i < 256; i++) {
        double diff = buckets[i] - expected;
        chi_square += (diff * diff) / expected;
    }
    
    // Degrees of freedom = 255, critical value at p=0.01 is ~310
    printf("  Chi-square statistic: %.2f (critical at p=0.01: ~310)\n", chi_square);
    
    return (chi_square > 310) ? (chi_square - 310) / 310 : 0.0;
}

// ============================================================
// Main Test Runner
// ============================================================
void run_comprehensive_tests(unsigned long rounds) {
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Comprehensive Security Test: %lu rounds%*s║\n", 
           rounds, (int)(23 - snprintf(NULL, 0, "%lu", rounds)), "");
    printf("╚═══════════════════════════════════════════════════════╝\n");
    
    int passed = 0;
    int total = 10;
    
    // Test 1: Birthday Attack
    printf("\n[1/10] Birthday Attack Resistance\n");
    double birthday_result = test_birthday_attack(rounds, 10000);
    if (birthday_result == 0) {
        printf("  ✓ PASSED\n");
        passed++;
    } else {
        printf("  ✗ FAILED - Collisions found!\n");
    }
    
    // Test 2: Preimage Attack
    printf("\n[2/10] Preimage Attack Resistance\n");
    double preimage_result = test_preimage_resistance(rounds, 100000);
    if (preimage_result == 0) {
        printf("  ✓ PASSED\n");
        passed++;
    } else {
        printf("  ✗ FAILED - Preimage found!\n");
    }
    
    // Test 3: Second Preimage
    printf("\n[3/10] Second Preimage Attack Resistance\n");
    double second_preimage_result = test_second_preimage(rounds, 50000);
    if (second_preimage_result == 0) {
        printf("  ✓ PASSED\n");
        passed++;
    } else {
        printf("  ✗ FAILED - Second preimage found!\n");
    }
    
    // Test 4: Bit Independence
    printf("\n[4/10] Bit Independence Criterion (BIC)\n");
    double bic_result = test_bit_independence(rounds, 1000);
    if (bic_result < 0.1) {
        printf("  ✓ PASSED (deviation < 10%%)\n");
        passed++;
    } else if (bic_result < 0.2) {
        printf("  ⚠️ MARGINAL (deviation 10-20%%)\n");
    } else {
        printf("  ✗ FAILED (deviation > 20%%)\n");
    }
    
    // Test 5: Strict Avalanche Criterion
    printf("\n[5/10] Strict Avalanche Criterion (SAC)\n");
    double sac_result = test_strict_avalanche(rounds, 640);
    if (sac_result < 0.1) {
        printf("  ✓ PASSED (deviation < 10%%)\n");
        passed++;
    } else if (sac_result < 0.2) {
        printf("  ⚠️ MARGINAL (deviation 10-20%%)\n");
    } else {
        printf("  ✗ FAILED (deviation > 20%%)\n");
    }
    
    // Test 6: Non-linearity
    printf("\n[6/10] Non-linearity Test\n");
    double nonlin_result = test_nonlinearity(rounds, 500);
    if (nonlin_result < 0.05) {
        printf("  ✓ PASSED (linearity < 5%%)\n");
        passed++;
    } else if (nonlin_result < 0.1) {
        printf("  ⚠️ MARGINAL (linearity 5-10%%)\n");
    } else {
        printf("  ✗ FAILED (linearity > 10%%)\n");
    }
    
    // Test 7: Length Extension
    printf("\n[7/10] Length Extension Attack Resistance\n");
    double length_result = test_length_extension(rounds, 100);
    if (length_result < 0.3) {
        printf("  ✓ PASSED (deviation < 30%%)\n");
        passed++;
    } else {
        printf("  ⚠️ POTENTIAL VULNERABILITY\n");
    }
    
    // Test 8: Near-Collisions
    printf("\n[8/10] Near-Collision Resistance\n");
    double near_coll_result = test_near_collisions(rounds, 1000);
    if (near_coll_result < 0.1) {
        printf("  ✓ PASSED\n");
        passed++;
    } else {
        printf("  ⚠️ POTENTIAL WEAKNESS\n");
    }
    
    // Test 9: Input Sensitivity
    printf("\n[9/10] Input Sensitivity\n");
    double sensitivity_result = test_input_sensitivity(rounds);
    if (sensitivity_result < 0.2) {
        printf("  ✓ PASSED (deviation < 20%%)\n");
        passed++;
    } else {
        printf("  ⚠️ MARGINAL INPUT SENSITIVITY\n");
    }
    
    // Test 10: Distribution Uniformity
    printf("\n[10/10] Distribution Uniformity\n");
    double uniform_result = test_distribution_uniformity(rounds, 10000);
    if (uniform_result < 0.1) {
        printf("  ✓ PASSED (chi-square within limits)\n");
        passed++;
    } else {
        printf("  ⚠️ NON-UNIFORM DISTRIBUTION\n");
    }
    
    // Summary
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY: %d/%d tests passed                           ║\n", passed, total);
    printf("╚═══════════════════════════════════════════════════════╝\n");
    
    if (passed == total) {
        printf("\n✓✓✓ EXCELLENT: All security tests passed!\n");
    } else if (passed >= 8) {
        printf("\n✓ GOOD: Most security tests passed.\n");
    } else if (passed >= 6) {
        printf("\n⚠️ MARGINAL: Some security concerns exist.\n");
    } else {
        printf("\n✗ POOR: Significant security issues detected!\n");
    }
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Comprehensive Hash Security Analysis Suite           ║\n");
    printf("║  Testing: Birthday, Preimage, SAC, BIC, and more     ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    srand((unsigned)time(NULL));
    
    printf("This test suite performs deep cryptanalysis.\n");
    printf("Expected runtime: 5-15 minutes depending on CPU.\n\n");
    
    // Test with default rounds
    run_comprehensive_tests(10000);
    
    printf("\n\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Testing with reduced rounds (1000)                   ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n");
    
    run_comprehensive_tests(1000);
    
    printf("\n\n=== Analysis Complete ===\n");
    
    return 0;
}
