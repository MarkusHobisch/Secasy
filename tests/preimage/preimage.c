/*
 * ============================================================================
 * Preimage & Second-Preimage Resistance Test Harness for Secasy Hash Function
 * ============================================================================
 * 
 * PURPOSE:
 * This test harness evaluates the cryptographic strength of the Secasy hash 
 * function against preimage attacks (finding input for given hash) and 
 * second-preimage attacks (finding different input with same hash as given input).
 * 
 * SECURITY GOALS:
 * - Preimage Resistance: Given hash H, computationally infeasible to find any input M such that hash(M) = H
 * - Second-Preimage Resistance: Given input M1, computationally infeasible to find different M2 such that hash(M1) = hash(M2)
 * 
 * METHODOLOGY:
 * Uses brute-force search with randomized inputs to measure resistance strength.
 * For n-bit hash output, ideal resistance requires ~2^n attempts for preimage,
 * ~2^n attempts for second-preimage.
 * 
 * LIMITATIONS:
 * - Brute-force approach: computationally expensive, limited search space
 * - Statistical sampling: does not guarantee cryptographic security proof
 * - Time constraints: practical tests much smaller than theoretical attack space
 * - This tool measures resistance against naive attacks, not sophisticated cryptanalysis
 * 
 * WARNING:
 * This test provides empirical resistance measurement but does NOT constitute
 * formal cryptographic security proof. Results should be interpreted as indicators
 * of hash function behavior under limited testing conditions.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
#else
    #include <unistd.h>
#endif

// Include Secasy hash function components  
#include "../../Calculations.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../SieveOfEratosthenes.h"
#include "../../util.h"
#include "../../Printing.h"
#include "../../Defines.h"

// Test configuration
#define MAX_INPUT_LENGTH 64
#define MAX_HASH_LENGTH 512
#define PROGRESS_INTERVAL 10000
#define TIME_ESTIMATE_SAMPLES 1000

// Global variables required by Secasy components (from main.c)
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

// Global test parameters
static int g_maxAttempts = 1000000;
static int g_inputLength = 16;
static int g_hashRounds = 10;
static int g_bufferSize = 256;
static int g_maxPrimeIndex = 200;
static unsigned int g_seed = 0;
static bool g_verbose = false;
static char* g_exportFile = NULL;

// Test mode flags
static bool g_testPreimage = true;
static bool g_testSecondPreimage = true;

// Statistics tracking
typedef struct {
    uint64_t attempts;
    uint64_t successes;
    double elapsedTime;
    bool found;
    char foundInput[MAX_INPUT_LENGTH * 2 + 1]; // hex representation
    char targetHash[MAX_HASH_LENGTH + 1];
} TestResults;

/**
 * Generate random input of specified length
 */
void generateRandomInput(char* buffer, int length, unsigned int* seed) {
    (void)seed; // Mark unused to avoid warning
    for (int i = 0; i < length; i++) {
        buffer[i] = (char)(rand() % 256);
    }
}

/**
 * Convert binary data to hex string
 */
void binaryToHex(const char* binary, int binaryLen, char* hex) {
    for (int i = 0; i < binaryLen; i++) {
        sprintf(hex + i * 2, "%02x", (unsigned char)binary[i]);
    }
    hex[binaryLen * 2] = '\0';
}

/**
 * Compare two hash values (string comparison for hex hashes)
 */
bool hashesEqual(const char* hash1, const char* hash2, int length) {
    (void)length; // Unused since we're comparing strings
    return strcmp(hash1, hash2) == 0;
}

/**
 * Write input data to temporary file and compute Secasy hash
 */
void computeSecasyHash(const char* input, int inputLen, char* output, int outputLen) {
    // Create temporary file with random input data
    char tempFilename[] = "temp_preimage_XXXXXX";
    
    #ifdef _WIN32
        // On Windows, use mktemp alternative
        if (_mktemp_s(tempFilename, strlen(tempFilename) + 1) != 0) {
            strncpy(tempFilename, "temp_preimage_test.tmp", sizeof(tempFilename));
        }
    #else
        int fd = mkstemp(tempFilename);
        if (fd == -1) {
            strncpy(tempFilename, "temp_preimage_test.tmp", sizeof(tempFilename));
        } else {
            close(fd);
        }
    #endif
    
    // Write input data to temporary file
    FILE* tempFile = fopen(tempFilename, "wb");
    if (!tempFile) {
        memset(output, 0, outputLen);
        return;
    }
    
    fwrite(input, 1, inputLen, tempFile);
    fclose(tempFile);
    
    // Initialize Secasy and compute hash
    initFieldWithDefaultNumbers(g_maxPrimeIndex);
    readAndProcessFile(tempFilename);
    
    char* hashResult = calculateHashValue();
    
    // Copy result to output buffer
    if (hashResult) {
        int hashLen = strlen(hashResult);
        int copyLen = (outputLen - 1 < hashLen) ? outputLen - 1 : hashLen;
        memcpy(output, hashResult, copyLen);
        output[copyLen] = '\0';
        free(hashResult);
    } else {
        memset(output, 0, outputLen);
    }
    
    // Clean up temporary file
    remove(tempFilename);
}

