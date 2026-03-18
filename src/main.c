#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <errno.h>
#include <stdint.h>
#if defined(_WIN32)
#include <sys/stat.h>
#define STAT_STRUCT struct _stat64
#define STAT_FN _stat64
#else
#include <sys/stat.h>
#define STAT_STRUCT struct stat
#define STAT_FN stat
#endif
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Defines.h"
#include "Printing.h"
#include "util.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;
static unsigned long maximumPrimeIndex = DEFAULT_MAX_PRIME_INDEX;
static char *inputFilename = NULL;
static char *inputString = NULL;
static unsigned char *inputHexBytes = NULL;
static size_t inputHexLen = 0;
static unsigned long long inputFileSize = 0ULL;

static void readInCommandLineOptions(int argc, char **argv);
static void readAndStoreNumberOfRoundsOption(void);
static void readAndStoreNumberOfMaximumPrimeIndexOption(void);
static void readAndStoreNumberOfBitsOption(void);
static void readAndStoreFilenameOption(void);
static void readAndStoreStringOption(void);
static void readAndStoreHexOption(void);
static void printHelperText(void);
static void printCommandLineOptions(void);
static void printStatistics(double cpuSeconds, double wallSeconds, unsigned long long fileSizeBytes);
static int getFileSize64(const char *path, unsigned long long *outSize);
static int parseUnsignedLong(const char *str, unsigned long *result);

int main(int argc, char **argv)
{
    char *hashValue = NULL;
    clock_t cpuStart = clock();
    double wallStart = wall_time_seconds();

    readInCommandLineOptions(argc, argv);

#if DEBUG_MODE
    g_debug_fp = fopen("debug.txt", "w");
    if (!g_debug_fp)
    {
        fprintf(stderr, "[WARNING] Could not open debug.txt for writing\n");
    }
#endif

    printCommandLineOptions();
    initFieldWithDefaultNumbers(maximumPrimeIndex);

    if (inputHexBytes)
    {
#if DEBUG_MODE
        printInputBits(inputHexBytes, inputHexLen);
#endif
        processBuffer(inputHexBytes, inputHexLen);
    }
    else if (inputString)
    {
#if DEBUG_MODE
        printInputBits((const unsigned char *)inputString, strlen(inputString));
#endif
        processBuffer((const unsigned char *)inputString, strlen(inputString));
    }
    else
    {
        if (getFileSize64(inputFilename, &inputFileSize) != 0)
        {
            inputFileSize = 0ULL; // Non-fatal
        }
#if DEBUG_MODE
        {
            FILE *dbgFile = fopen(inputFilename, "rb");
            if (dbgFile)
            {
                fseek(dbgFile, 0, SEEK_END);
                long dbgLen = ftell(dbgFile);
                fseek(dbgFile, 0, SEEK_SET);
                if (dbgLen > 0)
                {
                    unsigned char *dbgBuf = (unsigned char *)malloc((size_t)dbgLen);
                    if (dbgBuf)
                    {
                        size_t r = fread(dbgBuf, 1, (size_t)dbgLen, dbgFile);
                        printInputBits(dbgBuf, r);
                        free(dbgBuf);
                    }
                }
                fclose(dbgFile);
            }
        }
#endif
        readAndProcessFile(inputFilename);
    }

#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField("Init Phase");
    printPathMap();
    printPrimeIndexes();
    printColorIndexes();
    printSumsAndValues();
#endif

    hashValue = calculateHashValue();

#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField("Processing Phase");
    printSumsAndValues();
#endif

    if (hashValue)
    {
        LOG_INFO("HASH VALUE (%d-bit): %s", hashLengthInBits, hashValue);
    }
    else
    {
        LOG_ERROR("Hash calculation failed");
    }

    double cpuSeconds = (double)(clock() - cpuStart) / CLOCKS_PER_SEC;
    double wallSeconds = wall_time_seconds() - wallStart;
    printStatistics(cpuSeconds, wallSeconds, inputFileSize);

    free(hashValue);
    free(inputFilename);
    free(inputString);
    free(inputHexBytes);
#if DEBUG_MODE
    if (g_debug_fp)
    {
        fclose(g_debug_fp);
        g_debug_fp = NULL;
    }
    LOG_INFO("Debug output written to debug.txt");
#endif
    return EXIT_SUCCESS;
}

