#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Defines.h"
#include "Printing.h"

// maRkus -> 57dc2a5605a7d4ef
// -n 64
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;

int numberOfBits = DEFAULT_BIT_SIZE;

static unsigned long maximumPrimeIndex = DEFAULT_MAX_PRIME_INDEX;

static char* inputFilename;

static void readInCommandLineOptions();

static void readAndStoreNumberOfRoundsOption();

static long getFileSize();

static void readAndStoreNumberOfMaximumPrimeIndexOption();

static void readAndStoreNumberOfBitsOption();

static void readAndStoreFilenameOption();

static void printHelperText();

static void printCommandLineOptions();

static void printStatistics(clock_t tStart);

int main(int argc, char** argv)
{
    char* hashValue;
    clock_t tStart = clock();

    readInCommandLineOptions(argc, argv);
    printCommandLineOptions();

    initFieldWithDefaultNumbers(maximumPrimeIndex);
    readAndProcessFile(inputFilename);

    // Additional information on partial results
#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField();
    printPrimeIndexes();
    printColorIndexes();
    printSumsAndValues();
#endif

    hashValue = calculateHashValue();

    // Check finally values based on calculations
#if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    printField();
    printSumsAndValues();
#endif

    printf("\n\nHASH VALUE: %s \n", hashValue);

    printStatistics(tStart);
}

static void readInCommandLineOptions(int argc, char** argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "r:i:n:f:h")) != -1)
    {
        switch (opt)
        {
            case 'r':
            {
                readAndStoreNumberOfRoundsOption();
                break;
            }
            case 'i':
            {
                readAndStoreNumberOfMaximumPrimeIndexOption();
                break;
            }
            case 'n':
            {
                readAndStoreNumberOfBitsOption();
                break;
            }
            case 'f':
            {
                readAndStoreFilenameOption();
                break;
            }
            case 'h':
            {
                printHelperText();
                exit(EXIT_SUCCESS);
            }
            default:
            {
                fprintf(stderr, "Usage: %s supported arguments [-r] [-i] [-n] [-f] [-h]. \n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void readAndStoreNumberOfRoundsOption()
{
    char* end_ptr;
    numberOfRounds = strtol(optarg, &end_ptr, 10);
    if (numberOfRounds <= 0)
    {
        printf("rounds <= 0 is not allowed.\n");
        exit(EXIT_FAILURE);
    }
}

static void readAndStoreNumberOfMaximumPrimeIndexOption()
{
    char* end_ptr;
    maximumPrimeIndex = strtol(optarg, &end_ptr, 10);
    if (maximumPrimeIndex <= 0)
    {
        printf("prime index <= 0 is not allowed.\n");
        exit(EXIT_FAILURE);
    }
}

static void readAndStoreNumberOfBitsOption()
{
    char* end_ptr;
    numberOfBits = strtol(optarg, &end_ptr, 10);
    if (numberOfBits < 64)
    {
        printf("Bit size small than 64 is not supported.\n");
        exit(EXIT_FAILURE);
    }
    else if ((numberOfBits & (numberOfBits - 1)) != 0)
    {
        printf("Bit size must be the power of two.\n");
        exit(EXIT_FAILURE);
    }
}

static void readAndStoreFilenameOption()
{
    char* path = optarg;
    unsigned long lengthOfPath = strlen(path) + 1;
    if (path == NULL || lengthOfPath <= 0)
    {
        printf("Missing file. Please specify a file. \n");
        printHelperText();
        exit(EXIT_FAILURE);
    }
    inputFilename = (char*) calloc(lengthOfPath, sizeof(char));
    strncpy(inputFilename, path, lengthOfPath);
}

static void printHelperText(){
    printf("\n");
    printf("+--------------------------------------------------------------------------------------------------------------------+\n");
    printf("| Following arguments are available: [-r] [-i] [-n] [-f] [-h]                                                        |\n");
    printf("| n: bit size of hash value. e.g. -n 1024                                                                            |\n");
    printf("| i: max prime index for calculation of prime numbers. e.g. -i 100 (25 prime numbers in the range from 1 to 100)     |\n");
    printf("| r: number of rounds during hashing step. e.g -r 1000                                                               |\n");
    printf("| f: path of filename: e.g. -f input                                                                                 |\n");
    printf("| h: shows this help text                                                                                            |\n");
    printf("+--------------------------------------------------------------------------------------------------------------------+\n\n");
}

static void printCommandLineOptions()
{
    printf("inputFilename: %s\n", inputFilename);
    printf("numberOfRounds: %lu\n", numberOfRounds);
    printf("maximumPrimeIndex: %ld\n", maximumPrimeIndex);
    printf("numberOfBits: %d\n", numberOfBits);
}

static void printStatistics(const clock_t tStart)
{
    double time_diff = (double) (clock() - tStart) / CLOCKS_PER_SEC;
    double hash_rate = (double) getFileSize() / time_diff / (1024 * 1024);
    printf("\n\nTotal time: %.2f seconds \n", time_diff);
    printf("Hash rate: %.2f MB/s\n", hash_rate);
    printf("File size: %.2f MB\n", (double) getFileSize() / (1024 * 1024));
    // printDatatypeMaxValues();
}

static long getFileSize()
{
    FILE* fp = fopen(inputFilename, "rb");
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}
