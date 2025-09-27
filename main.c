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

// maRkus -> 57dc2a5605a7d4ef
// -n 64
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int numberOfBits = DEFAULT_BIT_SIZE;
static unsigned long maximumPrimeIndex = DEFAULT_MAX_PRIME_INDEX;
static char* inputFilename = NULL;
static unsigned long long inputFileSize = 0ULL;

static void readInCommandLineOptions(int argc, char** argv);
static void readAndStoreNumberOfRoundsOption(void);
static void readAndStoreNumberOfMaximumPrimeIndexOption(void);
static void readAndStoreNumberOfBitsOption(void);
static void readAndStoreFilenameOption(void);
static void printHelperText(void);
static void printCommandLineOptions(void);
static void printStatistics(double cpuSeconds, double wallSeconds, unsigned long long fileSizeBytes);
static int getFileSize64(const char* path, unsigned long long* outSize);

int main(int argc, char** argv)
{
    char* hashValue = NULL;
    clock_t cpuStart = clock();
    double wallStart = wall_time_seconds();

    readInCommandLineOptions(argc, argv);

    if (getFileSize64(inputFilename, &inputFileSize) != 0)
    {
        inputFileSize = 0ULL; // Non-fatal
    }

    printCommandLineOptions();

    initFieldWithDefaultNumbers(maximumPrimeIndex);
    readAndProcessFile(inputFilename);

#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField();
    printPrimeIndexes();
    printColorIndexes();
    printSumsAndValues();
#endif

    hashValue = calculateHashValue();

#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField();
    printSumsAndValues();
#endif

    if (hashValue)
    {
        LOG_INFO("HASH VALUE: %s", hashValue);
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
    return EXIT_SUCCESS;
}

static void readInCommandLineOptions(int argc, char** argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "r:i:n:f:h")) != -1)
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
            case 'h':
                printHelperText();
                exit(EXIT_SUCCESS);
            default:
                LOG_ERROR("Usage: %s supported arguments [-r] [-i] [-n] [-f] [-h]", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (!inputFilename)
    {
        LOG_ERROR("Missing input file. Provide one with -f <file>");
        exit(EXIT_FAILURE);
    }
}

static void readAndStoreNumberOfRoundsOption()
{
    char* end_ptr = NULL;
    errno = 0;
    unsigned long val = strtoul(optarg, &end_ptr, 10);
    if (errno != 0 || end_ptr == optarg || *end_ptr != '\0' || val == 0UL)
    {
        LOG_ERROR("Invalid value for rounds");
        exit(EXIT_FAILURE);
    }
    numberOfRounds = val;
}

static void readAndStoreNumberOfMaximumPrimeIndexOption()
{
    char* end_ptr = NULL;
    errno = 0;
    unsigned long val = strtoul(optarg, &end_ptr, 10);
    if (errno != 0 || end_ptr == optarg || *end_ptr != '\0' || val == 0UL)
    {
        LOG_ERROR("Invalid value for maximum prime index");
        exit(EXIT_FAILURE);
    }
    maximumPrimeIndex = val;
}

static void readAndStoreNumberOfBitsOption()
{
    char* end_ptr = NULL;
    errno = 0;
    long val = strtol(optarg, &end_ptr, 10);
    if (errno != 0 || end_ptr == optarg || *end_ptr != '\0')
    {
        LOG_ERROR("Invalid value for bit size");
        exit(EXIT_FAILURE);
    }
    if (val < MIN_HASH_BITS)
    {
        LOG_ERROR("Bit size lower than %d not supported", MIN_HASH_BITS);
        exit(EXIT_FAILURE);
    }
    if (!is_power_of_two(val))
    {
        LOG_ERROR("Bit size must be a power of two");
        exit(EXIT_FAILURE);
    }
    numberOfBits = (int)val;
}

static void readAndStoreFilenameOption()
{
    if (!optarg || *optarg == '\0')
    {
        LOG_ERROR("Missing filename after -f option");
        printHelperText();
        exit(EXIT_FAILURE);
    }
    char* dup = secasy_strdup(optarg);
    if (!dup)
    {
        LOG_ERROR("Memory allocation failed for filename");
        exit(EXIT_FAILURE);
    }
    inputFilename = dup;
}

static void printHelperText()
{
    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------+\n");
    printf("| Arguments: [-r] [-i] [-n] [-f] [-h]                                                             |\n");
    printf("|  -n <bits>  : bit size of hash value (power of two, >= %d)                                       |\n", MIN_HASH_BITS);
    printf("|  -i <index> : max prime index for calculation of prime numbers                                  |\n");
    printf("|  -r <rounds>: number of processing rounds                                                        |\n");
    printf("|  -f <file>  : input filename                                                                     |\n");
    printf("|  -h         : show this help                                                                     |\n");
    printf("+--------------------------------------------------------------------------------------------------+\n\n");
}

static void printCommandLineOptions()
{
    LOG_INFO("inputFilename: %s", inputFilename ? inputFilename : "(null)");
    LOG_INFO("numberOfRounds: %lu", numberOfRounds);
    LOG_INFO("maximumPrimeIndex: %lu", maximumPrimeIndex);
    LOG_INFO("numberOfBits: %d", numberOfBits);
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

static int getFileSize64(const char* path, unsigned long long* outSize)
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
#if defined(_WIN32)
    if (st.st_size < 0)
    {
        return -1;
    }
#endif
    *outSize = (unsigned long long)st.st_size;
    return 0;
}
