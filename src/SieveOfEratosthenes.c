#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include "Defines.h"
#include "SieveOfEratosthenes.h"
#include "util.h"

// using Sieve of Eratosthenes
// see https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes

static int *initPrimeSieve(unsigned int maxPrimeIndex);

static void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve);

static int *getAllPrimes(int *numberOfPrimes, unsigned int maxPrimeIndex, const int *primeSieve);

static void printAllPrimes(int numberOfPrimes, const int *primes);

int *generatePrimeNumbers(int *numberOfPrimes, const unsigned long maxPrimeIndex)
{
    LOG_INFO("generating primes up to %lu", maxPrimeIndex);
    if (!numberOfPrimes)
    {
        LOG_ERROR("numberOfPrimes pointer is NULL");
        return NULL;
    }

    if (maxPrimeIndex < 2UL)
    {
        *numberOfPrimes = 0;
        return NULL; // no primes below 2
    }

    if (maxPrimeIndex > MAX_ALLOWED_PRIME_INDEX)
    {
        LOG_ERROR("maxPrimeIndex %lu exceeds practical limit of %lu", maxPrimeIndex, (unsigned long)MAX_ALLOWED_PRIME_INDEX);
        *numberOfPrimes = 0;
        return NULL;
    }

    if (maxPrimeIndex > UINT_MAX)
    {
        LOG_ERROR("maxPrimeIndex %lu exceeds supported 32-bit sieve limit", maxPrimeIndex);
        *numberOfPrimes = 0;
        return NULL;
    }

    int *primeSieve = initPrimeSieve(maxPrimeIndex);
    if (!primeSieve)
    {
        *numberOfPrimes = 0;
        return NULL;
    }

    crossOutMultiples(maxPrimeIndex, primeSieve);
    int *primes = getAllPrimes(numberOfPrimes, maxPrimeIndex, primeSieve);

    if (!primes)
    {
        LOG_ERROR("Failed to collect primes");
        free(primeSieve);
        *numberOfPrimes = 0;
        return NULL;
    }

    printf("Number of primes <= %lu is %d\n", maxPrimeIndex, *numberOfPrimes);
    if (g_debug_mode)
        printAllPrimes(*numberOfPrimes, primes);

    free(primeSieve);
    return primes;
}

static int *initPrimeSieve(const unsigned int maxPrimeIndex)
{
    // overflow guard: ensure (maxPrimeIndex+1)*sizeof(int) fits in size_t
    if (maxPrimeIndex >= (UINT_MAX - 1U))
    {
        LOG_ERROR("maxPrimeIndex too large (%u)", maxPrimeIndex);
        return NULL;
    }

    int *primeSieve = (int *)calloc((size_t)maxPrimeIndex + 1U, sizeof(int));
    if (!primeSieve)
    {
        LOG_ERROR("Memory allocation failed for prime sieve (size=%u)", maxPrimeIndex + 1U);
        return NULL;
    }

    for (unsigned int i = 2; i <= maxPrimeIndex; i++)
    {
        primeSieve[i] = true;
    }

    return primeSieve;
}

static void crossOutMultiples(const unsigned int maxPrimeIndex, int *primeSieve)
{
    for (unsigned int i = 2; i * i <= maxPrimeIndex; ++i)
    {
        if (primeSieve[i])
        {
            unsigned int start = i * i;
            for (unsigned int j = start; j <= maxPrimeIndex; j += i)
            {
                primeSieve[j] = false;
            }
        }
    }
}

static int *getAllPrimes(int *numberOfPrimes, const unsigned int maxPrimeIndex, const int *primeSieve)
{
    int *primeNumbers = (int *)calloc(maxPrimeIndex ? maxPrimeIndex : 1, sizeof(int));
    if (!primeNumbers)
    {
        LOG_ERROR("Memory allocation failed for prime list (size=%u)", maxPrimeIndex);
        return NULL;
    }

    int primeCounter = 0;
    for (unsigned int i = 2; i < (maxPrimeIndex + 1U); i++)
    {
        if (primeSieve[i])
        {
            primeNumbers[primeCounter++] = (int)i;
        }
    }

    *numberOfPrimes = primeCounter;
    return primeNumbers;
}

static void printAllPrimes(const int numberOfPrimes, const int *primes)
{
    if (!primes)
    {
        printf("No primes found!\n");
        return;
    }

    printf("-------------- PRINT ALL PRIMES (%d) ------------\n", numberOfPrimes);
    for (int i = 0; i < numberOfPrimes; i++)
    {
        if (primes[i] != 0)
        {
            if (i % 10 == 0)
            {
                printf("\n");
            }
            printf("%d,", primes[i]);
        }
    }
    printf("\n");
}
