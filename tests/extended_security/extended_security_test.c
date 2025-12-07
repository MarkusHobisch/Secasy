/*
 * Extended Security Tests for Secasy Hash Function
 * 
 * Tests:
 * 1. Length Extension Attack Resistance
 * 2. Bit Independence (correlation between output bits)
 * 3. Near-Collision Detection
 * 4. Structured Input Patterns
 * 5. Zero-Sensitivity (all-zero, single-bit inputs)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "../../Defines.h"
#include "../../Calculations.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../util.h"
#include "../../Printing.h"

#define MAX_HASH_LEN 512
#define DEFAULT_ROUNDS 1000
#define DEFAULT_PRIME_INDEX 200

/* Global variables required by Secasy */
unsigned long numberOfRounds = DEFAULT_ROUNDS;
int numberOfBits = 128;

/* ========== Hash Wrapper ========== */

static int g_rounds = DEFAULT_ROUNDS;
static int g_primeIndex = DEFAULT_PRIME_INDEX;
static int g_hashBits = 128;

void compute_hash(const uint8_t *input, size_t len, char *hashOut) {
    /* Write to temp file */
    FILE *f = fopen("_ext_sec_temp.bin", "wb");
    if (!f) { hashOut[0] = '\0'; return; }
    fwrite(input, 1, len, f);
    fclose(f);
    
    /* Compute hash */
    numberOfRounds = g_rounds;
    numberOfBits = g_hashBits;
    initFieldWithDefaultNumbers(g_primeIndex);
    readAndProcessFile("_ext_sec_temp.bin");
    
    char *result = calculateHashValue();
    if (result) {
        strcpy(hashOut, result);
        free(result);
    } else {
        hashOut[0] = '\0';
    }
    
    remove("_ext_sec_temp.bin");
}

/* Convert hex string to binary for bit operations */
void hex_to_bits(const char *hex, uint8_t *bits, int *bitCount) {
    int len = strlen(hex);
    *bitCount = len * 4;
    
    for (int i = 0; i < len; i++) {
        int val;
        if (hex[i] >= '0' && hex[i] <= '9') val = hex[i] - '0';
        else if (hex[i] >= 'a' && hex[i] <= 'f') val = hex[i] - 'a' + 10;
        else if (hex[i] >= 'A' && hex[i] <= 'F') val = hex[i] - 'A' + 10;
        else val = 0;
        
        bits[i*4 + 0] = (val >> 3) & 1;
        bits[i*4 + 1] = (val >> 2) & 1;
        bits[i*4 + 2] = (val >> 1) & 1;
        bits[i*4 + 3] = (val >> 0) & 1;
    }
}

int hamming_distance(const char *h1, const char *h2) {
    uint8_t bits1[1024], bits2[1024];
    int bc1, bc2;
    hex_to_bits(h1, bits1, &bc1);
    hex_to_bits(h2, bits2, &bc2);
    
    int minBits = bc1 < bc2 ? bc1 : bc2;
    int dist = 0;
    for (int i = 0; i < minBits; i++) {
        if (bits1[i] != bits2[i]) dist++;
    }
    return dist;
}

/* ========== Test 1: Length Extension Attack ========== */

int test_length_extension(int trials) {
    printf("\n=== TEST 1: Length Extension Attack Resistance ===\n");
    
    int suspicious = 0;
    
    for (int t = 0; t < trials; t++) {
        /* Original message */
        uint8_t msg1[32];
        for (int i = 0; i < 32; i++) msg1[i] = rand() & 0xFF;
        
        char hash1[MAX_HASH_LEN];
        compute_hash(msg1, 32, hash1);
        
        /* Extended message: msg1 + padding + extension */
        uint8_t msg2[64];
        memcpy(msg2, msg1, 32);
        /* Add some padding */
        msg2[32] = 0x80;
        for (int i = 33; i < 48; i++) msg2[i] = 0;
        /* Add extension */
        for (int i = 48; i < 64; i++) msg2[i] = rand() & 0xFF;
        
        char hash2[MAX_HASH_LEN];
        compute_hash(msg2, 64, hash2);
        
        /* Check if hash2 can be predicted from hash1 */
        /* In vulnerable hashes, there's a relationship */
        /* For Secasy, we check Hamming distance - should be ~50% */
        
        int dist = hamming_distance(hash1, hash2);
        int hashBits = strlen(hash1) * 4;
        double ratio = (double)dist / hashBits;
        
        /* If ratio is far from 0.5, might indicate weakness */
        if (ratio < 0.3 || ratio > 0.7) {
            suspicious++;
        }
    }
    
    double suspiciousRate = (double)suspicious / trials;
    printf("Trials: %d\n", trials);
    printf("Suspicious patterns: %d (%.2f%%)\n", suspicious, suspiciousRate * 100);
    
    int passed = suspiciousRate < 0.05; /* Less than 5% suspicious */
    printf("Result: %s\n", passed ? "PASSED" : "FAILED");
    
    return passed;
}

