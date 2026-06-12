/*
 * Secasy Test Vector Verifier
 * ═══════════════════════════
 *
 * PURPOSE:
 *   Verifies that the Secasy hash function produces the exact expected output
 *   for all canonical test vectors defined in TEST_VECTORS.md.
 *   Exits with code 0 on full success, 1 on any mismatch.
 *
 * METHOD:
 *   For each test vector, computes the hash and compares it byte-for-byte
 *   against the hard-coded expected value. PASS/FAIL is printed per vector.
 *
 * CONCLUSION:
 *   Any deviation from the expected output indicates a regression, a platform
 *   incompatibility, or a configuration mismatch (wrong rounds / bit size).
 *
 * BUILD TARGET: SecasyVerifyVectors
 * HASH SIZE:    512-bit and 64-bit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

static char *hash_bytes(const unsigned char *data, size_t len, int bits)
{
    hashLengthInBits = bits;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

static char *hash_str(const char *s, int bits)
{
    return hash_bytes((const unsigned char *)s, strlen(s), bits);
}

/* ── Test vector table ─────────────────────────────────────────────── */
typedef struct
{
    const char *label;
    const unsigned char *data;
    size_t len;
    int bits;
    const char *expected;
} Vector;

/* Helper so we can use string literals as data */
#define STR(s) (const unsigned char *)(s), (sizeof(s) - 1)
#define BIN(arr) (arr), sizeof(arr)

