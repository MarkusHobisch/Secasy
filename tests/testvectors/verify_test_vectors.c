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
    TEST("\"\" (empty)", EMPTY, 0, 512, "b7f8505682b46bb6f46b3c43f5e8007c31b7d5dae7564298c3e777ff6daa72f9c470ddc2392ece703cafdbbb1f5750c31d91b88f79d4bada4242d9eaeeac6bfe");
    TEST_STR("\"a\"", "a", 512, "291abc5b712cc388b5a1ea370365e216c6fbf9a6b07a37bc5cea23be0bfa26904ed1b454b764322fa241263da401375bed7f3722aa8ce2fa16e7dd1d6fe887cb");
    TEST_STR("\"z\"", "z", 512, "f3c68bf71e6584e3a6759768bcd38d81d40cab1f17cde779cb9cf82d17712003e550fa98205141060e5d6e4e4748e7b48ecb3f24e68d04eb626a9f8663df8039");
    TEST_STR("\"0\"", "0", 512, "cde76b0a6492506e6fc13da5d2d77f2b4ccb186598f7ff8cd2da97f218528c3cfe4bc89e120b7fdff3580f1b2e9b05ce83fc46de4dd2d3722624c5765bb45cc8");
    TEST_STR("\"abc\"", "abc", 512, "d1197317b6f4c6adc516b29549dcb9d274abdb64022dba8a003e3a5dd998986c2f29c20fca7b57d01e5852bdbd264ad9ec46f85f346cb93e6ab49209736472d8");
    TEST_STR("\"secasy\"", "secasy", 512, "44cf221ec8fde8d06849176fdbe4709ce36c992019a690b5e356879b786cc3f2b9f63c883e89989e48666bbdacb739756e3d65d0b4acea70a703c575e7fe1a5a");
    TEST_STR("\"1234567890\"", "1234567890", 512, "d235c8a13224ae64207796ffbb5f28bc92eacd8caadfb8f8527565166764de9d498ff04f78f7ed20ae1fb4b4eb454110a956da4fc24861796dd3b24b937d8edc");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 512,
             "73aed62572b95b33b1762710af4646e6aaf945bb1a835dbb19b5cb2603c49798b782a5571eb17d0b66fc3b418eeb3d0bbb589d68ca2b2b01dfc560664b648123");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 512,
             "b49d9d9ac37b75b6e7a43e8f455c57edd2361749e41edb6c7823ebd0d2eaced8e84b634b9c6240fd54451818f74da471c5b80638d9a243d1ebbfe2cd40ad56e3");
    TEST("0x00", BYTE00, 1, 512, "f7298e77c73b8ae638e86b1f8e6c761f6076b0a8d056a9a56a482644e00c97be149f38085369487e1c6a3165c06553260368ca2060a6aebeaeb54ed0446d904f");
    TEST("0x01", BYTE01, 1, 512, "4d1e47018e6b172210b0c342b0ac5610927064762034eb79771cf509b2b686336364728331af379528c58db8055d03938e2b3f4e13204b39825954047c052414");
    TEST("0x80", BYTE80, 1, 512, "923dfc8031b6d2ef6b48d44a49d78f6521fdb2bf1dd229ea74c0eaf43aa05a1fb060eb61a2e4256cf6a376b2ef912eb283afb68fb228daa156997639ff92856a");
    TEST("0xFF", BYTEFF, 1, 512, "86e121ab1950265f39b0d32f9ca872b6adb42131efde18770c442e3222ce110342504c159a01e5e0871765b028e48e910a8252dd59e6f78351f046e657da075f");
    TEST("0x00 x8", ZEROS8, 8, 512, "39cd2afff4c7cff4fd8c281a57c3ff330af4b0084249084d680c46d656ea49fea96bb058e022b6ddc6998790782b08cb3ebbb40167202b2ca1996a0f2f371133");
    TEST("0x00 x16", ZEROS16, 16, 512, "aac4bf3fb25ffc63025def038f4b385c3d6568ce5222259fe64d8c0d178299efa36fe2fe35de2be93f18edb9039979973adbef8ca06613a12d696b8458e4f96e");
    TEST("0x00 x32", ZEROS32, 32, 512, "579a76b2b0d4d715303ce08fbea37e8be05bbe45228f1e9ecd55a92c09851297aef64ace0248a961b603a3889de9cea375f57c29b01c0072a0efb143e4ae97a8");
    TEST("0x00 x64", ZEROS64, 64, 512, "74cfef3d8c78dff43c65d959d191881e5a1adbee92e6a01d73d830dcd36ab9f9117a2450740f286407a4b3bb6c02357d9ebd5480683990e8b675b3b71df7fbc6");
    TEST("0xFF x8", ONES8, 8, 512, "127c338fdfe68e2bf7e29a3ddd94e28781dc0d7345025e6187ba7432ae1a57102c56a3763faa77662da4340d7d6568de5f8824af034c853d3fbf191af2f24fe1");
    TEST_STR("\"Markus\"", "Markus", 512, "1b18d19d83393932d0ec6a496d297dd4d1fa53bb4cee8fe516e1956fb8ca7a9935ef0c957518ead12a17415394b4ac59aec39ae2e4d68210472a3081970465f9");
    TEST_STR("\"Anna\"", "Anna", 512, "bd3b6c745f38b580c83e60137a8b60f756b7ae02714bcfe358a77474830bbae24f4c48b58ae0b4b27b85ea99329333c720d0f9aaad32ecafc57711e995cb185d");
    TEST_STR("\"markus\"", "markus", 512, "56e0618e6712a1b3aa79c6e3f45ef25b90a5e0732ff523aebfed0d14198b029021b30d83817b6b2e43ef60155a49bad7fad5c549aadaeaf57a7c94a38cfc789f");
    TEST_STR("\"maRkus\"", "maRkus", 512, "60776a80a7451b8b79094ca13795bcdf6aaaacc337cd5b051f2bef52d18b615f0b6e4955d1f0d18e9cdc319d7a68f5a6597e897ccac5a4ac9ba546d71a46afab");
    TEST_STR("\"Hanna\"", "Hanna", 512, "c1d70e05bba8ffcc03e1666ffbb573fe2b918bd8cc338399b4f50693028367198745d1e7a1a3208357d8fbd09230a5f83e408b9032e4bb41c44011935b24dfc0");
    TEST_STR("\"Antonette\"", "Antonette", 512, "f2e7b839bbca42dfd31fbd3ad803dfd0d5b73d4c21db7a30ea1f7196705095229b337ecb311570145a1377259d47b742987105518213642984f66beb6493adb6");

    /* ── 64-bit vectors ──────────────────────────────────────────── */
    printf("\n--- 64-bit ---\n");
    TEST("\"\" (empty)", EMPTY, 0, 64, "b7f8505682b46bb6");
    TEST_STR("\"a\"", "a", 64, "291abc5b712cc388");
    TEST_STR("\"abc\"", "abc", 64, "d1197317b6f4c6ad");
    TEST_STR("\"secasy\"", "secasy", 64, "44cf221ec8fde8d0");
    TEST_STR("pangram (dog)", "The quick brown fox jumps over the lazy dog", 64, "73aed62572b95b33");
    TEST_STR("pangram (cog)", "The quick brown fox jumps over the lazy cog", 64, "b49d9d9ac37b75b6");
    TEST_STR("\"Markus\"", "Markus", 64, "1b18d19d83393932");
    TEST_STR("\"Anna\"", "Anna", 64, "bd3b6c745f38b580");
    TEST_STR("\"markus\"", "markus", 64, "56e0618e6712a1b3");
    TEST_STR("\"maRkus\"", "maRkus", 64, "60776a80a7451b8b");
    TEST_STR("\"Hanna\"", "Hanna", 64, "c1d70e05bba8ffcc");
    TEST_STR("\"Antonette\"", "Antonette", 64, "f2e7b839bbca42df");

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