/* ========== Test 2: Bit Independence ========== */

int test_bit_independence(int trials) {
    printf("\n=== TEST 2: Bit Independence (Correlation) ===\n");
    
    int hashBits = g_hashBits;
    int maxBits = hashBits < 64 ? hashBits : 64; /* Limit for performance */
    
    /* Correlation matrix between bit pairs */
    int *both_flip = calloc(maxBits * maxBits, sizeof(int));
    int *bit_flip = calloc(maxBits, sizeof(int));
    
    for (int t = 0; t < trials; t++) {
        /* Generate two random inputs differing by 1 bit */
        uint8_t msg1[16], msg2[16];
        for (int i = 0; i < 16; i++) msg1[i] = rand() & 0xFF;
        memcpy(msg2, msg1, 16);
        
        int flipByte = rand() % 16;
        int flipBit = rand() % 8;
        msg2[flipByte] ^= (1 << flipBit);
        
        char hash1[MAX_HASH_LEN], hash2[MAX_HASH_LEN];
        compute_hash(msg1, 16, hash1);
        compute_hash(msg2, 16, hash2);
        
        uint8_t bits1[1024], bits2[1024];
        int bc1, bc2;
        hex_to_bits(hash1, bits1, &bc1);
        hex_to_bits(hash2, bits2, &bc2);
        
        /* Track which bits flipped */
        uint8_t flipped[64] = {0};
        for (int i = 0; i < maxBits && i < bc1; i++) {
            if (bits1[i] != bits2[i]) {
                flipped[i] = 1;
                bit_flip[i]++;
            }
        }
        
        /* Track pair correlations */
        for (int i = 0; i < maxBits; i++) {
            for (int j = i+1; j < maxBits; j++) {
                if (flipped[i] && flipped[j]) {
                    both_flip[i * maxBits + j]++;
                }
            }
        }
    }
    
    /* Calculate correlation coefficients */
    double maxCorr = 0;
    int maxI = 0, maxJ = 0;
    int highCorrCount = 0;
    
    for (int i = 0; i < maxBits; i++) {
        double pi = (double)bit_flip[i] / trials;
        for (int j = i+1; j < maxBits; j++) {
            double pj = (double)bit_flip[j] / trials;
            double pij = (double)both_flip[i * maxBits + j] / trials;
            
            /* Expected under independence: pi * pj */
            double expected = pi * pj;
            double observed = pij;
            
            /* Correlation measure */
            double denom = sqrt(pi * (1-pi) * pj * (1-pj));
            double corr = 0;
            if (denom > 0.001) {
                corr = (observed - expected) / denom;
            }
            
            if (fabs(corr) > fabs(maxCorr)) {
                maxCorr = corr;
                maxI = i;
                maxJ = j;
            }
            
            if (fabs(corr) > 0.15) {
                highCorrCount++;
            }
        }
    }
    
    printf("Trials: %d, Bits analyzed: %d\n", trials, maxBits);
    printf("Max correlation: %.4f (bits %d, %d)\n", maxCorr, maxI, maxJ);
    printf("High correlation pairs (|r| > 0.15): %d\n", highCorrCount);
    
    int totalPairs = maxBits * (maxBits - 1) / 2;
    double highCorrRate = (double)highCorrCount / totalPairs;
    printf("High correlation rate: %.2f%%\n", highCorrRate * 100);
    
    free(both_flip);
    free(bit_flip);
    
    int passed = highCorrRate < 0.05 && fabs(maxCorr) < 0.3;
    printf("Result: %s\n", passed ? "PASSED" : "FAILED");
    
    return passed;
}

/* ========== Test 3: Near-Collision Detection ========== */

