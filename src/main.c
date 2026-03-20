#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "CommandLine.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Defines.h"
#include "Printing.h"
#include "util.h"

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

int main(int argc, char **argv)
{
    secasy_enable_utf8_console();
    clock_t cpuStart = clock();
    double wallStart = wall_time_seconds();

    CommandLineOptions_t opts = parseCommandLineOptions(argc, argv);
    numberOfRounds = opts.numberOfRounds;
    hashLengthInBits = opts.hashLengthInBits;

    if (g_debug_mode)
    {
        g_debug_fp = fopen("debug.txt", "w");
        if (!g_debug_fp)
        {
            fprintf(stderr, "[WARNING] Could not open debug.txt for writing\n");
        }
    }

    printCommandLineOptions(&opts);
    initFieldWithDefaultNumbers(opts.maximumPrimeIndex);

    unsigned long long inputFileSize = 0ULL;

    if (opts.inputHexBytes)
    {
        if (g_debug_mode)
            printInputBits(opts.inputHexBytes, opts.inputHexLen);
        processBuffer(opts.inputHexBytes, opts.inputHexLen);
    }
    else if (opts.inputString)
    {
        if (g_debug_mode)
            printInputBits((const unsigned char *)opts.inputString, strlen(opts.inputString));
        processBuffer((const unsigned char *)opts.inputString, strlen(opts.inputString));
    }
    else
    {
        if (getFileSize64(opts.inputFilename, &inputFileSize) != 0)
        {
            inputFileSize = 0ULL;
        }
        if (g_debug_mode)
        {
            FILE *dbgFile = fopen(opts.inputFilename, "rb");
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
        readAndProcessFile(opts.inputFilename);
    }

    if (g_debug_mode && g_debug_extended)
    {
        printField("Init Phase");
        printPathMap();
        printPrimeIndexes();
        printColorIndexes();
        printSumsAndValues();
    }

    if (g_debug_mode)
        exportGridCSV("grid_init.csv", "Init Phase");

    char *hashValue = calculateHashValue();

    if (g_debug_mode && g_debug_extended)
    {
        printField("Processing Phase");
        printSumsAndValues();
    }

    if (g_debug_mode)
        exportGridCSV("grid_processed.csv", "Processing Phase");

    printHashValue(hashValue, hashLengthInBits);

    double cpuSeconds = (double)(clock() - cpuStart) / CLOCKS_PER_SEC;
    double wallSeconds = wall_time_seconds() - wallStart;
    printStatistics(cpuSeconds, wallSeconds, inputFileSize);

    free(hashValue);
    free(opts.inputFilename);
    free(opts.inputString);
    free(opts.inputHexBytes);

    if (g_debug_mode && g_debug_fp)
    {
        fclose(g_debug_fp);
        g_debug_fp = NULL;
        LOG_INFO("Debug output written to debug.txt");
    }

    return EXIT_SUCCESS;
}
