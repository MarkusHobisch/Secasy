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
#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"

unsigned long numberOfRounds   = DEFAULT_NUMBER_OF_ROUNDS;
int           hashLengthInBits = DEFAULT_BIT_SIZE;

static char* hash_bytes(const unsigned char* data, size_t len, int bits)
{
    hashLengthInBits = bits;
    initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
    processBuffer(data, len);
    return calculateHashValue();
}

static char* hash_str(const char* s, int bits)
{
    return hash_bytes((const unsigned char*)s, strlen(s), bits);
}

/* ── Test vector table ─────────────────────────────────────────────── */
typedef struct {
    const char* label;
    const unsigned char* data;
    size_t len;
    int bits;
    const char* expected;
} Vector;

/* Helper so we can use string literals as data */
#define STR(s)  (const unsigned char*)(s), (sizeof(s)-1)
#define BIN(arr) (arr), sizeof(arr)

static const unsigned char EMPTY[]    = {0};           /* len will be 0 */
static const unsigned char ZEROS8[]   = {0,0,0,0,0,0,0,0};
static const unsigned char ZEROS16[]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const unsigned char ZEROS32[32]= {0};
static const unsigned char ZEROS64[64]= {0};
static const unsigned char ONES8[]    = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const unsigned char BYTE00[]   = {0x00};
static const unsigned char BYTE01[]   = {0x01};
static const unsigned char BYTE80[]   = {0x80};
static const unsigned char BYTEFF[]   = {0xFF};

