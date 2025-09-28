#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Defines.h"
#include "SieveOfEratosthenes.h"
#include "util.h"

// using Sieve of Eratosthenes
// see https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes

static int *initPrimeSieve(unsigned int maxPrimeIndex);

static void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve);

static unsigned int optimizePrimeIndexMaxSize(unsigned int maxPrimeIndex);

static int *getAllPrimes(int *numberOfPrimes, unsigned int maxPrimeIndex, const int *primeSieve);

#if DEBUG_MODE
static void printAllPrimes(int numberOfPrimes, const int *primes);
#endif

int *generatePrimeNumbers(int *numberOfPrimes, const unsigned long maxPrimeIndex)
{
    LOG_INFO("generating primes up to %lu%s", maxPrimeIndex,
#ifdef SECASY_PRIMES_FULL
             " (full range)"
#else
             " (truncated heuristic possible)"
#endif
    );
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
    if (maxPrimeIndex > UINT_MAX)
    {
        LOG_ERROR("maxPrimeIndex %lu exceeds supported 32-bit sieve limit", maxPrimeIndex);
        *numberOfPrimes = 0;
        return NULL;
    }
    unsigned int capped = (unsigned int)maxPrimeIndex;
    int *primeSieve = initPrimeSieve(capped);
    if (!primeSieve)
    {
        *numberOfPrimes = 0;
        return NULL;
    }
    crossOutMultiples(capped, primeSieve);
    unsigned int optimizedMaxPrimeIndex = optimizePrimeIndexMaxSize(capped); // NOTE: intentionally truncates range
    int *primes = getAllPrimes(numberOfPrimes, optimizedMaxPrimeIndex, primeSieve);
    if (!primes)
    {
        LOG_ERROR("Failed to collect primes");
        free(primeSieve);
        *numberOfPrimes = 0;
        return NULL;
    }
    printf("Number of primes <= %lu is %d (truncated range used: %u)\n", maxPrimeIndex, *numberOfPrimes, optimizedMaxPrimeIndex);
#if DEBUG_MODE
    printAllPrimes(*numberOfPrimes, primes);
#endif
    free(primeSieve); // avoid leak
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
    // Classic optimized sieve inner loops: start at i*i and use addition instead of multiplication
    for (unsigned int i = 2; i * i <= maxPrimeIndex; ++i)
    {
        if (primeSieve[i])
        {
            unsigned int start = i * i; // safe: i*i <= maxPrimeIndex
            for (unsigned int j = start; j <= maxPrimeIndex; j += i)
            {
                primeSieve[j] = false;
            }
        }
    }
}

static unsigned int optimizePrimeIndexMaxSize(unsigned int maxPrimeIndex)
{
#ifdef SECASY_PRIMES_FULL
    return maxPrimeIndex; // full range mode enabled via build flag
#endif
    unsigned int size = maxPrimeIndex;
    if (maxPrimeIndex > 10000)
        size = maxPrimeIndex / 4;
    if (maxPrimeIndex >= 10000000)
        size = maxPrimeIndex / 14;
    if (maxPrimeIndex >= 100000000)
        size = maxPrimeIndex / 17;
    return size;
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

#if DEBUG_MODE
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
#endif