static const unsigned char EMPTY[] = {0}; /* len will be 0 */
static const unsigned char ZEROS8[] = {0, 0, 0, 0, 0, 0, 0, 0};
static const unsigned char ZEROS16[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const unsigned char ZEROS32[32] = {0};
static const unsigned char ZEROS64[64] = {0};
static const unsigned char ONES8[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const unsigned char BYTE00[] = {0x00};
static const unsigned char BYTE01[] = {0x01};
static const unsigned char BYTE80[] = {0x80};
static const unsigned char BYTEFF[] = {0xFF};

static int run_tests(void)
{
    int pass = 0, fail = 0;

    /* Macro to define and run one test */
#define TEST(label_, data_, len_, bits_, expected_)                              \
    do                                                                           \
    {                                                                            \
        char *got = hash_bytes((data_), (len_), (bits_));                        \
        int ok = (strcmp(got, (expected_)) == 0);                                \
        printf("[%s] %d-bit  %-46s\n", ok ? "PASS" : "FAIL", (bits_), (label_)); \
        if (!ok)                                                                 \
        {                                                                        \
            printf("       expected: %s\n", (expected_));                        \
            printf("       got:      %s\n", got);                                \
            fail++;                                                              \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            pass++;                                                              \
        }                                                                        \
        free(got);                                                               \
    } while (0)

#define TEST_STR(label_, str_, bits_, expected_)                                 \
    do                                                                           \
    {                                                                            \
        char *got = hash_str((str_), (bits_));                                   \
        int ok = (strcmp(got, (expected_)) == 0);                                \
        printf("[%s] %d-bit  %-46s\n", ok ? "PASS" : "FAIL", (bits_), (label_)); \
        if (!ok)                                                                 \
        {                                                                        \
            printf("       expected: %s\n", (expected_));                        \
            printf("       got:      %s\n", got);                                \
            fail++;                                                              \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            pass++;                                                              \
        }                                                                        \
        free(got);                                                               \
    } while (0)

    printf("Secasy Test Vector Verification\n");
    printf("================================\n");
    printf("Rounds : %lu\n", numberOfRounds);
    printf("Date   : 2026-06-12\n\n");

    /* ── 512-bit vectors ─────────────────────────────────────────── */
    printf("--- 512-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 512, "d73b7839a2ecfe6c6df0b0069a815ef41bb905f88aed985a613358c5ac206a9158fa80be81c2ff3de3151bcd9ab342ef3e75cd19a9e4039c666767dcd6291e0c");
    TEST_STR("\"a\"", "a", 512, "4c77343de76566edb46f421121000ca26bc544c0747efe19be85e657751be0215aeb98ea1f5c8bda6a82c6afa1b100de90819969a2585d167f69cc6978a31014");
    TEST_STR("\"z\"", "z", 512, "e669f8288dea3dd257d8fc6de657ed3bd38739fe30d03d080beffc4477c1572964e06fbd0d949fd4c270bd028ea7ff1dc65f2b6740feef50345b119283b06214");
    TEST_STR("\"0\"", "0", 512, "be46e07163a6428db693a1e1ca1cfbe768d1f3b5781e29265bd1bed91ca30418129d0c881ef7903d68e025ddc2123f05ca7825f7e4f46b207200eaafc7388792");
    TEST_STR("\"abc\"", "abc", 512, "564c6b22c9e268b3b37569bb1098bf36bf3eba09f454f8f737dfd23e344b737424ca3fb4aa0c287cc969e4ce22481cbbff899de4f16093821985897401c23890");
    TEST_STR("\"secasy\"", "secasy", 512, "d7b8ed372c5e4fecc94a2b7e424eb028b3019141d5a138267cd4b5ab8595e81a94b6cd3c4858c5a73db8751442b57b6c15f8ac684bc31152dd20a329ad589eb5");
    TEST_STR("\"1234567890\"", "1234567890", 512, "6072c026c17c78aeead275ebb55687d9088a686dfaa842f579b6d4312e6b25bd9d48ab4b62dfc086285eea1409d660e8f226bdc8fffbe9b3aad2fb5369698d3b");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "cbeee8a69e91d7afef0c9655d087621e451eb91da2f85b18c3801d9f3c2ba478b029f49dfeb760b875629fc3bdbfbde78ed8254c6470fb0ae83aa6065c3c54d4");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "da6a50e70f103d1a2db6aec270bad2d3a2cd38b540514bf457220399427e6bfade37d73be9f2bd1978e2b68a5a6170ce84aa3b75d0d603c3051755ef41157a11");
    TEST("0x00", BYTE00, 1, 512, "2e09d8db37a724cd02c9f25b6c734f0460f7c8f855e0469218b88a9e12d214beb1885d2e5974e68a252a8f8fe0e1a9260866e8fa1d9e51992c09ceedb6543865");
    TEST("0x01", BYTE01, 1, 512, "ebbf338c4cdc519233d5c59ce2612a01f6e0397915e68c9ed9999736870e9389a2442f44104680a6e2421245ea181aa3c37ab0633def41acc2c2ed38b4f3b79c");
    TEST("0x80", BYTE80, 1, 512, "3d3b80fd91e621a21f12a7a960732a806a182180e8f002675a2c60b7f4ef9204d128fb253037f6cedd6989473e558d4896b33864c48a1ef434b056af5f522d8c");
    TEST("0xFF", BYTEFF, 1, 512, "355e6d2f997d3be9bd63d8fdb6c9b86c71498bdf5602f23dbd83301e6257357e3f1758f0d8eacdf7dbaf17a632a49c0390d6eeff27a0b65bcbdd3ed0d89e6201");
    TEST("0x00 x8", ZEROS8, 8, 512, "280f44fe34035d6198369dc8772f72627e956e111ded7e4be33eb55ad8a8cc2a8051aa64d10f4bf63a273c88ab55b150811998ddf7730c73d862dc337986aef8");
    TEST("0x00 x16", ZEROS16, 16, 512, "8ed2fd24de925794ee76ccf6974e090989a073427167d29dad159687cf412ae528c2c792f6c6e903088ff562b04bd5aea940c9f4b56608036ba37792aad48e68");
    TEST("0x00 x32", ZEROS32, 32, 512, "8c2e115a198b12a0b3169c57c5bf0032fa82a2c46a3808d2892f7b4771db7163ce5313d3aee5285c9ad0c1abcb3c3afeb9a328e7be193958a0e95a6a8812c265");
    TEST("0x00 x64", ZEROS64, 64, 512, "7377f0ce60fecd3e50bf23dcc454b288fd21a828bca35ecf94e00de6b5ec34f4ea4b14d04cb0a7fededbfaa37328de3cc6fb8af54bb966d227a148e88970c58e");
    TEST("0xFF x8", ONES8, 8, 512, "106ca454d8c2f2fac5e17d9742d03fc432381db51e042fecc281e72cc70f7b188254ea9cced3b75405d5611bf03e16980d0b786a8c3e8eac40de393e59c32976");
    TEST_STR("\"Markus\"", "Markus", 512, "1905540d363075eab5159a7a7c3ffb859054383be7f08d6f326c682d27071a03f2b483b47bf276cbf9b30956bb446322ba7362b2de7b80acc17196f19866ffd4");
    TEST_STR("\"Anna\"", "Anna", 512, "cb50e2f929b25e3bd313e65b34c6422126295f05a6b8d1523a95051c1661d35aaea58b110ffc723b102f51b7afd17c7b28952bb8582a1e5caeba138a63edc7e4");
    TEST_STR("\"markus\"", "markus", 512, "adebba0f8e63c97d8d0b65388544bdd6ae2214a1b7a1161c0ec4f19c2e94f903fafb360a8e7c3b19eae4a0e5e40fae1c773f5c96dddde7a65c585cee05cb21b8");
    TEST_STR("\"maRkus\"", "maRkus", 512, "287b67f1ca707c2c29010e65157198e1e50f6f8de3d8899b75286bda84c932d0d45da2c3d67e814c4134bc173358e83a02e829f40d1f387b105be96f646f575b");
    TEST_STR("\"Hanna\"", "Hanna", 512, "d20e34d81e1828acaf1f29dae8b1873c0ed1b436f1038b9885c38ac5d4aea50ca422eee36be12d1b90d895f71a4446b56a105b8d19f9cb1fe2862c967be4a32b");
    TEST_STR("\"Antonette\"", "Antonette", 512, "30a5502bdf6aacba4bf1eb3371a7db3d7c89e9d4c2b8b36858c919f36311d74d6196db4afd65cf3e93363e5ea2eb2900e4bdfaa10de7673a5caf5be2c7b5a441");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 64, "d73b7839a2ecfe6c");
    TEST_STR("\"a\"", "a", 64, "4c77343de76566ed");
    TEST_STR("\"abc\"", "abc", 64, "564c6b22c9e268b3");
    TEST_STR("\"secasy\"", "secasy", 64, "d7b8ed372c5e4fec");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "cbeee8a69e91d7af");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "da6a50e70f103d1a");
    TEST_STR("\"Markus\"", "Markus", 64, "1905540d363075ea");
    TEST_STR("\"Anna\"", "Anna", 64, "cb50e2f929b25e3b");
    TEST_STR("\"markus\"", "markus", 64, "adebba0f8e63c97d");
    TEST_STR("\"maRkus\"", "maRkus", 64, "287b67f1ca707c2c");
    TEST_STR("\"Hanna\"", "Hanna", 64, "d20e34d81e1828ac");
    TEST_STR("\"Antonette\"", "Antonette", 64, "30a5502bdf6aacba");

    /* ── Summary ─────────────────────────────────────────────────── */
    printf("\n================================\n");
    printf("Results: %d/%d PASSED", pass, pass + fail);
    if (fail == 0)
        printf("  — ALL VECTORS VERIFIED\n");
    else
        printf("  — %d FAILED\n", fail);
    printf("================================\n");

    return (fail == 0) ? 0 : 1;
}

int main(void)
{
    return run_tests();
}
