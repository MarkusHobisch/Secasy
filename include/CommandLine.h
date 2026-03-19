#ifndef SECASY_COMMANDLINE_H
#define SECASY_COMMANDLINE_H

#include <stddef.h>

typedef struct
{
    unsigned long numberOfRounds;
    unsigned long maximumPrimeIndex;
    int hashLengthInBits;
    char *inputFilename;
    char *inputString;
    unsigned char *inputHexBytes;
    size_t inputHexLen;
} CommandLineOptions_t;

CommandLineOptions_t parseCommandLineOptions(int argc, char **argv);
void printCommandLineOptions(const CommandLineOptions_t *opts);
void printStatistics(double cpuSeconds, double wallSeconds, unsigned long long fileSizeBytes);
int getFileSize64(const char *path, unsigned long long *outSize);

#endif
