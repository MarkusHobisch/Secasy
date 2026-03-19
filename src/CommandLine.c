#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#if defined(_WIN32)
#include <sys/stat.h>
#define STAT_STRUCT struct _stat64
#define STAT_FN _stat64
#else
#include <sys/stat.h>
#define STAT_STRUCT struct stat
#define STAT_FN stat
#endif
#include "CommandLine.h"
#include "Defines.h"
#include "util.h"

#define MAX_HEX_DISPLAY_BYTES 200

static CommandLineOptions_t opts;

static int parseUnsignedLong(const char *str, unsigned long *result);
static char *dupOptarg(const char *flagName);
static void readAndStoreNumberOfRoundsOption(void);
static void readAndStoreNumberOfMaximumPrimeIndexOption(void);
static void readAndStoreNumberOfBitsOption(void);
static void readAndStoreFilenameOption(void);
static void readAndStoreStringOption(void);
static void readAndStoreHexOption(void);
static void printHelperText(void);

CommandLineOptions_t parseCommandLineOptions(int argc, char **argv)
{
    opts.numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
    opts.maximumPrimeIndex = DEFAULT_MAX_PRIME_INDEX;
    opts.hashLengthInBits = DEFAULT_BIT_SIZE;
    opts.inputFilename = NULL;
    opts.inputString = NULL;
    opts.inputHexBytes = NULL;
    opts.inputHexLen = 0;

    int opt;
    while ((opt = getopt(argc, argv, "r:i:n:f:s:x:deh")) != -1)
    {
        switch (opt)
        {
        case 'r':
            readAndStoreNumberOfRoundsOption();
            break;
        case 'i':
            readAndStoreNumberOfMaximumPrimeIndexOption();
            break;
        case 'n':
            readAndStoreNumberOfBitsOption();
            break;
        case 'f':
            readAndStoreFilenameOption();
            break;
        case 's':
            readAndStoreStringOption();
            break;
        case 'x':
            readAndStoreHexOption();
            break;
        case 'd':
            g_debug_mode = 1;
            break;
        case 'e':
            g_debug_mode = 1;
            g_debug_extended = 1;
            break;
        case 'h':
            printHelperText();
            exit(EXIT_SUCCESS);
        default:
            LOG_ERROR("Usage: %s supported arguments [-r] [-i] [-n] [-f] [-s] [-x] [-d] [-e] [-h]", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int inputCount = (opts.inputFilename ? 1 : 0) + (opts.inputString ? 1 : 0) + (opts.inputHexBytes ? 1 : 0);
    if (inputCount == 0)
    {
        LOG_ERROR("Missing input. Provide -f <file>, -s <string>, or -x <hex>");
        exit(EXIT_FAILURE);
    }

    if (inputCount > 1)
    {
        LOG_ERROR("Cannot use -f, -s, and -x together. Choose one.");
        exit(EXIT_FAILURE);
    }

    return opts;
}

void printCommandLineOptions(const CommandLineOptions_t *o)
{
    if (o->inputHexBytes)
    {
        if (o->inputHexLen > MAX_HEX_DISPLAY_BYTES)
        {
            LOG_ERROR("Hex input too long for display (%zu bytes, max %d)", o->inputHexLen, MAX_HEX_DISPLAY_BYTES);
            exit(EXIT_FAILURE);
        }

        printf("[INFO] inputHex: ");
        for (size_t i = 0; i < o->inputHexLen; i++)
            printf("%s0x%02x", i > 0 ? "," : "", o->inputHexBytes[i]);
        printf(" (%zu bytes)\n", o->inputHexLen);
    }
    else if (o->inputString)
        LOG_INFO("inputString: \"%s\" (%zu bytes)", o->inputString, strlen(o->inputString));
    else
        LOG_INFO("inputFilename: %s", o->inputFilename ? o->inputFilename : "(null)");

    LOG_INFO("numberOfRounds: %lu", o->numberOfRounds);
    LOG_INFO("maximumPrimeIndex: %lu", o->maximumPrimeIndex);
    LOG_INFO("hashLengthInBits: %d", o->hashLengthInBits);
    LOG_INFO("hashing...");
}

void printStatistics(const double cpuSeconds, const double wallSeconds, const unsigned long long fileSizeBytes)
{
    double fileMB = (fileSizeBytes > 0ULL) ? (double)fileSizeBytes / BYTES_PER_MB : 0.0;
    double hashRateWall = (wallSeconds > 0.0 && fileSizeBytes > 0ULL) ? (fileMB / wallSeconds) : 0.0;
    double hashRateCpu = (cpuSeconds > 0.0 && fileSizeBytes > 0ULL) ? (fileMB / cpuSeconds) : 0.0;

    printf("\n--- Statistics ---\n");
    printf("CPU time:  %.3f s\n", cpuSeconds);
    printf("Wall time: %.3f s\n", wallSeconds);
    if (fileSizeBytes)
    {
        printf("File size: %.2f MB\n", fileMB);
        printf("Hash rate (wall): %.2f MB/s\n", hashRateWall);
        printf("Hash rate (CPU) : %.2f MB/s\n", hashRateCpu);
    }
    else
    {
        printf("File size: (unknown)\n");
    }
}

int getFileSize64(const char *path, unsigned long long *outSize)
{
    if (!path || !outSize)
    {
        return -1;
    }

    STAT_STRUCT st;
    if (STAT_FN(path, &st) != 0)
    {
        return -1;
    }

    if (st.st_size < 0)
    {
        return -1;
    }

    *outSize = (unsigned long long)st.st_size;
    return 0;
}

/* ── Private helpers ────────────────────────────────────────────────── */

static void readAndStoreNumberOfRoundsOption(void)
{
    unsigned long val;
    if (parseUnsignedLong(optarg, &val) != 0)
    {
        LOG_ERROR("Invalid value for rounds");
        exit(EXIT_FAILURE);
    }

    if (val < DEFAULT_NUMBER_OF_ROUNDS)
    {
        LOG_ERROR("Requested %lu rounds, but minimum is %d.",
                  val, DEFAULT_NUMBER_OF_ROUNDS);
        exit(EXIT_FAILURE);
    }

    opts.numberOfRounds = val;
}

static void readAndStoreNumberOfMaximumPrimeIndexOption(void)
{
    unsigned long val;
    if (parseUnsignedLong(optarg, &val) != 0)
    {
        LOG_ERROR("Invalid value for maximum prime index");
        exit(EXIT_FAILURE);
    }

    opts.maximumPrimeIndex = val;
}

static void readAndStoreNumberOfBitsOption(void)
{
    char *end_ptr = NULL;
    errno = 0;
    long val = strtol(optarg, &end_ptr, 10);
    if (errno != 0 || end_ptr == optarg || *end_ptr != '\0')
    {
        LOG_ERROR("Invalid value for bit size");
        exit(EXIT_FAILURE);
    }

    if (val < HASH_OUTPUT_BITS)
    {
        LOG_ERROR("Bit size lower than %d not supported", HASH_OUTPUT_BITS);
        exit(EXIT_FAILURE);
    }

    if (!is_power_of_two(val))
    {
        LOG_ERROR("Bit size must be a power of two (64, 128, 256, ...)");
        exit(EXIT_FAILURE);
    }

    opts.hashLengthInBits = (int)val;
}

static void readAndStoreFilenameOption(void)
{
    opts.inputFilename = dupOptarg("f");
}

static void readAndStoreStringOption(void)
{
    opts.inputString = dupOptarg("s");
}

static void readAndStoreHexOption(void)
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing hex string after -x option");
        printHelperText();
        exit(EXIT_FAILURE);
    }

    size_t capacity = 1;
    for (const char *p = optarg; *p; p++)
    {
        if (*p == ',')
            capacity++;
    }

    unsigned char *bytes = (unsigned char *)malloc(capacity);
    if (!bytes)
    {
        LOG_ERROR("Memory allocation failed for hex input");
        exit(EXIT_FAILURE);
    }

    size_t count = 0;
    const char *ptr = optarg;
    while (*ptr)
    {
        while (*ptr == ',' || *ptr == ' ' || *ptr == '\t')
            ptr++;
        if (*ptr == '\0')
            break;

        char *end = NULL;
        errno = 0;
        unsigned long val = strtoul(ptr, &end, 16);
        if (errno != 0 || end == ptr || val > 0xFF)
        {
            LOG_ERROR("Invalid hex byte at position %zu: '%.10s'", count, ptr);
            free(bytes);
            exit(EXIT_FAILURE);
        }

        bytes[count++] = (unsigned char)val;
        ptr = end;
    }

    if (count == 0)
    {
        LOG_ERROR("No valid hex bytes found in -x argument");
        free(bytes);
        exit(EXIT_FAILURE);
    }

    opts.inputHexBytes = bytes;
    opts.inputHexLen = count;
}

static void printHelperText(void)
{
    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------+\n");
    printf("| Arguments: [-r] [-i] [-n] [-f] [-s] [-x] [-d] [-e] [-h]                                          |\n");
    printf("|  -n <bits>  : bit size of hash value (power of two, >= %d)                                       |\n", HASH_OUTPUT_BITS);
    printf("|  -i <index> : max prime index for calculation of prime numbers                                   |\n");
    printf("|  -r <rounds>: number of processing rounds                                                        |\n");
    printf("|  -f <file>  : input filename                                                                     |\n");
    printf("|  -s <string>: hash a string directly                                                             |\n");
    printf("|  -x <hex>   : hash hex bytes, e.g. -x \"0x45,0x47,0x78\"                                         |\n");
    printf("|  -d         : enable debug mode (writes debug.txt)                                               |\n");
    printf("|  -e         : enable extended debug output (implies -d)                                          |\n");
    printf("|  -h         : show this help                                                                     |\n");
    printf("+--------------------------------------------------------------------------------------------------+\n\n");
}

static char *dupOptarg(const char *flagName)
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing value after -%s option", flagName);
        printHelperText();
        exit(EXIT_FAILURE);
    }

    char *dup = secasy_strdup(optarg);
    if (!dup)
    {
        LOG_ERROR("Memory allocation failed for -%s value", flagName);
        exit(EXIT_FAILURE);
    }

    return dup;
}

static int parseUnsignedLong(const char *str, unsigned long *result)
{
    if (!str || !result)
        return -1;

    char *end_ptr = NULL;
    errno = 0;
    unsigned long val = strtoul(str, &end_ptr, 10);
    if (errno != 0 || end_ptr == str || *end_ptr != '\0' || val == 0UL)
        return -1;

    *result = val;
    return 0;
}
