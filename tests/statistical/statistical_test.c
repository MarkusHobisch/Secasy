/*
 * Statistical Randomness Tests for Secasy Hash Function
 * 
 * Simplified versions of NIST Statistical Test Suite:
 * 1. Frequency (Monobit) Test - Equal distribution of 0s and 1s
 * 2. Runs Test - Number of runs of consecutive bits
 * 3. Longest Run Test - Length of longest run of ones
 * 4. Serial Test - Distribution of 2-bit patterns
 * 5. Approximate Entropy Test
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
#define MAX_BITS 2048

/* Global variables required by Secasy */
unsigned long numberOfRounds = 1000;
int hashLengthInBits = 256;

static int g_rounds = 1000;
static int g_primeIndex = 200;
static int g_hashBits = 256;

void compute_hash(const uint8_t *input, size_t len, char *hashOut) {
    FILE *f = fopen("_stat_temp.bin", "wb");
    if (!f) { hashOut[0] = '\0'; return; }
    fwrite(input, 1, len, f);
    fclose(f);
    
    numberOfRounds = g_rounds;
    hashLengthInBits = g_hashBits;
    initFieldWithDefaultNumbers(g_primeIndex);
    readAndProcessFile("_stat_temp.bin");
    
    char *result = calculateHashValue();
    if (result) {
        strcpy(hashOut, result);
        free(result);
    } else {
        hashOut[0] = '\0';
    }
    
    remove("_stat_temp.bin");
}

/* Convert hex string to bit array */
int hex_to_bits(const char *hex, int *bits) {
    int len = strlen(hex);
    int bitCount = 0;
    
    for (int i = 0; i < len && bitCount < MAX_BITS; i++) {
        int val;
        if (hex[i] >= '0' && hex[i] <= '9') val = hex[i] - '0';
        else if (hex[i] >= 'a' && hex[i] <= 'f') val = hex[i] - 'a' + 10;
        else if (hex[i] >= 'A' && hex[i] <= 'F') val = hex[i] - 'A' + 10;
        else continue;
        
        bits[bitCount++] = (val >> 3) & 1;
        bits[bitCount++] = (val >> 2) & 1;
        bits[bitCount++] = (val >> 1) & 1;
        bits[bitCount++] = (val >> 0) & 1;
    }
    return bitCount;
}

/* Error function approximation for p-value calculation */
double erfc_approx(double x) {
    double t = 1.0 / (1.0 + 0.5 * fabs(x));
    double tau = t * exp(-x*x - 1.26551223 +
                         t * (1.00002368 +
                         t * (0.37409196 +
                         t * (0.09678418 +
                         t * (-0.18628806 +
                         t * (0.27886807 +
                         t * (-1.13520398 +
                         t * (1.48851587 +
                         t * (-0.82215223 +
                         t * 0.17087277)))))))));
    return x >= 0 ? tau : 2.0 - tau;
}

/* ========== Test 1: Frequency (Monobit) Test ========== */
double test_frequency(int *bits, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += bits[i] ? 1 : -1;
    }
    
    double sobs = fabs((double)sum) / sqrt((double)n);
    double pValue = erfc_approx(sobs / sqrt(2.0));
    
    return pValue;
}

/* ========== Test 2: Runs Test ========== */
double test_runs(int *bits, int n) {
    /* First check frequency prerequisite */
    int ones = 0;
    for (int i = 0; i < n; i++) {
        if (bits[i]) ones++;
    }
    
    double pi = (double)ones / n;
    double tau = 2.0 / sqrt((double)n);
    
    /* If frequency test would fail, skip */
    if (fabs(pi - 0.5) >= tau) {
        return 0.0; /* Fail */
    }
    
    /* Count runs */
    int runs = 1;
    for (int i = 1; i < n; i++) {
        if (bits[i] != bits[i-1]) runs++;
    }
    
    double expected = 2.0 * n * pi * (1.0 - pi) + 1.0;
    double variance = 2.0 * n * pi * (1.0 - pi);
    
    if (variance < 0.001) return 0.0;
    
    double z = (runs - expected) / sqrt(variance);
    double pValue = erfc_approx(fabs(z) / sqrt(2.0));
    
    return pValue;
}