/**
 * Test preimage resistance: given hash H, try to find input M such that hash(M) = H
 */
TestResults testPreimageResistance(const char* targetHash, int hashLen) {
    TestResults results = {0};
    strncpy(results.targetHash, targetHash, sizeof(results.targetHash) - 1);
    
    printf("\n=== PREIMAGE RESISTANCE TEST ===\n");
    printf("Target hash: %s\n", targetHash);
    printf("Search space: random %d-byte inputs\n", g_inputLength);
    printf("Max attempts: %d\n\n", g_maxAttempts);
    (void)hashLen; // Mark as used to avoid warning
    
    clock_t startTime = clock();
    unsigned int localSeed = g_seed;
    
    for (uint64_t attempt = 1; attempt <= (uint64_t)g_maxAttempts; attempt++) {
        results.attempts = attempt;
        
        // Generate random input
        char input[MAX_INPUT_LENGTH];
        generateRandomInput(input, g_inputLength, &localSeed);
        
        // Compute hash
        char computedHash[MAX_HASH_LENGTH];
        computeSecasyHash(input, g_inputLength, computedHash, sizeof(computedHash));
        
        // Check for match
        if (hashesEqual(computedHash, targetHash, 0)) {
            results.found = true;
            results.successes = 1;
            binaryToHex(input, g_inputLength, results.foundInput);
            
            printf("SUCCESS! Preimage found after %llu attempts\n", (unsigned long long)attempt);
            printf("Input (hex): %s\n", results.foundInput);
            break;
        }
        
        // Progress reporting
        if (attempt % PROGRESS_INTERVAL == 0) {
            double elapsed = (double)(clock() - startTime) / CLOCKS_PER_SEC;
            double rate = attempt / elapsed;
            double eta = (g_maxAttempts - attempt) / rate;
            
            printf("Progress: %llu/%d (%.2f%%) - Rate: %.0f/sec - ETA: %.0fs\n", 
                   (unsigned long long)attempt, g_maxAttempts, 
                   100.0 * attempt / g_maxAttempts, rate, eta);
        }
    }
    
    results.elapsedTime = (double)(clock() - startTime) / CLOCKS_PER_SEC;
    
    if (!results.found) {
        printf("No preimage found after %d attempts (%.2f seconds)\n", 
               g_maxAttempts, results.elapsedTime);
    }
    
    return results;
}

/**
 * Test second-preimage resistance: given input M1, try to find M2 ≠ M1 such that hash(M1) = hash(M2)
 */