int test_near_collisions(int trials) {
    printf("\n=== TEST 3: Near-Collision Detection ===\n");
    
    /* Store hashes and check for unusually close pairs */
    char **hashes = malloc(trials * sizeof(char*));
    for (int i = 0; i < trials; i++) {
        hashes[i] = malloc(MAX_HASH_LEN);
    }
    
    /* Generate random hashes */
    for (int t = 0; t < trials; t++) {
        uint8_t msg[16];
        for (int i = 0; i < 16; i++) msg[i] = rand() & 0xFF;
        compute_hash(msg, 16, hashes[t]);
    }
    
    /* Find minimum Hamming distance */
    int minDist = 9999;
    int minI = 0, minJ = 0;
    int nearCollisions = 0; /* Pairs with < 20% bit difference */
    
    int hashBits = strlen(hashes[0]) * 4;
    int threshold = hashBits * 0.2; /* 20% of bits */
    
    /* Sample pairs for large trials */
    int pairsToCheck = trials < 1000 ? trials * (trials-1) / 2 : 50000;
    
    for (int p = 0; p < pairsToCheck; p++) {
        int i, j;
        if (trials < 1000) {
            /* Check all pairs */
            i = p / trials;
            j = p % trials;
            if (j <= i) continue;
        } else {
            /* Random sampling */
            i = rand() % trials;
            j = rand() % trials;
            if (i == j) continue;
        }
        
        int dist = hamming_distance(hashes[i], hashes[j]);
        
        if (dist < minDist) {
            minDist = dist;
            minI = i;
            minJ = j;
        }
        
        if (dist < threshold) {
            nearCollisions++;
        }
    }
    
    printf("Hashes generated: %d\n", trials);
    printf("Hash bits: %d\n", hashBits);
    printf("Minimum Hamming distance: %d (%.1f%%)\n", minDist, 100.0 * minDist / hashBits);
    printf("Near-collisions (< 20%% diff): %d\n", nearCollisions);
    
    /* Expected minimum distance for random: ~n/2 - O(sqrt(n*log(m))) */
    /* For good hash, minimum should be > 30% of bits */
    double minRatio = (double)minDist / hashBits;
    
    int passed = minRatio > 0.25 && nearCollisions == 0;
    printf("Result: %s\n", passed ? "PASSED" : "FAILED");
    
    for (int i = 0; i < trials; i++) free(hashes[i]);
    free(hashes);
    
    return passed;
}

/* ========== Test 4: Structured Input Patterns ========== */

int test_structured_inputs(void) {
    printf("\n=== TEST 4: Structured Input Patterns ===\n");
    
    char hash_prev[MAX_HASH_LEN] = "";
    char hash_curr[MAX_HASH_LEN];
    int totalDist = 0;
    int count = 0;
    int minDist = 9999, maxDist = 0;
    
    printf("\n4a. Sequential counters (0, 1, 2, ...):\n");
    for (uint32_t i = 0; i < 1000; i++) {
        uint8_t msg[4] = {
            (i >> 24) & 0xFF,
            (i >> 16) & 0xFF,
            (i >> 8) & 0xFF,
            i & 0xFF
        };
        compute_hash(msg, 4, hash_curr);
        
        if (hash_prev[0]) {
            int dist = hamming_distance(hash_prev, hash_curr);
            totalDist += dist;
            count++;
            if (dist < minDist) minDist = dist;
            if (dist > maxDist) maxDist = dist;
        }
        strcpy(hash_prev, hash_curr);
    }
    
    int hashBits = strlen(hash_curr) * 4;
    double meanDist = (double)totalDist / count;
    printf("  Mean distance: %.1f / %d bits (%.1f%%)\n", meanDist, hashBits, 100*meanDist/hashBits);
    printf("  Min: %d, Max: %d\n", minDist, maxDist);
    
    int seq_passed = (meanDist / hashBits > 0.45) && (meanDist / hashBits < 0.55);
    printf("  Status: %s\n", seq_passed ? "PASSED" : "FAILED");
    
    printf("\n4b. All-zero with single bit set:\n");
    totalDist = 0; count = 0; minDist = 9999; maxDist = 0;
    hash_prev[0] = 0;
    
    for (int bit = 0; bit < 64; bit++) {
        uint8_t msg[8] = {0};
        msg[bit / 8] = 1 << (bit % 8);
        compute_hash(msg, 8, hash_curr);
        
        if (hash_prev[0]) {
            int dist = hamming_distance(hash_prev, hash_curr);
            totalDist += dist;
            count++;
            if (dist < minDist) minDist = dist;
            if (dist > maxDist) maxDist = dist;
        }
        strcpy(hash_prev, hash_curr);
    }
    
    meanDist = (double)totalDist / count;
    printf("  Mean distance: %.1f / %d bits (%.1f%%)\n", meanDist, hashBits, 100*meanDist/hashBits);
    printf("  Min: %d, Max: %d\n", minDist, maxDist);
    
    int single_passed = (meanDist / hashBits > 0.40) && (meanDist / hashBits < 0.60);
    printf("  Status: %s\n", single_passed ? "PASSED" : "FAILED");
    
    printf("\n4c. Repeating patterns (AAAA..., ABAB..., etc):\n");
    const char *patterns[] = {"AAAA", "ABAB", "ABCD", "0000", "FFFF", "0F0F"};
    int numPatterns = 6;
    
    char patternHashes[6][MAX_HASH_LEN];
    for (int p = 0; p < numPatterns; p++) {
        uint8_t msg[16];
        for (int i = 0; i < 16; i++) {
            msg[i] = patterns[p][i % strlen(patterns[p])];
        }
        compute_hash(msg, 16, patternHashes[p]);
    }
    
    /* Check distances between pattern hashes */
    int patternMinDist = 9999;
    for (int i = 0; i < numPatterns; i++) {
        for (int j = i+1; j < numPatterns; j++) {
            int dist = hamming_distance(patternHashes[i], patternHashes[j]);
            if (dist < patternMinDist) patternMinDist = dist;
        }
    }
    
    printf("  Min distance between patterns: %d / %d bits (%.1f%%)\n", 
           patternMinDist, hashBits, 100.0*patternMinDist/hashBits);
    
    int pattern_passed = (double)patternMinDist / hashBits > 0.35;
    printf("  Status: %s\n", pattern_passed ? "PASSED" : "FAILED");
    
    int passed = seq_passed && single_passed && pattern_passed;
    printf("\nOverall Structured Input Test: %s\n", passed ? "PASSED" : "FAILED");
    
    return passed;
}

