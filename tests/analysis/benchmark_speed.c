/**
 * Performance benchmark to measure hash speed improvements
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"
#include "../../Calculations.h"
#include "../../util.h"

unsigned long numberOfRounds = 100000;  // Default production value
int hashLengthInBits = 128;

void benchmark_small_inputs(void) {
    printf("\n=== Benchmark: Small Inputs (1-100 bytes) ===\n");
    
    const int num_iterations = 100000;
    unsigned char data[100];
    
    // Fill with random data
    for (int i = 0; i < 100; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    clock_t start = clock();
    double wall_start = wall_time_seconds();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        for (int len = 1; len <= 100; len++) {
            initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
            processBuffer(data, len);
            // Don't calculate full hash, just measure input processing
        }
    }
    
    clock_t end = clock();
    double wall_end = wall_time_seconds();
    
    double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
    double wall_time = wall_end - wall_start;
    
    long long total_bytes = (long long)num_iterations * 5050; // Sum of 1..100
    double mb_processed = total_bytes / (1024.0 * 1024.0);
    
    printf("Iterations: %d\n", num_iterations);
    printf("Total bytes processed: %.2f MB\n", mb_processed);
    printf("CPU time: %.3f s\n", cpu_time);
    printf("Wall time: %.3f s\n", wall_time);
    printf("Throughput (CPU): %.2f MB/s\n", mb_processed / cpu_time);
    printf("Throughput (Wall): %.2f MB/s\n", mb_processed / wall_time);
}

void benchmark_medium_inputs(void) {
    printf("\n=== Benchmark: Medium Inputs (1 KB) ===\n");
    
    const int num_iterations = 10000;
    const int data_size = 1024;
    unsigned char* data = malloc(data_size);
    
    // Fill with random data
    for (int i = 0; i < data_size; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    clock_t start = clock();
    double wall_start = wall_time_seconds();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(data, data_size);
    }
    
    clock_t end = clock();
    double wall_end = wall_time_seconds();
    
    double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
    double wall_time = wall_end - wall_start;
    
    double mb_processed = (num_iterations * data_size) / (1024.0 * 1024.0);
    
    printf("Iterations: %d\n", num_iterations);
    printf("Total bytes processed: %.2f MB\n", mb_processed);
    printf("CPU time: %.3f s\n", cpu_time);
    printf("Wall time: %.3f s\n", wall_time);
    printf("Throughput (CPU): %.2f MB/s\n", mb_processed / cpu_time);
    printf("Throughput (Wall): %.2f MB/s\n", mb_processed / wall_time);
    
    free(data);
}

void benchmark_large_inputs(void) {
    printf("\n=== Benchmark: Large Inputs (1 MB) ===\n");
    
    const int num_iterations = 100;
    const int data_size = 1024 * 1024;
    unsigned char* data = malloc(data_size);
    
    if (!data) {
        printf("Failed to allocate memory\n");
        return;
    }
    
    // Fill with random data
    for (int i = 0; i < data_size; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    clock_t start = clock();
    double wall_start = wall_time_seconds();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(data, data_size);
    }
    
    clock_t end = clock();
    double wall_end = wall_time_seconds();
    
    double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
    double wall_time = wall_end - wall_start;
    
    double mb_processed = (num_iterations * data_size) / (1024.0 * 1024.0);
    
    printf("Iterations: %d\n", num_iterations);
    printf("Total bytes processed: %.2f MB\n", mb_processed);
    printf("CPU time: %.3f s\n", cpu_time);
    printf("Wall time: %.3f s\n", wall_time);
    printf("Throughput (CPU): %.2f MB/s\n", mb_processed / cpu_time);
    printf("Throughput (Wall): %.2f MB/s\n", mb_processed / wall_time);
    
    free(data);
}

void benchmark_hash_generation(void) {
    printf("\n=== Benchmark: Full Hash Generation ===\n");
    
    const int num_iterations = 1000;
    unsigned char data[64];
    
    for (int i = 0; i < 64; i++) {
        data[i] = (unsigned char)(rand() % 256);
    }
    
    clock_t start = clock();
    double wall_start = wall_time_seconds();
    
    for (int iter = 0; iter < num_iterations; iter++) {
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(data, 64);
        char* hash = calculateHashValue();
        free(hash);
    }
    
    clock_t end = clock();
    double wall_end = wall_time_seconds();
    
    double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
    double wall_time = wall_end - wall_start;
    
    printf("Iterations: %d\n", num_iterations);
    printf("CPU time: %.3f s\n", cpu_time);
    printf("Wall time: %.3f s\n", wall_time);
    printf("Hashes per second (CPU): %.2f\n", num_iterations / cpu_time);
    printf("Hashes per second (Wall): %.2f\n", num_iterations / wall_time);
    printf("Time per hash: %.3f ms\n", (wall_time * 1000.0) / num_iterations);
}

int main(void) {
    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║  Secasy Hash Performance Benchmark            ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    srand((unsigned)time(NULL));
    
    benchmark_small_inputs();
    benchmark_medium_inputs();
    benchmark_large_inputs();
    benchmark_hash_generation();
    
    printf("\n╔═══════════════════════════════════════════════╗\n");
    printf("║  Benchmark Complete                           ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    
    return 0;
}
