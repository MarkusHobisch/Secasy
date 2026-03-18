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
    printf("Date   : 2026-03-16\n\n");

    /* ── 512-bit vectors ─────────────────────────────────────────── */
    printf("--- 512-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 512, "170cc4dcf0d6f18211e1b34dab26e019c2bd077cb542f53d717544b3b8151f622489f9c95b5ac055c7b13991597469ad62c84674564a659cf6e2298a794cb8a5");
    TEST_STR("\"a\"", "a", 512, "3a29643d127dc5db52e87165c6a6354f18e21f7af3ca01df6fa3e7a75aebae6d55b4836e98fdec67436705447af36e098df252cf471f4d21acfd939cd200a1fd");
    TEST_STR("\"z\"", "z", 512, "9be4960d677a2db545422ba467ba6368d96d1cd7707a2fa5c9298f28707b341bce96fe38559af0ac7e6eb37edca080667e2ad87aa5f44d76f6d76333889b5f6c");
    TEST_STR("\"0\"", "0", 512, "4bc2e5fd617e6e0ddd65c39f3a8175786d8b2734fbf841f50e04217e34b20fe5f8f3bae55f8236192a0e649f9cf75555f07f0b0b0429458e48ec33c5a1b8f1a5");
    TEST_STR("\"abc\"", "abc", 512, "0daae080dab87f0b766d974697bb5f1151c56afacb903c131e0445ecef9ee0b0bbf7987798aad36e7134185c37603600ee60f8451c28ba0789be55d09e40a317");
    TEST_STR("\"secasy\"", "secasy", 512, "ea791c8897d5abffaa2f570f12b34db890d4fb75559cc233079973c8274ce864450cc6116877186e7382824df50129f79dc1d379843e77995b3e23db67dc88ce");
    TEST_STR("\"1234567890\"", "1234567890", 512, "2d6ff2cfacadb4a42d0ce9dddcd8d714a05cefcb09d54225a9bc1c8931ee9512fb161dd6b314ab6af1672e4d3d9fc5c70bd869be03850682a0a3ec5e44afd5f6");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "29982929a9a81356bacd93d4469b4becba57254db71c55ad362a2f47a7454df6198bcc6b308d62eace54403ad9842a17d3cb4a18527e74de328bc763d4172105");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "5e7fac4bdfc5b792e33546dcd0b825cc657ac4a329a5529837790fe1728867142f3d8130c52c66f1c19307bd6a25a72451a19e341edb26bbe16ee3914e910239");
    TEST("0x00", BYTE00, 1, 512, "7934c6203539b4a56ed85db787b694d87daab5011975984b405ef3e21ab4f84bf922d9c8b80ea3a7ada32629b27f8485fa2fd504ebdf24741cba942b4f83a1b8");
    TEST("0x01", BYTE01, 1, 512, "88c0b9c2ca25b6529866ad1c7986278058d3291a6c16d94660bdc78ea74fa9fbdc9e2319e964e35713b3a3700cb2ae61b6a5d237a838729f02a89e97dfa967a2");
    TEST("0x80", BYTE80, 1, 512, "dc509a98affade882bb4eaa992b45cea638bde98e501a160bc2378a8875b4cfe24b43ddd94c0f98c9fafb44c79423477fc788cb9a4e4cf93290f0eccf2f6a666");
    TEST("0xFF", BYTEFF, 1, 512, "d64809ba27354d28625c661f266678c2dc41b3d6760b7bd24cf959de641827390a90276ecf16e93985240a5b4e301a05f6ec3c1ebddde24fff2855f1148b09ea");
    TEST("0x00 x8", ZEROS8, 8, 512, "8912d0a0bf063521b47492589b38846f88f75efe2eec62c4eb8f1bc1598e98ec1c597fb63a4b015b180bc93badbb1cba76081dcc893716fa99d7f60600ef3f87");
    TEST("0x00 x16", ZEROS16, 16, 512, "0e2bead304e4fc4e061ab0aedf7dcd73d07be927e64ccbf63d97a7f0b645b01c956adf4483c0e50eb9dbe7decc8517c57085b01150fa03c16a6f0362b19f70f5");
    TEST("0x00 x32", ZEROS32, 32, 512, "be12bc1f46521285ff22aa2a54ba8084875c0c2c9f2e6a5093e14bd9bdf3c2d21ea9661fea1c6a0f9ae270caca56de833483440e4588d8e27a83d581920b4199");
    TEST("0x00 x64", ZEROS64, 64, 512, "b2807173ae08fcbcf5ae5f601a9b8bda8bf21576bbfc692aa7961f9993ce60cfa98db2ed4a94a143df1eb3e7e4a91f4fd09da51b460099016035e7340b4fb224");
    TEST("0xFF x8", ONES8, 8, 512, "e54c726c1ac815eceef213d88b916229abb848962296e21676021ed47ab69de8eff7675e95ec2a1a32567cd1a4a4d4c448b61df289db6f05895b18e378268114");
    TEST_STR("\"Markus\"", "Markus", 512, "fe638bdac5e04f1bc59f1b92b2d166e7019c9e3602bb4a4874dd1d8d9e2ecf9519a7d4f172427b8d763193ebce54af48d2b6bb86a68fa45916b39a738d78c610");
    TEST_STR("\"Anna\"", "Anna", 512, "fa8c9eeda7c7dc8d8baf9685d7607f435d96434105e88b4030366cea3fbbacdd994fb75bf221a74687744ccfb13c4482bb8a75d1b53883e9a7e0bdf836381e9a");
    TEST_STR("\"markus\"", "markus", 512, "3bf4c43d76d8414df6bb8ed325583533874d588c40a589a8f0765abad8c04e3e2dc8fe3facb40980cd38639a0efaf67b43964675043eb7834c42337a272d60b6");
    TEST_STR("\"maRkus\"", "maRkus", 512, "2070f917fcc83908c4eaf6044d5f537f8fc6f2f10c149b3f97c2eff71297ce546fb3ffbfb55f4bb85efa0182252cbae7acfc9c786d694d7f9603a2ee0d6a92f3");
    TEST_STR("\"Hanna\"", "Hanna", 512, "9572554815e9adba188a0058b700e113b59dd8283b4f4c00c1054eb167affd9f1a0ffa298add933e905e85b72e16a875c7be228aa8fee38d9687962e8b1f6eb6");
    TEST_STR("\"Antonette\"", "Antonette", 512, "2e85188b06c593828b3bccd647f7bc8c598f98390987a01c69cbba3708a776bff7f456708ef584efce737695aef2e450ab13e1592dc235ca299495994a4efcbc");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 64, "170cc4dcf0d6f182");
    TEST_STR("\"a\"", "a", 64, "3a29643d127dc5db");
    TEST_STR("\"abc\"", "abc", 64, "0daae080dab87f0b");
    TEST_STR("\"secasy\"", "secasy", 64, "ea791c8897d5abff");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "29982929a9a81356");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "5e7fac4bdfc5b792");
    TEST_STR("\"Markus\"", "Markus", 64, "fe638bdac5e04f1b");
    TEST_STR("\"Anna\"", "Anna", 64, "fa8c9eeda7c7dc8d");
    TEST_STR("\"markus\"", "markus", 64, "3bf4c43d76d8414d");
    TEST_STR("\"maRkus\"", "maRkus", 64, "2070f917fcc83908");
    TEST_STR("\"Hanna\"", "Hanna", 64, "9572554815e9adba");
    TEST_STR("\"Antonette\"", "Antonette", 64, "2e85188b06c59382");

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