/* ========== Test 5: Zero Sensitivity ========== */

int test_zero_sensitivity(void) {
    printf("\n=== TEST 5: Zero Sensitivity ===\n");
    
    /* Hash of all zeros */
    uint8_t zeros[32] = {0};
    char hash_zeros[MAX_HASH_LEN];
    compute_hash(zeros, 32, hash_zeros);
    
    printf("Hash of 32 zero bytes: %.32s...\n", hash_zeros);
    
    /* Check it's not trivial */
    int allSame = 1;
    for (int i = 1; hash_zeros[i]; i++) {
        if (hash_zeros[i] != hash_zeros[0]) {
            allSame = 0;
            break;
        }
    }
    
    if (allSame) {
        printf("WARNING: Hash of zeros is trivial!\n");
        printf("Result: FAILED\n");
        return 0;
    }
    
    /* Compare with single-byte change at various positions */
    int minDist = 9999, maxDist = 0, totalDist = 0;
    
    for (int pos = 0; pos < 32; pos++) {
        uint8_t msg[32] = {0};
        msg[pos] = 1;
        
        char hash[MAX_HASH_LEN];
        compute_hash(msg, 32, hash);
        
        int dist = hamming_distance(hash_zeros, hash);
        totalDist += dist;
        if (dist < minDist) minDist = dist;
        if (dist > maxDist) maxDist = dist;
    }
    
    int hashBits = strlen(hash_zeros) * 4;
    double meanDist = (double)totalDist / 32;
    
    printf("Single byte=1 at each position:\n");
    printf("  Mean distance from zeros: %.1f / %d bits (%.1f%%)\n", 
           meanDist, hashBits, 100*meanDist/hashBits);
    printf("  Min: %d (%.1f%%), Max: %d (%.1f%%)\n", 
           minDist, 100.0*minDist/hashBits, maxDist, 100.0*maxDist/hashBits);
    
    /* All positions should cause significant change */
    int passed = (minDist > hashBits * 0.3) && (meanDist > hashBits * 0.45);
    printf("Result: %s\n", passed ? "PASSED" : "FAILED");
    
    return passed;
}

/* ========== Main ========== */

int main(int argc, char *argv[]) {
    int trials = 1000;
    int seed = time(NULL);
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i+1 < argc) {
            trials = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            g_rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i+1 < argc) {
            g_primeIndex = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            g_hashBits = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Extended Security Tests for Secasy\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("  -t <trials>  Number of trials (default: 1000)\n");
            printf("  -r <rounds>  Hash rounds (default: 1000)\n");
            printf("  -i <index>   Max prime index (default: 200)\n");
            printf("  -n <bits>    Hash output bits (default: 128)\n");
            printf("  -s <seed>    Random seed\n");
            return 0;
        }
    }
    
    srand(seed);
    
    printf("==============================================\n");
    printf("   EXTENDED SECURITY TESTS FOR SECASY HASH\n");
    printf("==============================================\n");
    printf("Trials: %d, Rounds: %d, Seed: %d\n", trials, g_rounds, seed);
    printf("Hash bits: %d, Prime index: %d\n", g_hashBits, g_primeIndex);
    
    int passed = 0, total = 5;
    
    passed += test_length_extension(trials);
    passed += test_bit_independence(trials);
    passed += test_near_collisions(trials);
    passed += test_structured_inputs();
    passed += test_zero_sensitivity();
    
    printf("\n==============================================\n");
    printf("   SUMMARY: %d / %d TESTS PASSED\n", passed, total);
    printf("==============================================\n");
    
    if (passed == total) {
        printf("All extended security tests PASSED!\n");
    } else {
        printf("WARNING: %d test(s) FAILED - review results above.\n", total - passed);
    }
    
    return passed == total ? 0 : 1;
}