TestResults testSecondPreimageResistance(const char* originalInput, int inputLen) {
    TestResults results = {0};
    
    // Compute hash of original input
    char originalHash[MAX_HASH_LENGTH];
    computeSecasyHash(originalInput, inputLen, originalHash, sizeof(originalHash));
    
    printf("\n=== SECOND-PREIMAGE RESISTANCE TEST ===\n");
    printf("Original input (hex): ");
    for (int i = 0; i < inputLen; i++) {
        printf("%02x", (unsigned char)originalInput[i]);
    }
    printf("\nOriginal hash: %s\n", originalHash);
    printf("Searching for different input with same hash...\n");
    printf("Max attempts: %d\n\n", g_maxAttempts);
    
    strncpy(results.targetHash, "second-preimage", sizeof(results.targetHash) - 1);
    
    clock_t startTime = clock();
    unsigned int localSeed = g_seed + 1; // Different seed than original
    
    for (uint64_t attempt = 1; attempt <= (uint64_t)g_maxAttempts; attempt++) {
        results.attempts = attempt;
        
        // Generate random input
        char input[MAX_INPUT_LENGTH];
        generateRandomInput(input, inputLen, &localSeed);
        
        // Skip if identical to original input
        if (memcmp(input, originalInput, inputLen) == 0) {
            continue;
        }
        
        // Compute hash
        char computedHash[MAX_HASH_LENGTH];
        computeSecasyHash(input, inputLen, computedHash, sizeof(computedHash));
        
        // Check for collision
        if (hashesEqual(computedHash, originalHash, 0)) {
            results.found = true;
            results.successes = 1;
            binaryToHex(input, inputLen, results.foundInput);
            
            printf("SUCCESS! Second-preimage found after %llu attempts\n", (unsigned long long)attempt);
            printf("Colliding input (hex): %s\n", results.foundInput);
            break;
        }
        
        // Progress reporting
        if (attempt % PROGRESS_INTERVAL == 0) {
            double elapsed = (double)(clock() - startTime) / CLOCKS_PER_SEC;
            double rate = attempt / elapsed;
            double eta = (g_maxAttempts - attempt) / rate;
            
            printf("Progress: %llu/%d (%.2f%%) - Rate: %.0f/sec - ETA: %.0fs\n", 
                   (unsigned long long)attempt, g_maxAttempts, 
                   100.0 * attempt / g_maxAttempts, rate, eta);
        }
    }
    
    results.elapsedTime = (double)(clock() - startTime) / CLOCKS_PER_SEC;
    
    if (!results.found) {
        printf("No second-preimage found after %d attempts (%.2f seconds)\n", 
               g_maxAttempts, results.elapsedTime);
    }
    
    return results;
}

/**
 * Export test results to CSV file
 */
void exportResults(TestResults preimageResults, TestResults secondPreimageResults) {
    if (!g_exportFile) return;
    
    FILE* file = fopen(g_exportFile, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not create export file %s\n", g_exportFile);
        return;
    }
    
    fprintf(file, "Test Type,Attempts,Success,Time (seconds),Success Rate,Theoretical Security Bits\n");
    
    // Preimage results
    double preimageSuccessRate = preimageResults.found ? 1.0 / preimageResults.attempts : 0.0;
    double preimageSecurityBits = preimageResults.found ? 
        -log2(preimageSuccessRate) : log2(preimageResults.attempts);
    
    fprintf(file, "Preimage,%llu,%s,%.3f,%.2e,%.1f\n",
            (unsigned long long)preimageResults.attempts,
            preimageResults.found ? "Yes" : "No",
            preimageResults.elapsedTime,
            preimageSuccessRate,
            preimageSecurityBits);
    
    // Second-preimage results
    double secondPreimageSuccessRate = secondPreimageResults.found ? 1.0 / secondPreimageResults.attempts : 0.0;
    double secondPreimageSecurityBits = secondPreimageResults.found ? 
        -log2(secondPreimageSuccessRate) : log2(secondPreimageResults.attempts);
    
    fprintf(file, "Second-Preimage,%llu,%s,%.3f,%.2e,%.1f\n",
            (unsigned long long)secondPreimageResults.attempts,
            secondPreimageResults.found ? "Yes" : "No",
            secondPreimageResults.elapsedTime,
            secondPreimageSuccessRate,
            secondPreimageSecurityBits);
    
    fclose(file);
    printf("\nResults exported to: %s\n", g_exportFile);
}

/**
 * Print usage information
 */
void printUsage(const char* progName) {
    printf("Secasy Preimage Resistance Test Tool\n");
    printf("Usage: %s [options]\n\n", progName);
    printf("Options:\n");
    printf("  -a <num>     Maximum attempts per test (default: %d)\n", g_maxAttempts);
    printf("  -l <bytes>   Input length in bytes (default: %d)\n", g_inputLength);
    printf("  -r <rounds>  Hash computation rounds (default: %d)\n", g_hashRounds);
    printf("  -n <size>    Internal buffer size (default: %d)\n", g_bufferSize);
    printf("  -i <index>   Maximum prime index (default: %d)\n", g_maxPrimeIndex);
    printf("  -s <seed>    Random seed (default: current time)\n");
    printf("  -P           Test only preimage resistance\n");
    printf("  -S           Test only second-preimage resistance\n");
    printf("  -o <file>    Export results to CSV file\n");
    printf("  -v           Verbose output\n");
    printf("  -h           Show this help\n\n");
    printf("Examples:\n");
    printf("  %s -a 100000 -l 8          # Quick test with 8-byte inputs\n", progName);
    printf("  %s -a 1000000 -o results.csv # Full test with CSV export\n", progName);
    printf("  %s -P -a 500000            # Preimage test only\n", progName);
}

