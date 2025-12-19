/*
 * Truncation Collision PoC for Secasy
 *
 * This demonstrates an EXPECTED birthday collision when you compare only a
 * truncated prefix of the hash (e.g. 24 bits). This is not a structural break
 * of the full hash; it is a practical demo of why short tags collide.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "../../Defines.h"
#include "../../InitializationPhase.h"
#include "../../ProcessingPhase.h"

/* Globals required by Secasy core */
unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int hashLengthInBits = DEFAULT_BIT_SIZE;

#define MAX_KEY_CHARS 128

static uint64_t fnv1a64(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void bytes_to_hex(const uint8_t *buf, size_t len, char *out, size_t outCap) {
    static const char *hex = "0123456789abcdef";
    if (outCap < len * 2 + 1) {
        if (outCap) out[0] = '\0';
        return;
    }
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex[(buf[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[buf[i] & 0xF];
    }
    out[len * 2] = '\0';
}

static char nibble_to_hex(int v) {
    v &= 0xF;
    return (char)(v < 10 ? ('0' + v) : ('a' + (v - 10)));
}

static int hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* Build a truncated key from the hex hash. If truncBits isn't a multiple of 4,
 * we keep only the HIGH bits of the final nibble and zero out the rest.
 */
static void trunc_key_from_hex(const char *hex, int truncBits, char *outKey, size_t outCap) {
    if (truncBits <= 0 || outCap == 0) {
        if (outCap) outKey[0] = '\0';
        return;
    }

    int fullNibbles = truncBits / 4;
    int remBits = truncBits % 4;
    int needNibbles = fullNibbles + (remBits ? 1 : 0);

    if ((size_t)needNibbles + 1 > outCap) {
        outKey[0] = '\0';
        return;
    }

    size_t hexLen = strlen(hex);
    if (hexLen < (size_t)needNibbles) {
        outKey[0] = '\0';
        return;
    }

    memcpy(outKey, hex, (size_t)needNibbles);
    outKey[needNibbles] = '\0';

    if (remBits) {
        int v = hex_to_nibble(outKey[needNibbles - 1]);
        int mask = 0xF << (4 - remBits);
        v &= mask;
        outKey[needNibbles - 1] = nibble_to_hex(v);
    }
}

typedef struct {
    uint8_t used;
    uint64_t h;
    char key[MAX_KEY_CHARS];
    uint8_t *msg;
    size_t msgLen;
    char *fullHash;
} Entry;

static size_t next_pow2(size_t x) {
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

static char *compute_hash(const uint8_t *msg, size_t len, int primeIndex) {
    initFieldWithDefaultNumbers(primeIndex);
    processBuffer(msg, len);
    return calculateHashValue();
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -m <messages>   Max messages to try (default: 20000)\n"
        "  -l <bytes>      Message length in bytes (default: 16)\n"
        "  -T <bits>       Truncation bits (default: 24)\n"
        "  -r <rounds>     Secasy rounds (default: %lu)\n"
        "  -i <index>      Max prime index (default: 500)\n"
        "  -n <param>      hashLengthInBits parameter (default: %d)\n"
        "  -s <seed>       RNG seed (default: time)\n",
        prog,
        (unsigned long)DEFAULT_NUMBER_OF_ROUNDS,
        DEFAULT_BIT_SIZE
    );
}

int main(int argc, char **argv) {
    size_t maxMessages = 20000;
    size_t msgLen = 16;
    int truncBits = 24;
    int primeIndex = 500;
    unsigned int seed = (unsigned int)time(NULL);

    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-m") == 0 && a + 1 < argc) {
            maxMessages = (size_t)strtoull(argv[++a], NULL, 10);
        } else if (strcmp(argv[a], "-l") == 0 && a + 1 < argc) {
            msgLen = (size_t)strtoull(argv[++a], NULL, 10);
        } else if (strcmp(argv[a], "-T") == 0 && a + 1 < argc) {
            truncBits = atoi(argv[++a]);
        } else if (strcmp(argv[a], "-r") == 0 && a + 1 < argc) {
            numberOfRounds = (unsigned long)strtoull(argv[++a], NULL, 10);
        } else if (strcmp(argv[a], "-i") == 0 && a + 1 < argc) {
            primeIndex = atoi(argv[++a]);
        } else if (strcmp(argv[a], "-n") == 0 && a + 1 < argc) {
            hashLengthInBits = atoi(argv[++a]);
        } else if (strcmp(argv[a], "-s") == 0 && a + 1 < argc) {
            seed = (unsigned int)strtoul(argv[++a], NULL, 10);
        } else if (strcmp(argv[a], "-h") == 0 || strcmp(argv[a], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[a]);
            usage(argv[0]);
            return 2;
        }
    }

    if (truncBits <= 0) {
        fprintf(stderr, "Truncation bits must be > 0\n");
        return 2;
    }

    srand(seed);

    printf("==============================================\n");
    printf("Secasy Truncation Collision PoC (birthday)\n");
    printf("==============================================\n");
    printf("This PoC finds a collision on a truncated prefix (EXPECTED).\n");
    printf("Config: messages=%zu len=%zu truncBits=%d rounds=%lu primeIndex=%d numberOfBitsParam=%d seed=%u\n",
           maxMessages, msgLen, truncBits, numberOfRounds, primeIndex, hashLengthInBits, seed);

    size_t tableSize = next_pow2(maxMessages * 2);
    Entry *table = (Entry *)calloc(tableSize, sizeof(Entry));
    if (!table) {
        fprintf(stderr, "OOM allocating table (%zu entries)\n", tableSize);
        return 1;
    }

    uint8_t *msg = (uint8_t *)malloc(msgLen);
    if (!msg) {
        fprintf(stderr, "OOM allocating message\n");
        free(table);
        return 1;
    }

    for (size_t attempt = 1; attempt <= maxMessages; attempt++) {
        for (size_t i = 0; i < msgLen; i++) msg[i] = (uint8_t)(rand() & 0xFF);

        char *hashHex = compute_hash(msg, msgLen, primeIndex);
        if (!hashHex || !hashHex[0]) {
            fprintf(stderr, "Hash computation failed\n");
            free(hashHex);
            break;
        }

        char key[MAX_KEY_CHARS];
        trunc_key_from_hex(hashHex, truncBits, key, sizeof(key));
        if (!key[0]) {
            fprintf(stderr, "Truncation key failed (hashLen=%zu)\n", strlen(hashHex));
            free(hashHex);
            break;
        }

        uint64_t h = fnv1a64(key, strlen(key));
        size_t idx = (size_t)h & (tableSize - 1);

        for (;;) {
            if (!table[idx].used) {
                table[idx].used = 1;
                table[idx].h = h;
                strncpy(table[idx].key, key, sizeof(table[idx].key) - 1);
                table[idx].key[sizeof(table[idx].key) - 1] = '\0';
                table[idx].msg = (uint8_t *)malloc(msgLen);
                if (!table[idx].msg) {
                    fprintf(stderr, "OOM storing message\n");
                    free(hashHex);
                    goto cleanup;
                }
                memcpy(table[idx].msg, msg, msgLen);
                table[idx].msgLen = msgLen;
                table[idx].fullHash = hashHex;
                break;
            }

            if (table[idx].h == h && strcmp(table[idx].key, key) == 0) {
                char msg1Hex[512], msg2Hex[512];
                bytes_to_hex(table[idx].msg, table[idx].msgLen, msg1Hex, sizeof(msg1Hex));
                bytes_to_hex(msg, msgLen, msg2Hex, sizeof(msg2Hex));

                printf("\nFOUND TRUNCATED COLLISION after %zu attempts\n", attempt);
                printf("Truncated key (%d bits): %s\n", truncBits, key);
                printf("Msg A (hex): %s\n", msg1Hex);
                printf("Hash A: %s\n", table[idx].fullHash);
                printf("Msg B (hex): %s\n", msg2Hex);
                printf("Hash B: %s\n", hashHex);

                free(hashHex);
                goto cleanup;
            }

            idx = (idx + 1) & (tableSize - 1);
        }

        if ((attempt % 1000) == 0) {
            printf("... tried %zu\n", attempt);
        }
    }

    printf("\nNo collision found within %zu attempts (try increasing -m or reducing -T).\n", maxMessages);

cleanup:
    for (size_t i = 0; i < tableSize; i++) {
        if (table[i].used) {
            free(table[i].msg);
            free(table[i].fullHash);
        }
    }
    free(table);
    free(msg);
    return 0;
}
