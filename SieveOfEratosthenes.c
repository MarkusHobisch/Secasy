//
// Created by Markus on 10.11.2017.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int primesLength = 0;
int *primeSieve = NULL;
int *primes = NULL;

// using Sieve of Eratosthenes
// see https://de.wikipedia.org/wiki/Sieb_des_Eratosthenes
int *generatePrimeNumbers(unsigned int n)
{
    // initially assume all integers are prime
    if (!primeSieve)
    {
        primeSieve = calloc(n + 1, sizeof(int));
        _STATIC_ASSERT(primeSieve != NULL);
    }

    for (int i = 2; i <= n; i++)
    {
        primeSieve[i] = true;
    }

    for (int i = 2; i * i <= n; i++)
    {
        if (primeSieve[i])
            for (int j = i; i * j <= n; j++)
            {
                primeSieve[i * j] = false;
            }
    }

    // optimization
    unsigned int size = n;
    if (n > 10000)
        size = n / 4;
    if (n >= 10000000)
        size = n / 14;
    if (n >= 100000000)
        size = n / 17;

    if (!primes)
    {
        primes = calloc(size, sizeof(int));
        _STATIC_ASSERT(primes != NULL);
    }

    for (int j = 2; j < (n + 1); j++)
    {
        if (primeSieve[j])
            primes[primesLength++] = j;
    }

    printf("The number of primes <= %d is %d\n", n, primesLength);
    return primes;
}

void printAllPrimes()
{
    if (!primes)
    {
        printf("No primes found!\n");
        return;
    }

    for (int i = 0; i < primesLength; i++)
    {
        if (primes[i] != 0)
            printf("%d\n", primes[i]);
    }
    printf("\n");
}