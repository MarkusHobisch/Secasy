/**
 * Collision Attack Attempt
 * 
 * Trying to find two different inputs that produce the same hash.
 * Strategy:
 * 1. Brute-force small inputs (1-3 bytes)
 * 2. Look for patterns that might collide
 * 3. Try algebraic tricks based on the algorithm structure
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"

extern Tile_t field[FIELD_SIZE][FIELD_SIZE];

// Define globals that main.c normally defines
unsigned long numberOfRounds = 100000;  // Standard setting
int hashLengthInBits = 128;

// Store hash results
typedef struct {
    unsigned char input[8];
    size_t input_len;
    char hash[256];
} HashEntry;

#define MAX_ENTRIES 1000000

static HashEntry* entries;
static size_t entry_count = 0;

// Simple hash for lookup (not cryptographic)
static unsigned int simple_hash(const char* str) {
    unsigned int h = 0;
    while (*str) {
        h = h * 31 + (unsigned char)*str++;
    }
    return h;
}

// Check if hash already exists
static int find_collision(const char* hash, unsigned char* input, size_t len) {
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].hash, hash) == 0) {
            // Found a match - check if input is different
            if (entries[i].input_len != len || 
                memcmp(entries[i].input, input, len) != 0) {
                printf("\n🎉 COLLISION FOUND! 🎉\n");
                printf("Input 1: ");
                for (size_t j = 0; j < entries[i].input_len; j++) {
                    printf("%02x ", entries[i].input[j]);
                }
                printf("(len=%zu)\n", entries[i].input_len);
                printf("Input 2: ");
                for (size_t j = 0; j < len; j++) {
                    printf("%02x ", input[j]);
                }
                printf("(len=%zu)\n", len);
                printf("Hash: %s\n", hash);
                return 1;
            }
        }
    }
    return 0;
}

// Add entry
static void add_entry(unsigned char* input, size_t len, const char* hash) {
    if (entry_count >= MAX_ENTRIES) return;
    memcpy(entries[entry_count].input, input, len);
    entries[entry_count].input_len = len;
    strncpy(entries[entry_count].hash, hash, 255);
    entry_count++;
}

// Compute hash for input
static void compute_hash(unsigned char* input, size_t len, char* out_hash) {
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(input, len);
    char* hash = calculateHashValue();
    strcpy(out_hash, hash);
    free(hash);
}

int main(int argc, char* argv[]) {
    printf("=== Secasy Collision Attack ===\n");
    printf("Rounds: %lu, Bits: %d\n", numberOfRounds, hashLengthInBits);
    printf("Attempting to find collisions...\n\n");
    
    entries = (HashEntry*)calloc(MAX_ENTRIES, sizeof(HashEntry));
    if (!entries) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    char hash[256];
    unsigned char input[8];
    clock_t start = clock();
    unsigned long attempts = 0;
    
    // Strategy 1: All 1-byte inputs
    printf("[1] Testing all 1-byte inputs (256 values)...\n");
    for (int a = 0; a < 256; a++) {
        input[0] = (unsigned char)a;
        compute_hash(input, 1, hash);
        if (find_collision(hash, input, 1)) goto found;
        add_entry(input, 1, hash);
        attempts++;
    }
    printf("    No collisions in 1-byte inputs\n");
    
    // Strategy 2: All 2-byte inputs
    printf("[2] Testing all 2-byte inputs (65536 values)...\n");
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            input[0] = (unsigned char)a;
            input[1] = (unsigned char)b;
            compute_hash(input, 2, hash);
            if (find_collision(hash, input, 2)) goto found;
            add_entry(input, 2, hash);
            attempts++;
        }
        if (a % 32 == 0) {
            printf("    Progress: %d/256 (%.1f%%)\n", a, a * 100.0 / 256);
        }
    }
    printf("    No collisions in 2-byte inputs\n");
    
    // Strategy 3: Sample 3-byte inputs
    printf("[3] Sampling 3-byte inputs (500000 random values)...\n");
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 500000; i++) {
        input[0] = rand() & 0xFF;
        input[1] = rand() & 0xFF;
        input[2] = rand() & 0xFF;
        compute_hash(input, 3, hash);
        if (find_collision(hash, input, 3)) goto found;
        add_entry(input, 3, hash);
        attempts++;
        
        if (i % 50000 == 0) {
            printf("    Progress: %d/500000 (%.1f%%)\n", i, i * 100.0 / 500000);
        }
    }
    printf("    No collisions found in 3-byte samples\n");
    
    // Strategy 4: Exploit algebraic structure
    printf("[4] Trying algebraic tricks...\n");
    
    // Try inputs that might produce same field state
    // E.g., directions that cancel out
    unsigned char trick1[] = {0x00};  // No movement
    unsigned char trick2[] = {0x00, 0x00};  // Double no movement
    compute_hash(trick1, 1, hash);
    char hash2[256];
    compute_hash(trick2, 2, hash2);
    printf("    0x00 hash: %s\n", hash);
    printf("    0x00,0x00 hash: %s\n", hash2);
    if (strcmp(hash, hash2) == 0) {
        printf("    🎉 COLLISION with zero bytes!\n");
        goto found;
    }
    
    // Try opposite directions
    unsigned char up_down[] = {0b00001100};  // UP then DOWN
    unsigned char down_up[] = {0b00000011};  // DOWN then UP  
    compute_hash(up_down, 1, hash);
    compute_hash(down_up, 1, hash2);
    printf("    UP-DOWN (0x0C): %s\n", hash);
    printf("    DOWN-UP (0x03): %s\n", hash2);
    if (strcmp(hash, hash2) == 0) {
        printf("    🎉 COLLISION with direction reversal!\n");
        goto found;
    }
    
    // Try left-right patterns
    unsigned char lr[] = {0b00000110};  // LEFT-RIGHT
    unsigned char rl[] = {0b00001001};  // RIGHT-LEFT
    compute_hash(lr, 1, hash);
    compute_hash(rl, 1, hash2);
    printf("    LEFT-RIGHT: %s\n", hash);
    printf("    RIGHT-LEFT: %s\n", hash2);
    if (strcmp(hash, hash2) == 0) {
        printf("    🎉 COLLISION with LR/RL reversal!\n");
        goto found;
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("\n=== Attack Summary ===\n");
    printf("Total attempts: %lu\n", attempts);
    printf("Unique hashes stored: %zu\n", entry_count);
    printf("Time elapsed: %.2f seconds\n", elapsed);
    printf("Rate: %.0f hashes/sec\n", attempts / elapsed);
    printf("\n❌ NO COLLISION FOUND\n");
    printf("The hash appears resistant to simple collision attacks.\n");
    
    free(entries);
    return 0;
    
found:
    free(entries);
    printf("\n⚠️  Collision attack SUCCEEDED!\n");
    return 1;
}
