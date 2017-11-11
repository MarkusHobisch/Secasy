#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "BlockOne.h"
#include <glib.h>

char input_filename_[FILENAME_LEN];
unsigned long rounds_ = 10000;
unsigned long prime_index_ = 16000000;

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



    // THIS IS SOME LIST STUFF FOR TESTING
    // Notice that these are initialized to the empty list.
    GList *string_list = NULL, *number_list = NULL;

    // This is a list of strings.
    string_list = g_list_prepend(string_list, "first");
    string_list = g_list_prepend(string_list, "second");

    // This is a list of integers.
    number_list = g_list_prepend(number_list, GINT_TO_POINTER(27));
    number_list = g_list_prepend(number_list, GINT_TO_POINTER(14));
    number_list = g_list_prepend(number_list, GINT_TO_POINTER(9));

    GList *l;
    for (l = number_list; l != NULL; l = l->next) {
        // do something with l->data
        printf("%d\n", GPOINTER_TO_INT(l->data));
    }
    for (l = string_list; l != NULL; l = l->next) {
        // do something with l->data
        printf("%s\n", (char*)  l->data);
    }

    printf("length: %o\n",g_list_length(number_list));

    printf("\n\nTotal time: %d seconds", (int) (clock() - tStart) / CLOCKS_PER_SEC);
}

long convertToMB(long bytes)
{
    return bytes / (1024 * 1024);
}

