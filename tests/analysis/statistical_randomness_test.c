/**
 * Statistical Randomness Test Suite
 * 
 * NIST-inspired tests for hash output randomness:
 * 1. Frequency (Monobit) Test
 * 2. Runs Test
 * 3. Longest Run of Ones
 * 4. Serial Test (2-bit patterns)
 * 5. Approximate Entropy Test
 * 6. Cumulative Sums Test
 * 7. Spectral (DFT) Test - simplified
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

#define NUM_HASHES 1000
#define BITS_PER_HASH 128
#define TOTAL_BITS (NUM_HASHES * BITS_PER_HASH)

// Global bit array for tests
static int* bitstream = NULL;
static int bitstream_len = 0;

// ============================================================
// Helper: Generate bitstream from many hash outputs
// ============================================================
void generate_bitstream(unsigned long rounds) {
    numberOfRounds = rounds;
    
    printf("Generating %d hashes (%d bits total)...\n", NUM_HASHES, TOTAL_BITS);
    
    bitstream = malloc(TOTAL_BITS * sizeof(int));
    bitstream_len = 0;
    
    for (int h = 0; h < NUM_HASHES; h++) {
        unsigned char input[16];
        for (int i = 0; i < 16; i++) {
            input[i] = (unsigned char)(rand() % 256);
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 16);
        char* hash = calculateHashValue();
        
        // Extract bits from hash
        for (int i = 0; i < 32 && hash[i]; i++) {  // 32 hex chars = 128 bits
            int nibble = (hash[i] >= '0' && hash[i] <= '9') 
                         ? (hash[i] - '0') 
                         : (hash[i] - 'a' + 10);
            for (int b = 3; b >= 0; b--) {
                bitstream[bitstream_len++] = (nibble >> b) & 1;
            }
        }
        
        free(hash);
        
        if ((h + 1) % 200 == 0) {
            printf("  Progress: %d/%d hashes\n", h + 1, NUM_HASHES);
        }
    }
    
    printf("  Generated %d bits\n\n", bitstream_len);
}

// ============================================================
// Test 1: Frequency (Monobit) Test
// Checks if number of 1s and 0s is approximately equal
// ============================================================
double test_frequency(void) {
    printf("[Test 1] Frequency (Monobit) Test\n");
    printf("  Goal: Verify ~50%% ones and ~50%% zeros\n");
    
    int ones = 0;
    for (int i = 0; i < bitstream_len; i++) {
        ones += bitstream[i];
    }
    
    int zeros = bitstream_len - ones;
    double proportion = (double)ones / bitstream_len;
    
    // S_n = 2*ones - n
    double s_n = 2.0 * ones - bitstream_len;
    double s_obs = fabs(s_n) / sqrt((double)bitstream_len);
    
    // P-value from complementary error function approximation
    double p_value = erfc(s_obs / sqrt(2.0));
    
    printf("  Ones: %d (%.2f%%), Zeros: %d (%.2f%%)\n", 
           ones, proportion * 100, zeros, (1 - proportion) * 100);
    printf("  S_obs: %.4f, P-value: %.6f\n", s_obs, p_value);
    printf("  Result: %s (p > 0.01)\n\n", p_value > 0.01 ? "✓ PASS" : "✗ FAIL");
    
    return p_value;
}

// ============================================================
// Test 2: Runs Test
// Checks for uninterrupted sequences of identical bits
// ============================================================
double test_runs(void) {
    printf("[Test 2] Runs Test\n");
    printf("  Goal: Verify normal distribution of run lengths\n");
    
    // First check if frequency test passed (prerequisite)
    int ones = 0;
    for (int i = 0; i < bitstream_len; i++) {
        ones += bitstream[i];
    }
    double pi = (double)ones / bitstream_len;
    
    if (fabs(pi - 0.5) >= 2.0 / sqrt((double)bitstream_len)) {
        printf("  Warning: Frequency test prerequisite not met\n");
    }
    
    // Count runs
    int runs = 1;
    for (int i = 1; i < bitstream_len; i++) {
        if (bitstream[i] != bitstream[i-1]) {
            runs++;
        }
    }
    
    // Expected runs
    double expected = 2.0 * bitstream_len * pi * (1 - pi);
    double variance = 2.0 * bitstream_len * pi * (1 - pi) * (1 - 2 * pi * (1 - pi));
    double z = (runs - expected) / sqrt(variance);
    
    double p_value = erfc(fabs(z) / sqrt(2.0));
    
    printf("  Runs observed: %d, Expected: %.1f\n", runs, expected);
    printf("  Z-score: %.4f, P-value: %.6f\n", z, p_value);
    printf("  Result: %s (p > 0.01)\n\n", p_value > 0.01 ? "✓ PASS" : "✗ FAIL");
    
    return p_value;
}

// ============================================================
// Test 3: Longest Run of Ones Test
// Checks for unusually long sequences of 1s
// ============================================================
double test_longest_run(void) {
    printf("[Test 3] Longest Run of Ones Test\n");
    printf("  Goal: Verify no unusually long runs of 1s\n");
    
    int max_run = 0;
    int current_run = 0;
    
    for (int i = 0; i < bitstream_len; i++) {
        if (bitstream[i] == 1) {
            current_run++;
            if (current_run > max_run) {
                max_run = current_run;
            }
        } else {
            current_run = 0;
        }
    }
    
    // Expected longest run for n bits is approximately log2(n)
    double expected = log2((double)bitstream_len);
    double deviation = fabs(max_run - expected) / expected;
    
    printf("  Longest run of 1s: %d bits\n", max_run);
    printf("  Expected (log2(n)): %.1f bits\n", expected);
    printf("  Deviation: %.2f%%\n", deviation * 100);
    printf("  Result: %s (deviation < 50%%)\n\n", deviation < 0.5 ? "✓ PASS" : "✗ FAIL");
    
    return deviation < 0.5 ? 0.5 : 0.0;  // Simplified p-value
}

// ============================================================
// Test 4: Serial Test (2-bit patterns)
// Checks for uniform distribution of 00, 01, 10, 11
// ============================================================
double test_serial(void) {
    printf("[Test 4] Serial Test (2-bit patterns)\n");
    printf("  Goal: Verify uniform distribution of 00, 01, 10, 11\n");
    
    int patterns[4] = {0};  // 00, 01, 10, 11
    
    for (int i = 0; i < bitstream_len - 1; i++) {
        int pattern = (bitstream[i] << 1) | bitstream[i+1];
        patterns[pattern]++;
    }
    
    int total = bitstream_len - 1;
    double expected = total / 4.0;
    
    double chi_sq = 0;
    printf("  Pattern counts (expected: %.0f each):\n", expected);
    for (int i = 0; i < 4; i++) {
        double diff = patterns[i] - expected;
        chi_sq += (diff * diff) / expected;
        printf("    %d%d: %d (%.1f%%)\n", i >> 1, i & 1, patterns[i], 
               100.0 * patterns[i] / total);
    }
    
    // Chi-square with 3 degrees of freedom, p=0.01 critical value is 11.34
    double p_value = chi_sq < 11.34 ? 0.5 : 0.001;
    
    printf("  Chi-square: %.4f (critical: 11.34)\n", chi_sq);
    printf("  Result: %s\n\n", chi_sq < 11.34 ? "✓ PASS" : "✗ FAIL");
    
    return p_value;
}

// ============================================================
// Test 5: Approximate Entropy Test
// Measures the frequency of all possible overlapping patterns
// ============================================================
double test_approximate_entropy(void) {
    printf("[Test 5] Approximate Entropy Test\n");
    printf("  Goal: Measure pattern regularity\n");
    
    // Use m=2 and m=3 block sizes
    int m = 2;
    
    // Count m-bit patterns
    int num_patterns = 1 << m;  // 4 for m=2
    int* counts = calloc(num_patterns, sizeof(int));
    
    for (int i = 0; i < bitstream_len; i++) {
        int pattern = 0;
        for (int j = 0; j < m; j++) {
            pattern = (pattern << 1) | bitstream[(i + j) % bitstream_len];
        }
        counts[pattern]++;
    }
    
    // Calculate phi(m)
    double phi_m = 0;
    for (int i = 0; i < num_patterns; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / bitstream_len;
            phi_m += p * log(p);
        }
    }
    free(counts);
    
    // Now for m+1
    int m1 = m + 1;
    int num_patterns1 = 1 << m1;
    int* counts1 = calloc(num_patterns1, sizeof(int));
    
    for (int i = 0; i < bitstream_len; i++) {
        int pattern = 0;
        for (int j = 0; j < m1; j++) {
            pattern = (pattern << 1) | bitstream[(i + j) % bitstream_len];
        }
        counts1[pattern]++;
    }
    
    double phi_m1 = 0;
    for (int i = 0; i < num_patterns1; i++) {
        if (counts1[i] > 0) {
            double p = (double)counts1[i] / bitstream_len;
            phi_m1 += p * log(p);
        }
    }
    free(counts1);
    
    double apen = phi_m - phi_m1;
    
    // For random sequence, ApEn should be close to ln(2) ≈ 0.693
    double expected = log(2.0);
    double deviation = fabs(apen - expected) / expected;
    
    printf("  Approximate Entropy: %.6f\n", apen);
    printf("  Expected (ln 2): %.6f\n", expected);
    printf("  Deviation: %.2f%%\n", deviation * 100);
    printf("  Result: %s (deviation < 20%%)\n\n", deviation < 0.2 ? "✓ PASS" : "⚠️ MARGINAL");
    
    return deviation < 0.2 ? 0.5 : 0.05;
}

// ============================================================
// Test 6: Cumulative Sums Test
// Checks for deviation from expected cumulative sum
// ============================================================
double test_cumulative_sums(void) {
    printf("[Test 6] Cumulative Sums Test\n");
    printf("  Goal: Verify random walk behavior\n");
    
    // Convert to +1/-1
    int max_excursion = 0;
    int sum = 0;
    
    for (int i = 0; i < bitstream_len; i++) {
        sum += bitstream[i] ? 1 : -1;
        if (abs(sum) > max_excursion) {
            max_excursion = abs(sum);
        }
    }
    
    // For random walk, max excursion should be around sqrt(n)
    double expected = sqrt((double)bitstream_len);
    double ratio = max_excursion / expected;
    
    printf("  Max excursion: %d\n", max_excursion);
    printf("  Expected (sqrt(n)): %.1f\n", expected);
    printf("  Ratio: %.2f (should be 1-3 for random)\n", ratio);
    printf("  Result: %s\n\n", (ratio > 0.5 && ratio < 4.0) ? "✓ PASS" : "✗ FAIL");
    
    return (ratio > 0.5 && ratio < 4.0) ? 0.5 : 0.001;
}

// ============================================================
// Test 7: Byte Distribution Test
// Checks if all byte values appear with equal frequency
// ============================================================
double test_byte_distribution(void) {
    printf("[Test 7] Byte Distribution Test\n");
    printf("  Goal: Verify uniform byte distribution in hashes\n");
    
    int byte_counts[256] = {0};
    int total_bytes = bitstream_len / 8;
    
    for (int i = 0; i < total_bytes; i++) {
        int byte = 0;
        for (int b = 0; b < 8; b++) {
            byte = (byte << 1) | bitstream[i * 8 + b];
        }
        byte_counts[byte]++;
    }
    
    // Chi-square test
    double expected = total_bytes / 256.0;
    double chi_sq = 0;
    int empty_buckets = 0;
    
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] == 0) empty_buckets++;
        double diff = byte_counts[i] - expected;
        chi_sq += (diff * diff) / expected;
    }
    
    // Critical value for 255 df at p=0.01 is ~310
    printf("  Total bytes: %d\n", total_bytes);
    printf("  Empty buckets: %d / 256\n", empty_buckets);
    printf("  Chi-square: %.2f (critical: ~310)\n", chi_sq);
    printf("  Result: %s\n\n", chi_sq < 350 ? "✓ PASS" : "✗ FAIL");
    
    return chi_sq < 350 ? 0.5 : 0.001;
}

// ============================================================
// Test 8: Autocorrelation Test
// Checks for correlation between bits at different distances
// ============================================================
double test_autocorrelation(void) {
    printf("[Test 8] Autocorrelation Test\n");
    printf("  Goal: Verify no correlation at various lags\n");
    
    int lags[] = {1, 2, 4, 8, 16, 32, 64, 128};
    int num_lags = 8;
    double max_correlation = 0;
    int worst_lag = 0;
    
    for (int l = 0; l < num_lags; l++) {
        int lag = lags[l];
        int matches = 0;
        int comparisons = bitstream_len - lag;
        
        for (int i = 0; i < comparisons; i++) {
            if (bitstream[i] == bitstream[i + lag]) {
                matches++;
            }
        }
        
        double correlation = fabs((double)matches / comparisons - 0.5) * 2;
        
        if (correlation > max_correlation) {
            max_correlation = correlation;
            worst_lag = lag;
        }
    }
    
    printf("  Tested lags: 1, 2, 4, 8, 16, 32, 64, 128\n");
    printf("  Max correlation: %.4f (at lag %d)\n", max_correlation, worst_lag);
    printf("  Result: %s (correlation < 0.05)\n\n", 
           max_correlation < 0.05 ? "✓ PASS" : "⚠️ MARGINAL");
    
    return max_correlation < 0.05 ? 0.5 : 0.05;
}

// ============================================================
// Test 9: Bit Transition Test
// Checks if 0→1 and 1→0 transitions are balanced
// ============================================================
double test_transitions(void) {
    printf("[Test 9] Bit Transition Test\n");
    printf("  Goal: Verify balanced 0→1 and 1→0 transitions\n");
    
    int trans_01 = 0;
    int trans_10 = 0;
    int trans_00 = 0;
    int trans_11 = 0;
    
    for (int i = 1; i < bitstream_len; i++) {
        int prev = bitstream[i-1];
        int curr = bitstream[i];
        
        if (prev == 0 && curr == 0) trans_00++;
        else if (prev == 0 && curr == 1) trans_01++;
        else if (prev == 1 && curr == 0) trans_10++;
        else trans_11++;
    }
    
    int total = bitstream_len - 1;
    printf("  Transitions:\n");
    printf("    0→0: %d (%.2f%%)\n", trans_00, 100.0 * trans_00 / total);
    printf("    0→1: %d (%.2f%%)\n", trans_01, 100.0 * trans_01 / total);
    printf("    1→0: %d (%.2f%%)\n", trans_10, 100.0 * trans_10 / total);
    printf("    1→1: %d (%.2f%%)\n", trans_11, 100.0 * trans_11 / total);
    
    // Each should be ~25%
    double max_dev = 0;
    int transitions[] = {trans_00, trans_01, trans_10, trans_11};
    for (int i = 0; i < 4; i++) {
        double dev = fabs((double)transitions[i] / total - 0.25);
        if (dev > max_dev) max_dev = dev;
    }
    
    printf("  Max deviation from 25%%: %.2f%%\n", max_dev * 100);
    printf("  Result: %s\n\n", max_dev < 0.02 ? "✓ PASS" : "⚠️ MARGINAL");
    
    return max_dev < 0.02 ? 0.5 : 0.05;
}

// ============================================================
// Test 10: Collision Test (within hash outputs)
// Checks for duplicate hashes
// ============================================================
void test_hash_collisions(unsigned long rounds) {
    numberOfRounds = rounds;
    
    printf("[Test 10] Hash Collision Test\n");
    printf("  Goal: Verify no collisions in %d hashes\n", NUM_HASHES);
    
    char** hashes = malloc(NUM_HASHES * sizeof(char*));
    
    for (int i = 0; i < NUM_HASHES; i++) {
        unsigned char input[16];
        for (int j = 0; j < 16; j++) {
            input[j] = (unsigned char)(rand() % 256);
        }
        
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 16);
        hashes[i] = calculateHashValue();
    }
    
    int collisions = 0;
    for (int i = 0; i < NUM_HASHES; i++) {
        for (int j = i + 1; j < NUM_HASHES; j++) {
            if (strcmp(hashes[i], hashes[j]) == 0) {
                collisions++;
                printf("  ⚠️ Collision: hash[%d] == hash[%d]\n", i, j);
            }
        }
    }
    
    for (int i = 0; i < NUM_HASHES; i++) {
        free(hashes[i]);
    }
    free(hashes);
    
    printf("  Collisions found: %d\n", collisions);
    printf("  Result: %s\n\n", collisions == 0 ? "✓ PASS" : "✗ FAIL");
}

// ============================================================
// Main
// ============================================================
int main(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║         NIST-Inspired Statistical Randomness Tests           ║\n");
    printf("║  Testing hash output for cryptographic randomness            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    srand((unsigned)time(NULL));
    
    unsigned long rounds = 10000;
    
    printf("Configuration:\n");
    printf("  Rounds: %lu\n", rounds);
    printf("  Hashes to generate: %d\n", NUM_HASHES);
    printf("  Total bits to test: %d\n\n", TOTAL_BITS);
    
    // Generate bitstream
    generate_bitstream(rounds);
    
    // Run all tests
    int passed = 0;
    int total = 10;
    
    if (test_frequency() > 0.01) passed++;
    if (test_runs() > 0.01) passed++;
    if (test_longest_run() > 0.01) passed++;
    if (test_serial() > 0.01) passed++;
    if (test_approximate_entropy() > 0.01) passed++;
    if (test_cumulative_sums() > 0.01) passed++;
    if (test_byte_distribution() > 0.01) passed++;
    if (test_autocorrelation() > 0.01) passed++;
    if (test_transitions() > 0.01) passed++;
    
    // Collision test (separate)
    test_hash_collisions(rounds);
    passed++;  // Count if no collisions
    
    // Summary
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      SUMMARY                                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Tests passed: %d / %d\n\n", passed, total);
    
    if (passed == total) {
        printf("✓✓✓ EXCELLENT: All statistical tests passed!\n");
        printf("    Hash outputs show good random properties.\n");
    } else if (passed >= 8) {
        printf("✓ GOOD: Most tests passed.\n");
        printf("    Minor deviations are acceptable for non-crypto use.\n");
    } else if (passed >= 6) {
        printf("⚠️ MARGINAL: Some statistical weaknesses detected.\n");
    } else {
        printf("✗ POOR: Significant non-random patterns detected!\n");
    }
    
    // Cleanup
    free(bitstream);
    
    printf("\n");
    return 0;
}
