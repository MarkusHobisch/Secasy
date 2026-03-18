#ifndef SECASY_INITIALIZATIONPHASE_H
#define SECASY_INITIALIZATIONPHASE_H

#include <stddef.h> // for size_t
#include <stdint.h>
#include "Defines.h"

void initFieldWithDefaultNumbers(unsigned long maxPrimeIndex);

void readAndProcessFile(const char *filename);

void processBuffer(const unsigned char *data, size_t len);

#if DEBUG_MODE
int getPathStepCount(void);
void getPathStep(int idx, uint32_t *fX, uint32_t *fY, uint32_t *tX, uint32_t *tY, int *dir);
#endif

#endif
