/**
 * Deep Security Analysis: Advanced cryptanalysis tests
 * Tests for linear approximations, differential properties, and state complexity
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

unsigned long numberOfRounds = 100000;
int hashLengthInBits = 128;

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern Position_t pos;

// Helper: Convert hex string to bit array
void hex_to_bits(const char* hex, int* bits, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        int hex_idx = i / 4;
        int bit_in_nibble = 3 - (i % 4);
        int nibble = (hex[hex_idx] >= '0' && hex[hex_idx] <= '9') 
                     ? (hex[hex_idx] - '0') 
                     : (hex[hex_idx] - 'a' + 10);
        bits[i] = (nibble >> bit_in_nibble) & 1;
    }
}

// Test 1: Linear Approximation Test
// Check if there are linear relationships between input and output bits
double test_linear_approximation(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    
    printf("  Running %d samples for linear approximation...\n", num_samples);
    
    int num_bits = 128;
    double max_bias = 0.0;
    int worst_input_bit = -1;
    int worst_output_bit = -1;
    
    // Test each output bit against all input bits
    for (int out_bit = 0; out_bit < num_bits; out_bit += 8) { // Sample every 8th bit for speed
        int correlation[256] = {0}; // Count correlations for each input bit
        
        for (int sample = 0; sample < num_samples; sample++) {
            unsigned char input[32];
            for (int i = 0; i < 32; i++) {
                input[i] = rand() % 256;
            }
            
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(input, 32);
            char* hash = calculateHashValue();
            
            // Get output bit
            int out_nibble = out_bit / 4;
            int out_bit_in_nibble = 3 - (out_bit % 4);
            int nibble_val = (hash[out_nibble] >= '0' && hash[out_nibble] <= '9') 
                             ? (hash[out_nibble] - '0') 
                             : (hash[out_nibble] - 'a' + 10);
            int output_bit = (nibble_val >> out_bit_in_nibble) & 1;
            
            // Check correlation with input bits (sample first 32 input bits)
            for (int in_bit = 0; in_bit < 32; in_bit++) {
                int input_bit = (input[in_bit / 8] >> (in_bit % 8)) & 1;
                if (input_bit == output_bit) {
                    correlation[in_bit]++;
                }
            }
            
            free(hash);
        }
        
        // Check for bias (should be ~50% if no linear relationship)
        for (int in_bit = 0; in_bit < 32; in_bit++) {
            double prob = (double)correlation[in_bit] / num_samples;
            double bias = fabs(prob - 0.5);
            if (bias > max_bias) {
                max_bias = bias;
                worst_input_bit = in_bit;
                worst_output_bit = out_bit;
            }
        }
        
        if ((out_bit + 8) % 32 == 0) {
            printf("    Progress: %d/%d output bits tested\n", out_bit + 8, num_bits);
        }
    }
    
    printf("  Max bias: %.4f (input bit %d → output bit %d)\n", 
           max_bias, worst_input_bit, worst_output_bit);
    
    return max_bias;
}

// Test 2: Differential Cryptanalysis
// Check if input differences lead to predictable output differences
double test_differential_properties(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    
    printf("  Testing differential properties with %d sample pairs...\n", num_samples);
    
    typedef struct {
        int input_diff_hamming;
        int output_diff_hamming;
    } DiffPair;
    
    DiffPair* pairs = malloc(num_samples * sizeof(DiffPair));
    
    // Test with 1-bit differences
    for (int sample = 0; sample < num_samples; sample++) {
        unsigned char input1[32];
        unsigned char input2[32];
        
        // Generate random input
        for (int i = 0; i < 32; i++) {
            input1[i] = rand() % 256;
            input2[i] = input1[i];
        }
        
        // Flip random number of bits (1-8 bits)
        int bits_to_flip = 1 + (rand() % 8);
        for (int i = 0; i < bits_to_flip; i++) {
            int byte_pos = rand() % 32;
            int bit_pos = rand() % 8;
            input2[byte_pos] ^= (1 << bit_pos);
        }
        
        // Hash both
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input1, 32);
        char* hash1 = calculateHashValue();
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input2, 32);
        char* hash2 = calculateHashValue();
        
        // Count differences
        int output_diffs = 0;
        for (size_t i = 0; i < strlen(hash1); i++) {
            if (hash1[i] != hash2[i]) {
                int val1 = (hash1[i] >= '0' && hash1[i] <= '9') ? (hash1[i] - '0') : (hash1[i] - 'a' + 10);
                int val2 = (hash2[i] >= '0' && hash2[i] <= '9') ? (hash2[i] - '0') : (hash2[i] - 'a' + 10);
                int xor = val1 ^ val2;
                for (int bit = 0; bit < 4; bit++) {
                    if (xor & (1 << bit)) output_diffs++;
                }
            }
        }
        
        pairs[sample].input_diff_hamming = bits_to_flip;
        pairs[sample].output_diff_hamming = output_diffs;
        
        free(hash1);
        free(hash2);
        
        if ((sample + 1) % (num_samples / 10) == 0) {
            printf("    Progress: %d/%d pairs tested\n", sample + 1, num_samples);
        }
    }
    
    // Analyze: Check if input differences predict output differences
    // Group by input difference
    double correlations[9] = {0};
    int counts[9] = {0};
    
    for (int i = 0; i < num_samples; i++) {
        int in_diff = pairs[i].input_diff_hamming;
        if (in_diff <= 8) {
            correlations[in_diff] += pairs[i].output_diff_hamming;
            counts[in_diff]++;
        }
    }
    
    double max_deviation = 0.0;
    printf("\n  Input diff → Avg output diff:\n");
    for (int i = 1; i <= 8; i++) {
        if (counts[i] > 0) {
            double avg = correlations[i] / counts[i];
            double expected = 64.0; // 50% of 128 bits
            double deviation = fabs(avg - expected) / expected;
            printf("    %d bits → %.1f bits (deviation: %.2f%%)\n", 
                   i, avg, deviation * 100);
            if (deviation > max_deviation) {
                max_deviation = deviation;
            }
        }
    }
    
    free(pairs);
    return max_deviation;
}

// Test 3: State Complexity Analysis
// Check how many distinct internal states are reached
double test_state_complexity(unsigned long rounds, int num_samples) {
    numberOfRounds = rounds;
    
    printf("  Analyzing internal state complexity with %d samples...\n", num_samples);
    
    // Hash the field state to detect duplicates
    typedef struct {
        uint64_t state_hash;
    } StateInfo;
    
    StateInfo* states = malloc(num_samples * sizeof(StateInfo));
    
    for (int sample = 0; sample < num_samples; sample++) {
        unsigned char input[32];
        for (int i = 0; i < 32; i++) {
            input[i] = rand() % 256;
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 32);
        
        // After processing, examine field state
        // Create a simple hash of the field state
        uint64_t state_hash = 0;
        for (int x = 0; x < FIELD_SIZE; x++) {
            for (int y = 0; y < FIELD_SIZE; y++) {
                state_hash ^= (uint64_t)field[x][y].value * (x * 11 + y * 13 + 17);
                state_hash = (state_hash << 5) | (state_hash >> 59);
            }
        }
        states[sample].state_hash = state_hash;
        
        if ((sample + 1) % (num_samples / 10) == 0) {
            printf("    Progress: %d/%d states collected\n", sample + 1, num_samples);
        }
    }
    
    // Count unique states
    int unique_states = 0;
    for (int i = 0; i < num_samples; i++) {
        int is_unique = 1;
        for (int j = 0; j < i; j++) {
            if (states[i].state_hash == states[j].state_hash) {
                is_unique = 0;
                break;
            }
        }
        if (is_unique) unique_states++;
    }
    
    double uniqueness = (double)unique_states / num_samples;
    printf("  Unique states: %d / %d (%.2f%%)\n", 
           unique_states, num_samples, uniqueness * 100);
    
    free(states);
    return uniqueness;
}

// Test 4: Weak Key Detection
// Check if certain inputs lead to weak internal states
int test_weak_keys(unsigned long rounds) {
    numberOfRounds = rounds;
    
    printf("  Testing for weak keys...\n");
    
    // Test patterns that might be weak
    unsigned char weak_patterns[][32] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // All zeros
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // All ones
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // Repeated 0x01
        {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55}, // Alternating 01010101
        {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}, // Alternating 10101010
    };
    int num_patterns = 5;
    
    char* hashes[5];
    for (int i = 0; i < num_patterns; i++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(weak_patterns[i], 8);
        hashes[i] = calculateHashValue();
    }
    
    // Check if any have suspiciously low entropy
    int weak_found = 0;
    for (int i = 0; i < num_patterns; i++) {
        // Count unique hex chars (should be well distributed)
        int char_counts[16] = {0};
        for (size_t j = 0; j < strlen(hashes[i]); j++) {
            int val = (hashes[i][j] >= '0' && hashes[i][j] <= '9') 
                      ? (hashes[i][j] - '0') 
                      : (hashes[i][j] - 'a' + 10);
            char_counts[val]++;
        }
        
        // Calculate entropy
        double entropy = 0.0;
        int total = strlen(hashes[i]);
        for (int c = 0; c < 16; c++) {
            if (char_counts[c] > 0) {
                double p = (double)char_counts[c] / total;
                entropy -= p * log2(p);
            }
        }
        
        printf("    Pattern %d: entropy=%.3f ", i, entropy);
        if (entropy < 3.5) { // Max entropy is 4.0 for 16 symbols
            printf("⚠️ LOW\n");
            weak_found++;
        } else {
            printf("✓\n");
        }
    }
    
    for (int i = 0; i < num_patterns; i++) {
        free(hashes[i]);
    }
    
    return weak_found;
}

void comprehensive_analysis(unsigned long rounds) {
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Deep Cryptanalysis: %lu rounds%*s║\n", 
           rounds, (int)(25 - snprintf(NULL, 0, "%lu", rounds)), "");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    // Test 1: Linear Approximation
    printf("\n[1/4] Linear Approximation Test\n");
    printf("Goal: Find linear relationships between input/output\n");
    double linear_bias = test_linear_approximation(rounds, 500);
    printf("\n  Result: ");
    if (linear_bias < 0.05) {
        printf("✓ SECURE (bias < 5%%)\n");
    } else if (linear_bias < 0.10) {
        printf("⚠️ MARGINAL (bias 5-10%%)\n");
    } else {
        printf("✗ VULNERABLE (bias > 10%%)\n");
    }
    
    // Test 2: Differential Analysis
    printf("\n[2/4] Differential Cryptanalysis\n");
    printf("Goal: Check if input differences predict output\n");
    double diff_deviation = test_differential_properties(rounds, 200);
    printf("\n  Result: ");
    if (diff_deviation < 0.10) {
        printf("✓ SECURE (deviation < 10%%)\n");
    } else if (diff_deviation < 0.20) {
        printf("⚠️ MARGINAL (deviation 10-20%%)\n");
    } else {
        printf("✗ VULNERABLE (deviation > 20%%)\n");
    }
    
    // Test 3: State Complexity
    printf("\n[3/4] Internal State Complexity\n");
    printf("Goal: Verify high state diversity\n");
    double state_uniqueness = test_state_complexity(rounds, 200);
    printf("\n  Result: ");
    if (state_uniqueness > 0.95) {
        printf("✓ SECURE (>95%% unique)\n");
    } else if (state_uniqueness > 0.90) {
        printf("⚠️ MARGINAL (90-95%% unique)\n");
    } else {
        printf("✗ VULNERABLE (<90%% unique)\n");
    }
    
    // Test 4: Weak Keys
    printf("\n[4/4] Weak Key Detection\n");
    printf("Goal: Find inputs that create weak states\n");
    int weak_keys = test_weak_keys(rounds);
    printf("\n  Result: ");
    if (weak_keys == 0) {
        printf("✓ SECURE (no weak keys found)\n");
    } else {
        printf("⚠️ Found %d weak patterns\n", weak_keys);
    }
    
    // Overall verdict
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Overall Security Assessment                  ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    int issues = 0;
    if (linear_bias >= 0.05) issues++;
    if (diff_deviation >= 0.10) issues++;
    if (state_uniqueness <= 0.95) issues++;
    if (weak_keys > 0) issues++;
    
    if (issues == 0) {
        printf("\n✓ SECURE - All tests passed\n");
        printf("  %lu rounds provide strong security properties.\n", rounds);
    } else if (issues <= 2) {
        printf("\n⚠️ MARGINAL - %d test(s) showed concerns\n", issues);
        printf("  %lu rounds may be insufficient for critical applications.\n", rounds);
    } else {
        printf("\n✗ INSECURE - %d test(s) failed\n", issues);
        printf("  %lu rounds are NOT sufficient!\n", rounds);
    }
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  Advanced Cryptanalysis Test Suite           ║\n");
    printf("║  Testing: Linear, Differential, State        ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    srand((unsigned)time(NULL));
    
    // Test different round counts
    unsigned long test_rounds[] = {100, 1000, 5000, 10000, 50000};
    int num_tests = 5;
    
    printf("\nNote: This will take several minutes...\n");
    
    for (int i = 0; i < num_tests; i++) {
        comprehensive_analysis(test_rounds[i]);
        printf("\n");
    }
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Final Recommendation                         ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    printf("\nBased on deep cryptanalysis:\n");
    printf("  - If 100-1000 rounds pass all tests: safe to reduce\n");
    printf("  - If only 10000+ pass: keep current default\n");
    printf("  - If even 50000 shows issues: algorithm needs revision\n");
    
    return 0;
}
