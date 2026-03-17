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
    printf("Date   : 2026-03-16\n\n");

    /* ── 512-bit vectors ─────────────────────────────────────────── */
    printf("--- 512-bit ---\n");
    TEST("\"\" (empty)",     EMPTY,   0,  512, "170cc4dcf0d6f18211e1b34dab26e019c2bd077cb542f53d717544b3b8151f622489f9c95b5ac055c7b13991597469ad62c84674564a659cf6e2298a794cb8a5");
    TEST_STR("\"a\"",        "a",     512, "920d7d34b8ac5dbf824de63a33dbbcb2c0ef18d6c08c424278f0c8c12a6b2ca60d22b1bcf6cb1637edada5fc98bc0035457a187825b3e65482d99f15a764dbdd");
    TEST_STR("\"z\"",        "z",     512, "89ccc5f01da80129a7134aa98b06e1a8e7d5328b5c04ed60644128720e36c5f55da025f9fa16c48ffc25aec6d5319e653afa518a064cc73a0bcc67260e0891e7");
    TEST_STR("\"0\"",        "0",     512, "c9c16ab50a0ece4959ca4981fea665a7a15f5ad8bd461ee19070a1f63508ef62f78d4b6fb60d121d61ef1c3fb26d3cf8edf81cee1b4879fd08b4100a557a29cc");
    TEST_STR("\"abc\"",      "abc",   512, "8fd33a7c0c31b821d4f98f934204b5826e5fdd4403afdf9b4f024d4a631b2b9c3ced0a04ae8f0816244c8fcc0daf3035075cac4c7b9d1a3f0304c8809ede9cf9");
    TEST_STR("\"secasy\"",   "secasy",512, "51c42cb1e08f95bc5358d6f6b58e8c78f2c05853115b6c6ae2c873d864c6fffdd7a8f8f8e755b928ce0ef3b586e4bc4c84093f8a572230e21dbfa5e4a714e961");
    TEST_STR("\"1234567890\"","1234567890",512,"2027f8e1b9a9c8b163af2e6d83ee9b58287b3e96e991686ebdefc1755742fd0883080a8a179cf0e2f080d65ac991d3d9e1fc44390c316ec02f6c3e6b3610b1da");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "feae885d7e5d5edec261bba1f6cd8b227424617a1cfd392098ee5a5e8798a9bd0ecced6678737ec84d571c31e9058778496e951a0b3eadedd6958c141d1b78e0");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "860e1653369d48aa6ea027249e1e60c99d292c9ed73ce43d91f0efd67b86e4ef40e207d28cb533b28b4a753f1786be42989dcd4ebf6ef86962e4fb1b053946c5");
    TEST("0x00",             BYTE00,  1,  512, "92cc8686ac8048228da8db8195451497412b4021c0449c0cb4fadbdbc018f1bd46af79b7c652f2c8041322a758809a4a7be9aaf4cbd69933849048fb231db598");
    TEST("0x01",             BYTE01,  1,  512, "42fa914a659125ed69ad1d66ef6bdebb11eb767901a44cfb4bce1a00a61644db84a92588b09be4be456bb9e871cb9a1f6a3c54dae01015702ac249eb7be4912b");
    TEST("0x80",             BYTE80,  1,  512, "e27b1ca5ba734b2d98f8928cd1e707458323bc579de31175dceedd5c6c42a010fdca02c956c188cffdb6086f6f1fc7e1c79b274a81483e18a5425fb01931489a");
    TEST("0xFF",             BYTEFF,  1,  512, "02ec7ca8d713092fc59d5617fd3dd8806fa58ed4edbddcd8661467fba78e375b2203b95e489fd4e69212fa02595cd1f5ec331d336782a60b23ef2cacfbb9b8ce");
    TEST("0x00 x8",          ZEROS8,  8,  512, "243db3e455f7dbf22bb6e7233afa2a7a03699e774dd84d34ac45a1e84f5735e4685aa39440dacf12deead17402f9d41507fc4eae726e42d4271166764a952bdc");
    TEST("0x00 x16",         ZEROS16, 16, 512, "70b3869e26d522a9ba2146fb456a49fc3599f2021079e14641bab1e4d612e439aed2424d53910c11ed4b5a37078e9153ffd16795bc7bcf8af75ef52f24793d20");
    TEST("0x00 x32",         ZEROS32, 32, 512, "6086e805cef420cdec5960a2da7455bcd01fbc47828bc2f758fb3d3965fc25576a6693f31a2f971d82158d85e54d40fa19e8873dde538ef5dfcd065e882bda2c");
    TEST("0x00 x64",         ZEROS64, 64, 512, "2ddec90fa0f4d268b9a6c1f41c20e7cf5ce9d668ab66da5c5e7de598b4011172a1b81c3005e5ed2b417e0f18e03f42aebd3fb5048d9effce45e3af296363adbc");
    TEST("0xFF x8",          ONES8,   8,  512, "815c958b076077f548abaf69ba01834ad6b93f2cc7e2be9f88d56b48a1f3c68af8d1af69814e5afc6eb61e86ab4fcb06a61513f3a23a1f6de21bf114dd197730");
    TEST_STR("\"Markus\"",   "Markus",512, "f60952a118cb936b815904a40cc320b168778e8ca9c2821e89af15a33757467afdecee36400acbcf3305afd65d8b3ee10e6a54c1baa1a85156350e91c41bc5f4");
    TEST_STR("\"Anna\"",     "Anna",  512, "52a4087c20be454559e8fa3ea3c13f25c4b213dffa6df6c13c1eb7e2e0fabaedb5f3eff0d88055da3c6e0f1ae24c72965487768f2d7cb917f8d9fe49b959f181");
    TEST_STR("\"markus\"",   "markus",512, "359a202190a89defd891e743f90b90ee6f14e9c9c9cd2368fe5464fe6e29348c2b2dc030e49bc829f53cdfd620ad6b8a3ee65b1c34465d653ff47eaf6205ed1f");
    TEST_STR("\"maRkus\"",   "maRkus",512, "66f4b76a53345f34b841a5364684395f9723a46b2fb9e36e51aefbd14af178201a8326a0a4d7b963304a16b1a426c0770a0bd09b41775ca43a034bd2d4046787");
    TEST_STR("\"Hanna\"",    "Hanna", 512, "44986085b29f103e0e7fecf3f02e74f475ab27f87d016a0b409e400cd9d17bd972135ad23c0530d4606cb35b5f2c99494ff1daa82073c5ddc5e2f40ec18a9c9b");
    TEST_STR("\"Antonette\"","Antonette",512,"7e9e13182dadc6b5223ba4f8f11d6724b3074c6a74bfadda0753864ac77cebe7e1e317fbe1fbb2dc3a716245b946ec1a9a254979ecc9c6634fde5700a257202b");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)",     EMPTY,   0,  64, "170cc4dcf0d6f182");
    TEST_STR("\"a\"",        "a",     64, "920d7d34b8ac5dbf");
    TEST_STR("\"abc\"",      "abc",   64, "8fd33a7c0c31b821");
    TEST_STR("\"secasy\"",   "secasy",64, "51c42cb1e08f95bc");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "feae885d7e5d5ede");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "860e1653369d48aa");
    TEST_STR("\"Markus\"",   "Markus",64, "f60952a118cb936b");
    TEST_STR("\"Anna\"",     "Anna",  64, "52a4087c20be4545");
    TEST_STR("\"markus\"",   "markus",64, "359a202190a89def");
    TEST_STR("\"maRkus\"",   "maRkus",64, "66f4b76a53345f34");
    TEST_STR("\"Hanna\"",    "Hanna", 64, "44986085b29f103e");
    TEST_STR("\"Antonette\"","Antonette",64,"7e9e13182dadc6b5");

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