static void readInCommandLineOptions(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "r:i:n:f:s:x:h")) != -1)
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
        case 'h':
            printHelperText();
            exit(EXIT_SUCCESS);
        default:
            LOG_ERROR("Usage: %s supported arguments [-r] [-i] [-n] [-f] [-s] [-x] [-h]", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    int inputCount = (inputFilename ? 1 : 0) + (inputString ? 1 : 0) + (inputHexBytes ? 1 : 0);
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
}

static void readAndStoreNumberOfRoundsOption()
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
    numberOfRounds = val;
}

static void readAndStoreNumberOfMaximumPrimeIndexOption()
{
    unsigned long val;
    if (parseUnsignedLong(optarg, &val) != 0)
    {
        LOG_ERROR("Invalid value for maximum prime index");
        exit(EXIT_FAILURE);
    }
    maximumPrimeIndex = val;
}

static void readAndStoreNumberOfBitsOption()
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
    hashLengthInBits = (int)val;
}

static void readAndStoreFilenameOption()
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing filename after -f option");
        printHelperText();
        exit(EXIT_FAILURE);
    }
    char *dup = secasy_strdup(optarg);
    if (!dup)
    {
        LOG_ERROR("Memory allocation failed for filename");
        exit(EXIT_FAILURE);
    }
    inputFilename = dup;
}

static void readAndStoreStringOption()
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing string after -s option");
        printHelperText();
        exit(EXIT_FAILURE);
    }
    char *dup = secasy_strdup(optarg);
    if (!dup)
    {
        LOG_ERROR("Memory allocation failed for input string");
        exit(EXIT_FAILURE);
    }
    inputString = dup;
}

static void readAndStoreHexOption()
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing hex string after -x option");
        printHelperText();
        exit(EXIT_FAILURE);
    }

    /* Count commas to estimate max number of bytes */
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
        /* Skip whitespace and commas */
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

    inputHexBytes = bytes;
    inputHexLen = count;
}

static void printHelperText()
{
    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------+\n");
    printf("| Arguments: [-r] [-i] [-n] [-f] [-s] [-x] [-h]                                                      |\n");
    printf("|  -n <bits>  : bit size of hash value (power of two, >= %d)                                       |\n", HASH_OUTPUT_BITS);
    printf("|  -i <index> : max prime index for calculation of prime numbers                                   |\n");
    printf("|  -r <rounds>: number of processing rounds                                                        |\n");
    printf("|  -f <file>  : input filename                                                                     |\n");
    printf("|  -s <string>: hash a string directly                                                             |\n");
    printf("|  -x <hex>   : hash hex bytes, e.g. -x \"0x45,0x47,0x78\"                                         |\n");
    printf("|  -h         : show this help                                                                     |\n");
    printf("+--------------------------------------------------------------------------------------------------+\n\n");
}

static void printCommandLineOptions()
{
    if (inputHexBytes)
    {
        /* Print hex bytes for logging */
        char hexBuf[1024];
        size_t off = 0;
        for (size_t i = 0; i < inputHexLen && off + 5 < sizeof(hexBuf); i++)
        {
            if (i > 0)
                hexBuf[off++] = ',';
            off += (size_t)snprintf(hexBuf + off, sizeof(hexBuf) - off, "0x%02x", inputHexBytes[i]);
        }
        hexBuf[off] = '\0';
        LOG_INFO("inputHex: %s (%zu bytes)", hexBuf, inputHexLen);
    }
    else if (inputString)
        LOG_INFO("inputString: \"%s\" (%zu bytes)", inputString, strlen(inputString));
    else
        LOG_INFO("inputFilename: %s", inputFilename ? inputFilename : "(null)");
    LOG_INFO("numberOfRounds: %lu", numberOfRounds);
    LOG_INFO("maximumPrimeIndex: %lu", maximumPrimeIndex);
    LOG_INFO("hashLengthInBits: %d", hashLengthInBits);
    if (inputFileSize)
    {
        LOG_INFO("detected file size: %llu bytes", (unsigned long long)inputFileSize);
        LOG_INFO("hashing...");
    }
    else
    {
        LOG_INFO("hashing...");
    }
}

static void printStatistics(double cpuSeconds, double wallSeconds, unsigned long long fileSizeBytes)
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

static int getFileSize64(const char *path, unsigned long long *outSize)
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
