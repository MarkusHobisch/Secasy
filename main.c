#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "BlockOne.h"

char input_filename_[FILENAME_LEN];
unsigned long rounds_ = 10000;
unsigned long prime_index_ = 16000000;

void printDatatypeMaxValues();

int main(int argc, char **argv)
{
    clock_t tStart = clock();

    if (argc < 2) // no arguments were passed
    {
        printf("No input file found!");
        return -1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (i == 1) // the first passed argument: file name
        {
            size_t input_len = strlen(argv[i]) + 1;
            if (input_len > 100)
            {
                printf("Error: filename is too long! [1..100 signs]\n");
                return -1;
            }
            strncpy(input_filename_, argv[i], 100);
        }

        if (i == 2) // the second passed argument: amount of rounds
        {
            char *end_ptr;
            long rounds = strtol(argv[i], &end_ptr, 10);
            if (rounds <= 0)
            {
                printf("rounds <= 0 is not allowed!\n");
                return -1;
            }
            rounds_ = (unsigned long) rounds;
        }

        if (i == 3) // the third passed argument: the number of generated primes
        {
            char *end_ptr;
            long primes = strtol(argv[i], &end_ptr, 10);
            if (primes <= 0)
            {
                printf("prime index <= 0 is not allowed!\n");
                return -1;
            }
            prime_index_ = (unsigned long) primes;
        }
    }
    printf("input_filename_: %s\n", input_filename_);
    printf("rounds_: %lu\n", rounds_);
    printf("prime_index_: %ld\n", prime_index_);

    // Todo some calculations...
    generateField();
    // printAllPrimes();
    //printField();
    printColorIndexes();
    printSumsAndValues();
    meltingPot();
    printField();

    long long hashValue = getHashValue();
    printf("-------------- RESULTS --------------------------\n");
    printf("hash value (long): %llu \n", hashValue);

    printf("hash value: (hex): %llx\n", hashValue);
    /*char string[64];
    snprintf (string, 64, "hash value: (hex): %lX \n", hashValue);*/

    printf("\n\nTotal time: %.2f seconds", (double) (clock() - tStart) / CLOCKS_PER_SEC);
    // printDatatypeMaxValues();
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

