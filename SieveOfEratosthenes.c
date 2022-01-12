#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "InitializationPhase.h"

// using Sieve of Eratosthenes
// see https://en.wikipedia.org/wiki/Sieve_of_Eratosthenes

int numberOfPrimes = 0;

int *initPrimeSieve(unsigned int maxPrimeIndex);

void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve);

unsigned int optimizeMaxPrimeIndex(unsigned int maxPrimeIndex);

int *getAllPrimes(unsigned int maxPrimeIndex, const int *primeSieve);

void printAllPrimes(int *primes);

int *generatePrimeNumbers(unsigned int maxPrimeIndex)
{
    int *primeSieve = initPrimeSieve(maxPrimeIndex);
    crossOutMultiples(maxPrimeIndex, primeSieve);

    unsigned int optimizedMaxPrimeIndex = optimizeMaxPrimeIndex(maxPrimeIndex);

    int *primes = getAllPrimes(optimizedMaxPrimeIndex, primeSieve);

    printf("number of primes <= %d is %d\n", maxPrimeIndex, numberOfPrimes);
    if (DEBUG_MODE)
    {
        printAllPrimes(primes);
    }
    return primes;
}

int *initPrimeSieve(unsigned int maxPrimeIndex)
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

void crossOutMultiples(unsigned int maxPrimeIndex, int *primeSieve)
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

unsigned int optimizeMaxPrimeIndex(unsigned int maxPrimeIndex)
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

int *getAllPrimes(unsigned int maxPrimeIndex, const int *primeSieve)
{
    int *primes = calloc(maxPrimeIndex, sizeof(int));
    assert(primes != NULL);

    int primeCounter = 0;
    for (int j = 2; j < (maxPrimeIndex + 1); j++)
    {
        if (primeSieve[j])
            primes[primeCounter++] = j;
    }
    numberOfPrimes = primeCounter;
    return primes;
}

void printAllPrimes(int *primes)
{
    if (!primes)
    {
        printf("No primes found!\n");
        return;
    }

    for (int i = 0; i < numberOfPrimes; i++)
    {
        if (primes[i] != 0)
            printf("%d\n", primes[i]);
    }
    printf("\n");
}