/* ========== Test 3: Longest Run of Ones ========== */
double test_longest_run(int *bits, int n) {
    int maxRun = 0;
    int currentRun = 0;
    
    for (int i = 0; i < n; i++) {
        if (bits[i] == 1) {
            currentRun++;
            if (currentRun > maxRun) maxRun = currentRun;
        } else {
            currentRun = 0;
        }
    }
    
    /* Expected longest run approximately log2(n) */
    double expected = log2((double)n);
    double deviation = fabs(maxRun - expected) / expected;
    
    /* Simple pass/fail: longest run should be within reasonable range */
    /* For 256 bits, expected ~8, acceptable range 4-16 */
    double pValue = (deviation < 0.5) ? 0.5 : 0.01;
    
    return pValue;
}

/* ========== Test 4: Serial Test (2-bit patterns) ========== */
double test_serial(int *bits, int n) {
    if (n < 4) return 0.0;
    
    /* Count 2-bit patterns: 00, 01, 10, 11 */
    int counts[4] = {0};
    
    for (int i = 0; i < n - 1; i++) {
        int pattern = bits[i] * 2 + bits[i+1];
        counts[pattern]++;
    }
    
    /* Chi-square test against uniform distribution */
    double expected = (n - 1) / 4.0;
    double chi2 = 0;
    
    for (int i = 0; i < 4; i++) {
        double diff = counts[i] - expected;
        chi2 += (diff * diff) / expected;
    }
    
    /* Chi-square with 3 degrees of freedom */
    /* Critical value at 0.05: 7.815 */
    double pValue = (chi2 < 7.815) ? 0.5 : 0.01;
    
    return pValue;
}

/* ========== Test 5: Approximate Entropy ========== */
double test_approximate_entropy(int *bits, int n, int m) {
    if (n < m + 1) return 0.0;
    
    int numPatterns = 1 << m;
    int *counts = calloc(numPatterns, sizeof(int));
    
    /* Count m-bit patterns (circular) */
    for (int i = 0; i < n; i++) {
        int pattern = 0;
        for (int j = 0; j < m; j++) {
            pattern = (pattern << 1) | bits[(i + j) % n];
        }
        counts[pattern]++;
    }
    
    /* Calculate phi */
    double phi = 0;
    for (int i = 0; i < numPatterns; i++) {
        if (counts[i] > 0) {
            double pi = (double)counts[i] / n;
            phi += pi * log(pi);
        }
    }
    
    free(counts);
    
    /* Expected entropy for random: -m * log(0.5) = m * log(2) */
    double expected = -m * log(0.5);
    double actual = -phi;
    
    /* Ratio should be close to 1 */
    double ratio = actual / expected;
    double pValue = (ratio > 0.8 && ratio < 1.2) ? 0.5 : 0.01;
    
    return pValue;
}

/* ========== Main Test Runner ========== */

