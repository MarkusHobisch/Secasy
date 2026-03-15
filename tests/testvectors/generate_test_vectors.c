/*
 * Secasy Test Vector Generator
 * ════════════════════════════
 *
 * PURPOSE:
 *   Generates canonical test vectors for the Secasy hash function.
 *   Test vectors allow third parties to verify a correct implementation
 *   by comparing their output against these reference values.
 *
 * METHOD:
 *   Computes Secasy-512 hashes for a set of fixed, well-known inputs
 *   covering edge cases: empty input, single bytes, short strings,
 *   known phrases, binary patterns, and larger payloads.
 *   All vectors use production defaults: 10 rounds, 512-bit output.
 *
 * CONCLUSION:
 *   The output of this program defines the official Secasy-512 test vectors.
 *   Any correct implementation must produce identical hex strings for each input.
 *
 * BUILD TARGET: SecasyTestVectors
 * HASH SIZE:    DEFAULT_BIT_SIZE (512)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"

unsigned long numberOfRounds  = DEFAULT_NUMBER_OF_ROUNDS;
int           hashLengthInBits = DEFAULT_BIT_SIZE;

/* Compute hash of raw bytes and return hex string (caller must free). */
static char* hash_bytes(const unsigned char* data, size_t len)
{
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

/* Compute hash of a NUL-terminated string. */
static char* hash_str(const char* s)
{
    return hash_bytes((const unsigned char*)s, strlen(s));
}

/* Print one test vector line. */
static void print_vector(const char* label, const char* description,
                          const unsigned char* data, size_t len)
{
    char* h = hash_bytes(data, len);
    printf("%-40s  %s\n", label, h);
    printf("  # %s  (input_len=%zu)\n\n", description, len);
    free(h);
}

int main(void)
{
    printf("Secasy-512 Test Vectors\n");
    printf("===================================================="
           "============================\n");
    printf("Algorithm : Secasy\n");
    printf("Hash size : 512 bit\n");
    printf("Rounds    : %lu\n", numberOfRounds);
    printf("Encoding  : lowercase hexadecimal\n");
    printf("Date      : 2026-03-15\n");
    printf("===================================================="
           "============================\n\n");

    printf("%-40s  %s\n", "INPUT", "SECASY-512 HASH");
    printf("%-40s  %s\n",
           "----------------------------------------",
           "----------------------------------------------------------------"
           "----------------------------------------------------------------");
    printf("\n");

    /* ── 1. Empty input ─────────────────────────────────────────── */
    {
        const unsigned char empty[] = {0};
        print_vector("\"\"",
                     "Empty string (zero bytes)",
                     empty, 0);
    }

    /* ── 2. Single ASCII characters ─────────────────────────────── */
    {
        const unsigned char a[] = {'a'};
        print_vector("\"a\"",
                     "Single lowercase letter",
                     a, 1);
    }
    {
        const unsigned char z[] = {'z'};
        print_vector("\"z\"",
                     "Single lowercase letter",
                     z, 1);
    }
    {
        const unsigned char zero[] = {'0'};
        print_vector("\"0\"",
                     "Single digit character",
                     zero, 1);
    }

    /* ── 3. Short strings ────────────────────────────────────────── */
    {
        const char* s = "abc";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"abc\"", h);
        printf("  # Classic 3-char string  (input_len=3)\n\n");
        free(h);
    }
    {
        const char* s = "secasy";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"secasy\"", h);
        printf("  # Algorithm name  (input_len=6)\n\n");
        free(h);
    }

    /* ── 4. Classic test phrases ─────────────────────────────────── */
    {
        const char* s = "The quick brown fox jumps over the lazy dog";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"The quick brown fox...\"", h);
        printf("  # Classic pangram  (input_len=%zu)\n\n", strlen(s));
        free(h);
    }
    {
        const char* s = "The quick brown fox jumps over the lazy cog";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"The quick brown fox...cog\"", h);
        printf("  # Pangram with 1-char difference (dog->cog)  (input_len=%zu)\n\n", strlen(s));
        free(h);
    }

    /* ── 5. Single-bit difference sensitivity ────────────────────── */
    {
        const unsigned char byte_00[] = {0x00};
        print_vector("0x00",
                     "Single zero byte",
                     byte_00, 1);
    }
    {
        const unsigned char byte_01[] = {0x01};
        print_vector("0x01",
                     "Single byte — 1-bit difference from 0x00",
                     byte_01, 1);
    }
    {
        const unsigned char byte_ff[] = {0xFF};
        print_vector("0xFF",
                     "Single all-ones byte",
                     byte_ff, 1);
    }
    {
        const unsigned char byte_80[] = {0x80};
        print_vector("0x80",
                     "Single high-bit byte",
                     byte_80, 1);
    }

    /* ── 6. All-zero blocks of increasing size ───────────────────── */
    {
        unsigned char buf[64];
        memset(buf, 0x00, sizeof(buf));
        print_vector("0x00 * 8",  "8 zero bytes",  buf, 8);
        print_vector("0x00 * 16", "16 zero bytes", buf, 16);
        print_vector("0x00 * 32", "32 zero bytes", buf, 32);
        print_vector("0x00 * 64", "64 zero bytes", buf, 64);
    }

    /* ── 7. All-ones blocks ───────────────────────────────────────── */
    {
        unsigned char buf[64];
        memset(buf, 0xFF, sizeof(buf));
        print_vector("0xFF * 8",  "8 all-ones bytes",  buf, 8);
        print_vector("0xFF * 64", "64 all-ones bytes", buf, 64);
    }

    /* ── 8. Sequential bytes 0x00..0xFF ──────────────────────────── */
    {
        unsigned char buf[256];
        for (int i = 0; i < 256; i++) buf[i] = (unsigned char)i;
        print_vector("0x00..0xFF",
                     "256 sequential bytes (0 to 255)",
                     buf, 256);
    }

    /* ── 9. Personal names (custom vectors) ─────────────────────── */
    {
        const char* s = "Markus";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"Markus\"", h);
        printf("  # Personal name  (input_len=6)\n\n");
        free(h);
    }
    {
        const char* s = "Anna";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"Anna\"", h);
        printf("  # Personal name  (input_len=4)\n\n");
        free(h);
    }

    /* ── 10. Digit strings ───────────────────────────────────────────── */
    {
        const char* s = "1234567890";
        char* h = hash_str(s);
        printf("%-40s  %s\n", "\"1234567890\"", h);
        printf("  # Digit sequence  (input_len=10)\n\n");
        free(h);
    }

    /* ── 10. Repeated character blocks ───────────────────────────── */
    {
        unsigned char buf[64];
        memset(buf, 'a', 64);
        print_vector("'a' * 64",
                     "64 repeated lowercase 'a' bytes",
                     buf, 64);
    }
    {
        unsigned char buf[64];
        memset(buf, 'a', 64);
        print_vector("'a' * 55",
                     "55 repeated lowercase 'a' bytes (near block boundary)",
                     buf, 55);
    }
    {
        unsigned char buf[64];
        memset(buf, 'a', 64);
        print_vector("'a' * 56",
                     "56 repeated lowercase 'a' bytes (block boundary)",
                     buf, 56);
    }

    /* ── 11. 1 KB of zeros ───────────────────────────────────────── */
    {
        unsigned char buf[1024];
        memset(buf, 0x00, sizeof(buf));
        print_vector("0x00 * 1024",
                     "1 KiB of zero bytes",
                     buf, 1024);
    }

    /* ── 12. 1 KB of alternating 0xAA / 0x55 ────────────────────── */
    {
        unsigned char buf[1024];
        for (int i = 0; i < 1024; i++) buf[i] = (i % 2 == 0) ? 0xAA : 0x55;
        print_vector("0xAA55 * 512",
                     "1 KiB alternating 0xAA/0x55 pattern",
                     buf, 1024);
    }

    printf("====================================================\n");
    printf("Total vectors: 27\n");
    printf("Configuration: %d-bit output, %lu rounds\n",
           hashLengthInBits, numberOfRounds);
    printf("====================================================\n");

    return 0;
}
