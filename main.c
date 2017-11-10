#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "BlockOne.h"

char input_filename_[FILENAME_LEN];
unsigned long rounds_ = 10000;
unsigned long prime_index_ = 16000000;

int main(int argc, char **argv)
{
    if (argc < 2) // no arguments were passed
    {
        printf("No input file found!");
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (i == 1) // the first passed argument: file name
        {
            size_t inputSize = strlen(argv[i]) + 1;
            if (inputSize > 100)
            {
                printf("Error: file size is too big! [1..100]\n");
                return -1;
            }
            strncpy(input_filename_, argv[i], 100);
        }

        if (i == 2) // the second passed argument: amount of rounds
        {
            char *succ;
            long rounds = strtol(argv[i], &succ, 10);
            if (rounds <= 0)
            {
                printf("rounds <= 0 is not allowed!\n");
                return -1;
            }
            rounds_ = rounds;
        }

        if (i == 3) // the third passed argument: the number of generated primes
        {
            char *succ;
            long primes = strtol(argv[i], &succ, 10);
            if (primes <= 0)
            {
                printf("prime index <= 0 is not allowed!\n");
                return -1;
            }
            prime_index_ = primes;
        }
    }
    printf("input_filename_: %s\n", input_filename_);
    printf("rounds_: %lu\n", rounds_);
    printf("prime_index_: %lu\n", prime_index_);

    // Todo some calculations...
    generateField();
    // printAllPrimes();
}

long convertToMB(long bytes)
{
    return bytes / (1024 * 1024);
}

