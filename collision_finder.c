/**
 * Collision Finder for Secasy Hash Function
 *
 * Strategy:
 * 1. Birthday attack - generate many hashes and look for collisions
 * 2. Algebraic analysis - exploit the sum/product structure
 * 3. Near-collision hunting - find inputs with very similar hashes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Defines.h"

// Number of hashes to generate for birthday attack
#define NUM_SAMPLES 1000000
#define MAX_INPUT_LEN 64
#define HASH_TABLE_SIZE 2000003  // Prime number for hash table

typedef struct HashEntry {
    char* hash;
    unsigned char* input;
    size_t input_len;
    struct HashEntry* next;
} HashEntry;

static HashEntry* hash_table[HASH_TABLE_SIZE];

// Simple string hash for hash table lookup
static unsigned long djb2_hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_TABLE_SIZE;
}

// Generate random input of given length
static void random_input(unsigned char* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = (unsigned char)(rand() % 256);
    }
}

// Compute hash for a buffer
static char* compute_hash(const unsigned char* data, size_t len) {
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

// Check if collision exists and add to table
static int check_and_add(const char* hash, const unsigned char* input, size_t input_len) {
    unsigned long idx = djb2_hash(hash);
    HashEntry* entry = hash_table[idx];

    while (entry) {
        if (strcmp(entry->hash, hash) == 0) {
            // Check if inputs are actually different
            if (entry->input_len != input_len ||
                memcmp(entry->input, input, input_len) != 0) {
                // COLLISION FOUND!
                printf("\n*** COLLISION FOUND! ***\n");
                printf("Hash: %s\n", hash);
                printf("Input 1 (%zu bytes): ", entry->input_len);
                for (size_t i = 0; i < entry->input_len; i++) {
                    printf("%02x", entry->input[i]);
                }
                printf("\nInput 2 (%zu bytes): ", input_len);
                for (size_t i = 0; i < input_len; i++) {
                    printf("%02x", input[i]);
                }
                printf("\n");
                return 1; // Collision found
            }
        }
        entry = entry->next;
    }

    // Add new entry
    HashEntry* new_entry = malloc(sizeof(HashEntry));
    new_entry->hash = strdup(hash);
    new_entry->input = malloc(input_len);
    memcpy(new_entry->input, input, input_len);
    new_entry->input_len = input_len;
    new_entry->next = hash_table[idx];
    hash_table[idx] = new_entry;

    return 0;
}

// Birthday attack
static int birthday_attack(int num_samples) {
    printf("Starting birthday attack with %d samples...\n", num_samples);

    unsigned char input[MAX_INPUT_LEN];
    int collisions = 0;

    clock_t start = clock();

    for (int i = 0; i < num_samples; i++) {
        // Vary input length between 1 and MAX_INPUT_LEN
        size_t len = (rand() % MAX_INPUT_LEN) + 1;
        random_input(input, len);

        char* hash = compute_hash(input, len);

        if (check_and_add(hash, input, len)) {
            collisions++;
        }

        free(hash);

        if ((i + 1) % 10000 == 0) {
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            printf("\rProgress: %d/%d (%.1f/s), Collisions: %d",
                   i + 1, num_samples, (i + 1) / elapsed, collisions);
            fflush(stdout);
        }
    }

    printf("\nBirthday attack complete. Collisions found: %d\n", collisions);
    return collisions;
}

// Try to find algebraic collision by exploiting symmetry
static int algebraic_attack(void) {
    printf("\nStarting algebraic attack (exploiting structure)...\n");

    // Test 1: Check if swapping bytes creates same hash
    unsigned char input1[] = "ABCD";
    unsigned char input2[] = "DCBA";

    char* hash1 = compute_hash(input1, 4);
    char* hash2 = compute_hash(input2, 4);

    printf("Test 1 - Simple reversal:\n");
    printf("  'ABCD' -> %s\n", hash1);
    printf("  'DCBA' -> %s\n", hash2);
    if (strcmp(hash1, hash2) == 0) {
        printf("  COLLISION FOUND!\n");
        free(hash1); free(hash2);
        return 1;
    }
    free(hash1); free(hash2);

    // Test 2: Check if zero padding affects hash differently
    unsigned char input3[] = {0x41, 0x00, 0x42}; // A\0B
    unsigned char input4[] = {0x42, 0x00, 0x41}; // B\0A

    hash1 = compute_hash(input3, 3);
    hash2 = compute_hash(input4, 3);

    printf("Test 2 - With null bytes:\n");
    printf("  41 00 42 -> %s\n", hash1);
    printf("  42 00 41 -> %s\n", hash2);
    if (strcmp(hash1, hash2) == 0) {
        printf("  COLLISION FOUND!\n");
        free(hash1); free(hash2);
        return 1;
    }
    free(hash1); free(hash2);

    // Test 3: Multiple zeros (null bytes are handled specially)
    unsigned char input5[] = {0x00, 0x00, 0x00, 0x41};
    unsigned char input6[] = {0x00, 0x00, 0x00, 0x00, 0x41};

    hash1 = compute_hash(input5, 4);
    hash2 = compute_hash(input6, 5);

    printf("Test 3 - Variable null prefix:\n");
    printf("  00 00 00 41 -> %s\n", hash1);
    printf("  00 00 00 00 41 -> %s\n", hash2);
    if (strcmp(hash1, hash2) == 0) {
        printf("  COLLISION FOUND!\n");
        free(hash1); free(hash2);
        return 1;
    }
    free(hash1); free(hash2);

    // Test 4: All zeros of different lengths
    unsigned char zeros4[4] = {0};
    unsigned char zeros8[8] = {0};

    hash1 = compute_hash(zeros4, 4);
    hash2 = compute_hash(zeros8, 8);

    printf("Test 4 - All zeros different lengths:\n");
    printf("  4 zeros -> %s\n", hash1);
    printf("  8 zeros -> %s\n", hash2);
    if (strcmp(hash1, hash2) == 0) {
        printf("  COLLISION FOUND!\n");
        free(hash1); free(hash2);
        return 1;
    }
    free(hash1); free(hash2);

    // Test 5: Check for length extension issues
    unsigned char base[] = "test";
    unsigned char extended[] = "test\x00\x00\x00\x00";

    hash1 = compute_hash(base, 4);
    hash2 = compute_hash(extended, 8);

    printf("Test 5 - Null padding extension:\n");
    printf("  'test' (4 bytes) -> %s\n", hash1);
    printf("  'test' + 4 nulls (8 bytes) -> %s\n", hash2);
    if (strcmp(hash1, hash2) == 0) {
        printf("  COLLISION FOUND!\n");
        free(hash1); free(hash2);
        return 1;
    }
    free(hash1); free(hash2);

    printf("No algebraic collisions found in basic tests.\n");
    return 0;
}

// Near-collision analysis
static void near_collision_analysis(int num_samples) {
    printf("\nStarting near-collision analysis...\n");

    unsigned char input[8];
    char* hashes[1000];
    unsigned char inputs[1000][8];
    int count = 0;
    int max_samples = (num_samples < 1000) ? num_samples : 1000;

    // Generate samples
    for (int i = 0; i < max_samples; i++) {
        random_input(input, 8);
        memcpy(inputs[i], input, 8);
        hashes[i] = compute_hash(input, 8);
        count++;
    }

    // Find pairs with shortest edit distance in hash
    int min_diff = 99999;
    int best_i = -1, best_j = -1;

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strlen(hashes[i]) == strlen(hashes[j])) {
                int diff = 0;
                for (size_t k = 0; k < strlen(hashes[i]); k++) {
                    if (hashes[i][k] != hashes[j][k]) diff++;
                }
                if (diff < min_diff) {
                    min_diff = diff;
                    best_i = i;
                    best_j = j;
                }
            }
        }
    }

    if (best_i >= 0) {
        printf("Closest pair (differ in %d chars):\n", min_diff);
        printf("  Input 1: ");
        for (int i = 0; i < 8; i++) printf("%02x", inputs[best_i][i]);
        printf(" -> %s\n", hashes[best_i]);
        printf("  Input 2: ");
        for (int i = 0; i < 8; i++) printf("%02x", inputs[best_j][i]);
        printf(" -> %s\n", hashes[best_j]);

        if (min_diff == 0) {
            printf("  COLLISION FOUND!\n");
        }
    }

    // Cleanup
    for (int i = 0; i < count; i++) {
        free(hashes[i]);
    }
}

// Cleanup hash table
static void cleanup_hash_table(void) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = hash_table[i];
        while (entry) {
            HashEntry* next = entry->next;
            free(entry->hash);
            free(entry->input);
            free(entry);
            entry = next;
        }
        hash_table[i] = NULL;
    }
}

int main(int argc, char* argv[]) {
    int num_samples = NUM_SAMPLES;

    if (argc > 1) {
        num_samples = atoi(argv[1]);
        if (num_samples <= 0) num_samples = NUM_SAMPLES;
    }

    srand((unsigned int)time(NULL));

    printf("=== Secasy Collision Finder ===\n");
    printf("Samples: %d\n\n", num_samples);

    // Run algebraic attack first (fast)
    int found = algebraic_attack();

    if (!found) {
        // Run birthday attack
        found = birthday_attack(num_samples);
    }

    if (!found) {
        // Run near-collision analysis
        near_collision_analysis(1000);
    }

    cleanup_hash_table();

    if (found) {
        printf("\n=== COLLISION SUCCESSFULLY FOUND! ===\n");
        return 0;
    } else {
        printf("\n=== No collision found in %d samples ===\n", num_samples);
        printf("This doesn't mean the hash is secure - larger search space needed.\n");
        return 1;
    }
}

