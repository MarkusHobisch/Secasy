#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
unsigned long numberOfRounds;
int hashLengthInBits;
int main(void) {
    numberOfRounds = 10;
    hashLengthInBits = 512;
    char *hashes[256];
    for (int i = 0; i < 256; i++) {
        uint8_t input[16];
        memset(input, (uint8_t)i, 16);
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(input, 16);
        hashes[i] = calculateHashValue();
    }
    for (int i = 0; i < 256; i++)
        for (int j = i+1; j < 256; j++)
            if (strcmp(hashes[i], hashes[j]) == 0)
                printf("COLLISION: byte 0x%02X and 0x%02X -> %s\n", i, j, hashes[i]);
    printf("Done.\n");
    for (int i = 0; i < 256; i++) free(hashes[i]);
    return 0;
}
