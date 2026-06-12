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
    TEST("\"\" (empty)", EMPTY, 0, 512, "0223add84c1344216d1618cab705af13d80883bd21f81a0542faeeaf8cea84f8aded59a1f7dcefea18dfc49462cf5add83d22f86cdc1c5cfeec49a7938b430c1");
    TEST_STR("\"a\"", "a", 512, "55d3c40bef7b48854a3645bf74344fd23e98c772f8ed571f32fb49267da65e6c275dcada025f65b91bc04c8d87186d061022ce410bd1745304854ff4908a7ba0");
    TEST_STR("\"z\"", "z", 512, "9337bc9dff2a3b53d500650fa34d47fa16c90d81477054a25891b5f2eb9361499a5a5e648fb66df0dc2306d633d97a971debaf47d7fc873f5fb457b97c1f93e6");
    TEST_STR("\"0\"", "0", 512, "d02380dba32fff80466044eaede83133bc9d08fa38a062e532d9cd0983589498a9169118ce10c64a1f53552818c8f7fd95901937638129af0bccdd46ae395b62");
    TEST_STR("\"abc\"", "abc", 512, "cbbd5e397871b0c4d686decf25866a6be1505f64d29b2412ec19dffa7fafddb9f6e360902cc4976001ace125d9d951080c7661bb86ee0aaf173fe2513402c456");
    TEST_STR("\"secasy\"", "secasy", 512, "2cd0dab047357f778d77c074c5018335ee1ea63942cd86f34ec58bfdc0998ab2af6c71c23e658e7010135786bc31922f70ba3d4b39fd95edd161230fb7c999ab");
    TEST_STR("\"1234567890\"", "1234567890", 512, "3d1f134ab491d3157603c86d3da652e3aee87d8fc6bad2b1e7cd32b24fcf527f20b1e7d4d8e3d24e59969cf761f8521c927b5219eb0cd1eacb60073c742151b8");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "504403abb1f3641b2f60a4846923b23690704070345e34ee6cedd844ed866b114b8d770736b49b5eca9a0feef7e6e8188959aac1b1110be6e9e8478c7a461e52");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "bdb65ed6fbffe32d00d92b1206de53e5a093f7d00be7028fc7dbac52d720535f0a4791cc0c92a38fdebf5e2e8392f42e6e542a6bc32321fb3940772f43f39289");
    TEST("0x00", BYTE00, 1, 512, "96ee9ae4b56de493aac84bb77b5b7303bea1fc8a41490173d27bad5d07368fe3e6555e2fcd241e53fa2f0f029311acc30e08bfd558ff3b3421e270a81eecc9a4");
    TEST("0x01", BYTE01, 1, 512, "a16c91469e8355001cc4bda936bfec59981cea0bcefc83b11375166e67391b0a8ecd42d0ff75b2620a256f3397b249bb857d9b962feee11300d5c7f8c82b786c");
    TEST("0x80", BYTE80, 1, 512, "0f7c54d8114fcbf3f758447ae97e62d4df34341dc1acf9b6c71023c099db9098aeec1363720a277a96c803064a38be5c7ea3f2a92267553e667fe24bfa95ec20");
    TEST("0xFF", BYTEFF, 1, 512, "6d320a4d99b92beb21fdac87b3dc9f2bd6c94ec1ce00126a8b94f0fbe82385aa406093360246f8eaf52c35701c6a6c29a9f7d7aa368ddf695ec379e450b152a9");
    TEST("0x00 x8", ZEROS8, 8, 512, "d1de76cc9419fe2bdef0bdec2ade68abec03050bc1a2d32bf9154c2b58673dab0627934aef2ba82c1339da6a85f012ac204c218a1cb47d2c2d5e68a9b378e7ac");
    TEST("0x00 x16", ZEROS16, 16, 512, "8ac44b90e0011334dcf74f98dfdfa0d99775547e4c663bbd9a4fb1f37b239770112b8ee051203b0534bd68012930d53ba6e6f7a2131218e75ba2b188f19ed3ca");
    TEST("0x00 x32", ZEROS32, 32, 512, "e8b2cc38a22642f5e390a431029140d0de6e7c2962fc3eabd94c5421c3673c86d42a2c1a23d23a61cf080412843d383cc9e5dc0ae4a83617c4c3b403451333f2");
    TEST("0x00 x64", ZEROS64, 64, 512, "cd3486996fec845b4aef64cb3395dc572af1e9e610371051829e9d979232a360c3cba4aa122e3330f46abc1fed04e20270cebabee6655b5522a37747f276001a");
    TEST("0xFF x8", ONES8, 8, 512, "146e1c6bbcd1b9c835eaa0c9d9d9415e810d58fb89d9a35b4a806e2cf6f44d97dfba3d31745cb68eeb26cb2eefd02c845cb2e63da4f6271214b6e27130432636");
    TEST_STR("\"Markus\"", "Markus", 512, "c38c743806b043e367b72411f32586ca0be1d3ebdf9ac9b1b00c83c5cc100c975437339fb8854f7ef861e379a4fa92649c8c9353916fd54b40b7432d7de51832");
    TEST_STR("\"Anna\"", "Anna", 512, "5fc38f965be81ec446d2af539f98253e2de1cf10e3482bb814f0eece26f83232fc000e8b6aa838abe30f2e48ae583f25ca1e4e05f208459fb12d6dc335b84c19");
    TEST_STR("\"markus\"", "markus", 512, "e1504ee4641aad3dfb8b1c2c3bf434ca15c5e97413cdbc583000b6bbeba743e54a3b8403c380cb726476514b9b5a52ff7eb11e937333da8c98ebebdb4b0d6219");
    TEST_STR("\"maRkus\"", "maRkus", 512, "000d4cc3865a9293ee3449e7794de8a6dc5b470b6c413ebaca82442f5f3494ceb8a941535227eae2a6d03e77451b40f694f73b9b380e970a831e38bf2b01ed1e");
    TEST_STR("\"Hanna\"", "Hanna", 512, "594336c1627d29035b53fe20bdaa30e65d64c58018d738c95f758cdf740440ac6186543ecf31488f63971b9e2a5e507265a7e2fd858b585567b8aa5ce0b86038");
    TEST_STR("\"Antonette\"", "Antonette", 512, "f872ed30e211589864ddf7e3a702f615d14902966bf493913db40d4930e6310eaa1f17fbf5d7ce8a168a22aebac96c0782f52d617fbb0983ef60381444aca6ff");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 64, "0223add84c134421");
    TEST_STR("\"a\"", "a", 64, "55d3c40bef7b4885");
    TEST_STR("\"abc\"", "abc", 64, "cbbd5e397871b0c4");
    TEST_STR("\"secasy\"", "secasy", 64, "2cd0dab047357f77");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "504403abb1f3641b");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "bdb65ed6fbffe32d");
    TEST_STR("\"Markus\"", "Markus", 64, "c38c743806b043e3");
    TEST_STR("\"Anna\"", "Anna", 64, "5fc38f965be81ec4");
    TEST_STR("\"markus\"", "markus", 64, "e1504ee4641aad3d");
    TEST_STR("\"maRkus\"", "maRkus", 64, "000d4cc3865a9293");
    TEST_STR("\"Hanna\"", "Hanna", 64, "594336c1627d2903");
    TEST_STR("\"Antonette\"", "Antonette", 64, "f872ed30e2115898");

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
