#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "InitializationPhase.h"

// maRkus -> 69901ca8141
// -n 64 -i 100 -r 1 -f C...

char *inputFilename;
unsigned long numberOfRounds = 100000;
unsigned long maximumPrimeIndex = 16000000;
int numberOfBits = 512;
char *hashValue;

void readInCommandLineOptions();

void readAndStoreNumberOfRoundsOption();

long getFileSize();

void readAndStoreNumberOfMaximumPrimeIndexOption();

void readAndStoreNumberOfBitsOption();

void readAndStoreFilenameOption();

void printCommandLineOptions();

void printStatistics(clock_t tStart);

int main(int argc, char **argv)
{
    clock_t tStart = clock();

    readInCommandLineOptions(argc, argv);
    printCommandLineOptions();

    initFieldWithDefaultNumbers(maximumPrimeIndex);
    readAndProcessFile(inputFilename);

    // Additional information on partial results
    if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    {
        printAllPrimes();
        printField();
        printPrimeIndexes();
        printColorIndexes();
        printSumsAndValues();
    }

    hashValue = calculateHashValue();

    // Check finally values based on calculations
    if (DEBUG_MODE && DEBUG_LOG_EXTENDED)
    {
        printField();
        printSumsAndValues();
    }

    printf("\n\nHASH VALUE: %s \n", hashValue);

    printStatistics(tStart);
}

void readInCommandLineOptions(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "r:i:n:f:")) != -1)
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
            default:
                fprintf(stderr, "Usage: %s allowed arguments [-r] [-i] [-n] [-f]. \n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

void readAndStoreNumberOfRoundsOption()
{
    char *end_ptr;
    numberOfRounds = strtol(optarg, &end_ptr, 10);
    if (numberOfRounds <= 0)
    {
        printf("rounds <= 0 is not allowed.\n");
        exit(EXIT_FAILURE);
    }
}

void readAndStoreNumberOfMaximumPrimeIndexOption()
{
    char *end_ptr;
    maximumPrimeIndex = strtol(optarg, &end_ptr, 10);
    if (maximumPrimeIndex <= 0)
    {
        printf("prime index <= 0 is not allowed.\n");
        exit(EXIT_FAILURE);
    }
}

void readAndStoreNumberOfBitsOption()
{
    char *end_ptr;
    numberOfBits = strtol(optarg, &end_ptr, 10);
    if (numberOfBits < 64)
    {
        printf("Bit size small than 64 is not supported.\n");
        exit(EXIT_FAILURE);
    } else if ((numberOfBits & (numberOfBits - 1)) != 0)
    {
        printf("Bit size must be the power of two.\n");
        exit(EXIT_FAILURE);
    }
}

void readAndStoreFilenameOption()
{
    char *path = optarg;
    unsigned long lengthOfPath = strlen(path) + 1;
    if (path == NULL || lengthOfPath <= 0)
    {
        printf("Missing file. Please specify a file. \n");
        fprintf(stderr, "Usage: allowed arguments [-r] [-i] [-n] [-f].\n");
        exit(EXIT_FAILURE);
    }
    inputFilename = (char *) calloc(lengthOfPath, sizeof(char));
    strncpy(inputFilename, path, lengthOfPath);
}

void printCommandLineOptions()
{
    printf("inputFilename: %s\n", inputFilename);
    printf("numberOfRounds: %lu\n", numberOfRounds);
    printf("maximumPrimeIndex: %ld\n", maximumPrimeIndex);
    printf("numberOfBits: %d\n", numberOfBits);
}


void printStatistics(clock_t tStart)
{
    double time_diff = (double) (clock() - tStart) / CLOCKS_PER_SEC;
    double hash_rate = (double) getFileSize() / time_diff / (1024 * 1024);
    printf("\n\nTotal time: %.2f seconds \n", time_diff);
    printf("Hash rate: %.2f MB/s\n", hash_rate);
    printf("File size: %.2f MB\n", (double) getFileSize() / (1024 * 1024));
    // printDatatypeMaxValues();
}

long getFileSize()
{
    FILE *fp = fopen(inputFilename, "rb");
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