/**
 * Parse command line arguments
 */
bool parseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            g_maxAttempts = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            g_inputLength = atoi(argv[++i]);
            if (g_inputLength > MAX_INPUT_LENGTH) {
                fprintf(stderr, "Error: Input length exceeds maximum (%d)\n", MAX_INPUT_LENGTH);
                return false;
            }
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            g_hashRounds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            g_bufferSize = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            g_maxPrimeIndex = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            g_seed = (unsigned int)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-P") == 0) {
            g_testPreimage = true;
            g_testSecondPreimage = false;
        } else if (strcmp(argv[i], "-S") == 0) {
            g_testPreimage = false;
            g_testSecondPreimage = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            g_exportFile = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            g_verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            return false;
        } else {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    printf("Secasy Preimage Resistance Test Tool\n");
    printf("====================================\n\n");
    
    // Parse command line arguments
    if (!parseArguments(argc, argv)) {
        printUsage(argv[0]);
        return argc > 1 ? 1 : 0;
    }
    
    // Initialize random seed
    if (g_seed == 0) {
        g_seed = (unsigned int)time(NULL);
    }
    printf("Configuration:\n");
    printf("- Max attempts: %d\n", g_maxAttempts);
    printf("- Input length: %d bytes\n", g_inputLength);
    printf("- Hash rounds: %d\n", g_hashRounds);
    printf("- Buffer size: %d\n", g_bufferSize);
    printf("- Seed: %u\n", g_seed);
    printf("- Tests: %s%s%s\n", 
           g_testPreimage ? "Preimage" : "",
           (g_testPreimage && g_testSecondPreimage) ? " + " : "",
           g_testSecondPreimage ? "Second-Preimage" : "");
    
    srand(g_seed);
    
    TestResults preimageResults = {0};
    TestResults secondPreimageResults = {0};
    
    // Test preimage resistance
    if (g_testPreimage) {
        // Generate a target hash by hashing a known input
        char knownInput[MAX_INPUT_LENGTH];
        unsigned int tempSeed = g_seed;
        generateRandomInput(knownInput, g_inputLength, &tempSeed);
        char targetHash[MAX_HASH_LENGTH];
        computeSecasyHash(knownInput, g_inputLength, targetHash, sizeof(targetHash));
        
        preimageResults = testPreimageResistance(targetHash, 0);
    }
    
    // Test second-preimage resistance
    if (g_testSecondPreimage) {
        char originalInput[MAX_INPUT_LENGTH];
        unsigned int tempSeed = g_seed;
        generateRandomInput(originalInput, g_inputLength, &tempSeed);
        
        secondPreimageResults = testSecondPreimageResistance(originalInput, g_inputLength);
    }
    
    // Summary
    printf("\n=== TEST SUMMARY ===\n");
    if (g_testPreimage) {
        printf("Preimage Test: %s after %llu attempts (%.2f sec)\n",
               preimageResults.found ? "FOUND" : "NOT FOUND",
               (unsigned long long)preimageResults.attempts, preimageResults.elapsedTime);
        if (preimageResults.found) {
            printf("  Security estimate: %.1f bits\n", -log2(1.0 / preimageResults.attempts));
        } else {
            printf("  Lower bound: %.1f bits\n", log2(preimageResults.attempts));
        }
    }
    
    if (g_testSecondPreimage) {
        printf("Second-Preimage Test: %s after %llu attempts (%.2f sec)\n",
               secondPreimageResults.found ? "FOUND" : "NOT FOUND",
               (unsigned long long)secondPreimageResults.attempts, secondPreimageResults.elapsedTime);
        if (secondPreimageResults.found) {
            printf("  Security estimate: %.1f bits\n", -log2(1.0 / secondPreimageResults.attempts));
        } else {
            printf("  Lower bound: %.1f bits\n", log2(secondPreimageResults.attempts));
        }
    }
    
    // Export results if requested
    if (g_exportFile) {
        exportResults(preimageResults, secondPreimageResults);
    }
    
    printf("\nWARNING: These results are based on limited brute-force testing.\n");
    printf("They do NOT constitute formal cryptographic security proofs.\n");
    
    return 0;
}