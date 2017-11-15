#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "BlockOne.h"

char input_filename_[FILENAME_LEN];
unsigned long rounds_ = 10000;
unsigned long prime_index_ = 16000000;
int bit_size_ = 64;
char *hashValue;

void printDatatypeMaxValues();

unsigned int getFileSize();

int main(int argc, char **argv)
{
    clock_t tStart = clock();

    int opt;
    while ((opt = getopt(argc, argv, "r:i:b:")) != -1)
    {
        switch (opt)
        {
            case 'r':
            {
                char *end_ptr = NULL;
                long rounds = strtol(optarg, &end_ptr, 10);
                if (rounds <= 0)
                {
                    printf("rounds <= 0 is not allowed!\n");
                    return -1;
                }
                rounds_ = (unsigned long) rounds;
                break;
            }
            case 'i':
            {
                char *end_ptr;
                long primes = strtol(optarg, &end_ptr, 10);
                if (primes <= 0)
                {
                    printf("prime index <= 0 is not allowed!\n");
                    return -1;
                }
                prime_index_ = (unsigned long) primes;
                break;
            }
            case 'b':
            {
                char *end_ptr;
                long bit_size = strtol(optarg, &end_ptr, 10);
                if (bit_size < 64)
                {
                    printf("Bit size small than 64 is not supported!\n");
                    return -1;
                } else if ((bit_size & (bit_size - 1)) != 0)
                {
                    printf("Bit size must be the power of two!\n");
                    return -1;
                }
                bit_size_ = bit_size;
                break;
            }
            default:
                fprintf(stderr, "Usage: %s [-ilw] [file...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    size_t input_len = strlen(argv[argc - 1]) + 1;
    if (input_len > 100)
    {
        printf("Error: filename is too long! [1..100 signs]\n");
        return -1;
    }
    strncpy(input_filename_, argv[argc-1], 100);

    printf("input_filename_: %s\n", input_filename_);
    printf("rounds_: %lu\n", rounds_);
    printf("prime_index_: %ld\n", prime_index_);
    printf("bit_size_: %ld\n", bit_size_);

    // Todo some calculations...
    generateField();
    // printAllPrimes();
    //printField();
    //printColorIndexes();
    //printSumsAndValues();
    hashValue = meltingPot();
    //printField();

    printf("\n\nHASH VALUE: %s \n", hashValue);

    double time_diff = (double) (clock() - tStart) / CLOCKS_PER_SEC;
    double hash_rate = (double) getFileSize() / time_diff / (1024 * 1024);
    printf("\n\nTotal time: %.2f seconds \n", time_diff);
    printf("Hash rate: %.2f MB/s\n", hash_rate);
    printf("File size: %.2f MB\n", (double) getFileSize() / (1024 * 1024));
    // printDatatypeMaxValues();
}

unsigned int getFileSize()
{
    FILE *fp = fopen(input_filename_, "rb");
    fseek(fp, 0L, SEEK_END);
    return ftell(fp);
}

void printDatatypeMaxValues()
{
    printf("\n\n**************/////// MAX VALUES OF DATATYPES ///////**************\n");
    printf("+ LONG_MAX                 %ld\n", LONG_MAX);
    printf("+ INT_MAX                  %ld\n", INT_MAX);
    printf("+ LONG_LONG_MAX            %llu\n", LLONG_MAX);
    printf("+ I64_MAX                  %llu\n", _I64_MAX);
    printf("+ UNSIGNED_LONG_LONG_MAX   %llu\n", ULONG_LONG_MAX);
    printf("*******************************************************************\n\n");
}

