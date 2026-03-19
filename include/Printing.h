#ifndef SECASY_PRINTING_H
#define SECASY_PRINTING_H

#include "Defines.h"

void printField(const char *phase);

void printColorIndexes(void);

void printPrimeIndexes(void);

void printSumsAndValues(void);

void printDatatypeMaxValues(void);

void printInputBits(const unsigned char *data, size_t len);

void printPathMap(void);

void exportGridCSV(const char *filename, const char *phase);

#endif