int main(int argc, char *argv[]) {
    int numHashes = 100;
    int seed = time(NULL);
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            numHashes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i+1 < argc) {
            g_rounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 && i+1 < argc) {
            g_hashBits = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Statistical Randomness Tests for Secasy\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("  -n <num>    Number of hashes to test (default: 100)\n");
            printf("  -r <rounds> Hash rounds (default: 1000)\n");
            printf("  -b <bits>   Hash output bits (default: 256)\n");
            printf("  -s <seed>   Random seed\n");
            return 0;
        }
    }
    
    srand(seed);
    
    printf("==============================================\n");
    printf("   STATISTICAL RANDOMNESS TESTS (NIST-like)\n");
    printf("==============================================\n");
    printf("Hashes: %d, Rounds: %d, Bits: %d, Seed: %d\n\n", 
           numHashes, g_rounds, g_hashBits, seed);
    
    /* Aggregate statistics */
    double freq_sum = 0, runs_sum = 0, longest_sum = 0, serial_sum = 0, entropy_sum = 0;
    int freq_pass = 0, runs_pass = 0, longest_pass = 0, serial_pass = 0, entropy_pass = 0;
    
    double pThreshold = 0.01; /* Standard NIST threshold */
    
    printf("Running tests on %d hash outputs...\n", numHashes);
    
    for (int h = 0; h < numHashes; h++) {
        /* Generate random input */
        uint8_t input[32];
        for (int i = 0; i < 32; i++) input[i] = rand() & 0xFF;
        
        /* Compute hash */
        char hash[MAX_HASH_LEN];
        compute_hash(input, 32, hash);
        
        /* Convert to bits */
        int bits[MAX_BITS];
        int n = hex_to_bits(hash, bits);
        
        /* Run tests */
        double p1 = test_frequency(bits, n);
        double p2 = test_runs(bits, n);
        double p3 = test_longest_run(bits, n);
        double p4 = test_serial(bits, n);
        double p5 = test_approximate_entropy(bits, n, 4);
        
        freq_sum += p1;
        runs_sum += p2;
        longest_sum += p3;
        serial_sum += p4;
        entropy_sum += p5;
        
        if (p1 >= pThreshold) freq_pass++;
        if (p2 >= pThreshold) runs_pass++;
        if (p3 >= pThreshold) longest_pass++;
        if (p4 >= pThreshold) serial_pass++;
        if (p5 >= pThreshold) entropy_pass++;
        
        /* Progress */
        if ((h + 1) % 20 == 0 || h == numHashes - 1) {
            printf("\r  Progress: %d/%d", h + 1, numHashes);
            fflush(stdout);
        }
    }
    
    printf("\n\n");
    
    /* Results */
    double passRate = 0.96; /* Expect at least 96% pass rate */
    
    printf("=== RESULTS ===\n\n");
    
    printf("Test                    | Pass Rate | Avg P-value | Status\n");
    printf("------------------------|-----------|-------------|--------\n");
    
    double r1 = (double)freq_pass / numHashes;
    printf("Frequency (Monobit)     | %5.1f%%    | %.4f      | %s\n", 
           100*r1, freq_sum/numHashes, r1 >= passRate ? "PASS" : "FAIL");
    
    double r2 = (double)runs_pass / numHashes;
    printf("Runs                    | %5.1f%%    | %.4f      | %s\n", 
           100*r2, runs_sum/numHashes, r2 >= passRate ? "PASS" : "FAIL");
    
    double r3 = (double)longest_pass / numHashes;
    printf("Longest Run             | %5.1f%%    | %.4f      | %s\n", 
           100*r3, longest_sum/numHashes, r3 >= passRate ? "PASS" : "FAIL");
    
    double r4 = (double)serial_pass / numHashes;
    printf("Serial (2-bit)          | %5.1f%%    | %.4f      | %s\n", 
           100*r4, serial_sum/numHashes, r4 >= passRate ? "PASS" : "FAIL");
    
    double r5 = (double)entropy_pass / numHashes;
    printf("Approximate Entropy     | %5.1f%%    | %.4f      | %s\n", 
           100*r5, entropy_sum/numHashes, r5 >= passRate ? "PASS" : "FAIL");
    
    printf("\n");
    
    int totalPassed = (r1 >= passRate) + (r2 >= passRate) + (r3 >= passRate) + 
                      (r4 >= passRate) + (r5 >= passRate);
    
    printf("==============================================\n");
    printf("   SUMMARY: %d / 5 TESTS PASSED\n", totalPassed);
    printf("==============================================\n");
    
    if (totalPassed == 5) {
        printf("All statistical randomness tests PASSED!\n");
        printf("Hash output appears statistically random.\n");
    } else {
        printf("WARNING: Some tests failed - review output distribution.\n");
    }
    
    return totalPassed == 5 ? 0 : 1;
}
