#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Defines.h"

// using Sieve of Eratosthenes
// see https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes

static int *initPrimeSieve(unsigned int maxPrimeIndex);
static void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve);
static unsigned int optimizePrimeIndexMaxSize(unsigned int maxPrimeIndex);
static int *getAllPrimes(int numberOfPrimes, unsigned int maxPrimeIndex, const int *primeSieve);
static void printAllPrimes(int numberOfPrimes, int *primes);

int *generatePrimeNumbers(int numberOfPrimes, unsigned int maxPrimeIndex)
{
    int *primeSieve = initPrimeSieve(maxPrimeIndex);
    crossOutMultiples(maxPrimeIndex, primeSieve);

    unsigned int optimizedMaxPrimeIndex = optimizePrimeIndexMaxSize(maxPrimeIndex);
    int *primes = getAllPrimes(numberOfPrimes, optimizedMaxPrimeIndex, primeSieve);

    printf("Number of primes <= %d is %d\n", maxPrimeIndex, numberOfPrimes);
    if (DEBUG_MODE)
    {
        // printAllPrimes(numberOfPrimes, primes);
    }
    return primes;
}

static int *initPrimeSieve(unsigned int maxPrimeIndex)
{
    int *primeSieve = calloc(maxPrimeIndex + 1, sizeof(int));
    assert(primeSieve != NULL && "mem alloc failed!");

    // initially assume all integers are primes
    for (int i = 2; i <= maxPrimeIndex; i++)
    {
        primeSieve[i] = true;
    }

    return primeSieve;
}

static void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve)
{
    for (int i = 2; i * i <= maxPrimeIndex; i++)
    {
        if (primeSieve[i])
            for (int j = i; i * j <= maxPrimeIndex; j++)
            {
                primeSieve[i * j] = false;
            }
    }
}

static unsigned int optimizePrimeIndexMaxSize(unsigned int maxPrimeIndex)
{
    unsigned int size = maxPrimeIndex;
    if (maxPrimeIndex > 10000)
        size = maxPrimeIndex / 4;
    if (maxPrimeIndex >= 10000000)
        size = maxPrimeIndex / 14;
    if (maxPrimeIndex >= 100000000)
        size = maxPrimeIndex / 17;
    return size;
}

static int *getAllPrimes(int numberOfPrimes, unsigned int maxPrimeIndex, const int *primeSieve)
{
    int *primeNumbers = calloc(maxPrimeIndex, sizeof(int));
    assert(primeNumbers != NULL);

    int primeCounter = 0;
    for (int j = 2; j < (maxPrimeIndex + 1); j++)
    {
        if (primeSieve[j])
            primeNumbers[primeCounter++] = j;
    }
    numberOfPrimes = primeCounter;
    return primeNumbers;
}

static void printAllPrimes(int numberOfPrimes, int *primes)
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
            printf("\t%d", primes[i]);
    }
    printf("\n");
}