static int run_tests(void)
{
    int pass = 0, fail = 0;

    /* Macro to define and run one test */
#define TEST(label_, data_, len_, bits_, expected_) \
    do { \
        char* got = hash_bytes((data_), (len_), (bits_)); \
        int ok = (strcmp(got, (expected_)) == 0); \
        printf("[%s] %d-bit  %-46s\n", ok ? "PASS" : "FAIL", (bits_), (label_)); \
        if (!ok) { \
            printf("       expected: %s\n", (expected_)); \
            printf("       got:      %s\n", got); \
            fail++; \
        } else { pass++; } \
        free(got); \
    } while(0)

#define TEST_STR(label_, str_, bits_, expected_) \
    do { \
        char* got = hash_str((str_), (bits_)); \
        int ok = (strcmp(got, (expected_)) == 0); \
        printf("[%s] %d-bit  %-46s\n", ok ? "PASS" : "FAIL", (bits_), (label_)); \
        if (!ok) { \
            printf("       expected: %s\n", (expected_)); \
            printf("       got:      %s\n", got); \
            fail++; \
        } else { pass++; } \
        free(got); \
    } while(0)

    printf("Secasy Test Vector Verification\n");
    printf("================================\n");
    printf("Rounds : %lu\n", numberOfRounds);
    printf("Date   : 2026-03-15\n\n");

    /* ── 512-bit vectors ─────────────────────────────────────────── */
    printf("--- 512-bit ---\n");
    TEST("\"\" (empty)",     EMPTY,   0,  512, "171201cadb67c9037cab4d0774f1f4efd853df21737c875319f9288a6bd4eff5b36037eba14c9bcc6c8ddf75faed794a8a9a29d9764a2917636501221db14740");
    TEST_STR("\"a\"",        "a",     512, "7a808caa3e4a79bc840618351a3d2c05080c19885554f34e03d92680b5e9e26924fd9211318a8da40b559b1ed9cbead7ba06e4172b9eefd83e94d814a9b0db12");
    TEST_STR("\"z\"",        "z",     512, "dca7959152e282f0f604fd560535d5161cf80e522224cd922a72b1d7ac938e392d47bc7471bf93a877ef480e049fb4698369ab1ccb34871e28cbf3c1945c95c0");
    TEST_STR("\"0\"",        "0",     512, "a71fc3757caf88fe054a1987c8efd3d987bcf075521f0299000769f7d56d0605c7dfe489ccaad36165f581bdd812b5818054f00f9a73a93e7a7e47a5d0a10b73");
    TEST_STR("\"abc\"",      "abc",   512, "0fcdfe71b624b788623d1950214285172b73963011fb020a6ba436e1fb66ed357fd8c789933b08e638e6d9c8d0d86924a4e9985c7a7bc98b136afa2a88d320d4");
    TEST_STR("\"secasy\"",   "secasy",512, "fbc6840e290b51b709998bfe48feb479f8fba2df646803cd586dbf5970908ce6c9f999556915dfbf13af7ad5a38427c68b43a1d51b9de5f1ae3c80054e2d9277");
    TEST_STR("\"1234567890\"","1234567890",512,"37f1a1e3e697a10af2e4e4e3fe90ec69894467df2c11f1d6f3d21c51758c600256298db3a49046f17f7803d23b3af9ef13174dfa67bd8f6655b1ac2a148970a1");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "1fe172638cf0cb765ec1dd61fb6e10a63022bfcfa910b7b8f949c03afa7310201d798d68986fb19784f06cbec6201e89974e017c59595a818efdda5ff427e1fc");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "459b054bbb41d8deed1614dbf507c0fb36e6419f6b2e4d27692b6300ecba1c8b15dad6c18888f9faac0f9c3ed1f08e7d4d719cc821ba2b96e97309304fc82a03");
    TEST("0x00",             BYTE00,  1,  512, "02c7e717c3c562956f8e6331c0c87c82a14b39adac337ce1bfb2dad6cdc6188dabdf2d7041d971f043a41a13cd6c7e141c9fc451909c294522fdb047cae62b63");
    TEST("0x01",             BYTE01,  1,  512, "304e386689f5ae2ae32ffee66086d68f2ef90f2c228e81fdd530970beab284c2ab7bc08d65d3913d31df51e572fbec56827cfc42cd208513b7e8545130ccc576");
    TEST("0x80",             BYTE80,  1,  512, "4f4f36598dbdb1bebb48d07514dcbc4e5601193ff1acf0170e247d5a049326f8bf83741c5f3b35d4cd7afa6927707115133154a6a91721c21d2548bc2b56f8f2");
    TEST("0xFF",             BYTEFF,  1,  512, "af0be72a1c79357f87144c4632621d526e486f77698bccd3bf32715c236d8562886833d6e6de75de1b894e560030906d5cf5d91a13c16ec4cac40684940d0ddc");
    TEST("0x00 x8",          ZEROS8,  8,  512, "de51092bb53e1ab98d1e1f050cddeb1b01cae6be95a353240789418035c1cd08582d5261536418aeeb403357dcfd35f18b5fa5f438db6a2b06fd79560c49f139");
    TEST("0x00 x16",         ZEROS16, 16, 512, "41ad1d14dc9feaa44b7008a4d2c7a8573f1472729af48d0ff6d23cb9c4e6011d800390f49fba039a277379ef92807f1577179beee9e996607da73bce94a0696f");
    TEST("0x00 x32",         ZEROS32, 32, 512, "131e877bc9f7e0c42f31caaf59c2485c50f4eb862f087362d40b0aa8d15600f9df160e3a16bead6fe67bf62e61464a4080b46dfa14f6ac52202a7bbc162b6cdc");
    TEST("0x00 x64",         ZEROS64, 64, 512, "e639333a956c48d76141c0fe452a06bfe55244bfe0143ab35b27960b441343353895bb1a224e9bf573c92c0e9b39b405a3e8f3ee866325f6536720a2d3b1ed89");
    TEST("0xFF x8",          ONES8,   8,  512, "76b1e8fc174db951b20e562ba056b41a69c61c44d43c6927e1a032e1146b3be63b1c59653978121a0b13f3c3b09b43e5174328b35b6f04324004ee0aebcff01c");
    TEST_STR("\"Markus\"",   "Markus",512, "e9d8503507996b04a2e828857579471351a2c1050cee893c8ea9672ad7dc6e9fef07d75c752746c9d057fa00c9cecdaf52a56da7aafab98fa286b88503c78c00");
    TEST_STR("\"Anna\"",     "Anna",  512, "32dce55effa5d1621608a4e659e7e39d94f19c87bfc396fd43296009b000158503acc3b46e2fd3fcf6717f0cfaec70163dcf8d3671a135e4870762c230db22d6");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)",     EMPTY,   0,  64, "171201cadb67c903");
    TEST_STR("\"a\"",        "a",     64, "7a808caa3e4a79bc");
    TEST_STR("\"abc\"",      "abc",   64, "0fcdfe71b624b788");
    TEST_STR("\"secasy\"",   "secasy",64, "fbc6840e290b51b7");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "1fe172638cf0cb76");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "459b054bbb41d8de");
    TEST_STR("\"Markus\"",   "Markus",64, "e9d8503507996b04");
    TEST_STR("\"Anna\"",     "Anna",  64, "32dce55effa5d162");

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
