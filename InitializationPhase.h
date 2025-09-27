#ifndef SECASY_INITIALIZATIONPHASE_H
#define SECASY_INITIALIZATIONPHASE_H

#include <stddef.h> // for size_t

void initFieldWithDefaultNumbers(unsigned long maxPrimeIndex);

void readAndProcessFile(const char* filename);

void processBuffer(const unsigned char* data, size_t len);

#endif
