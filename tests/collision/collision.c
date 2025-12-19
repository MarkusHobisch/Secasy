/*
 * Secasy Collision / Distribution Harness (Experimental)
 * -----------------------------------------------------
 * Purpose:
 *   Empirically probe early-stage collision behavior and output distribution
 *   characteristics of the Secasy hash (hex representation) under random
 *   uniformly generated messages.
 *
 * What it tests:
 *   - Collision counts for full or truncated hashes (birthday expectation)
 *   - Global hex digit frequency uniformity (Chi^2)
 *   - Positional hex digit uniformity (per-index Chi^2)
 *   - Leading byte distribution (256-way frequency + Chi^2)
 *   - Truncation sweep (-X): multiple effective bit widths without re-hashing
 *
 * Security intent:
 *   These measurements provide an initial sanity check for gross bias,
 *   malformed finalization, or structural hotspots. Passing does NOT prove
 *   collision resistance; it only indicates absence of obvious low-order
 *   distribution defects for the tested sample sizes.
 *
 * Not covered:
 *   - Cryptographic collision bound proofs
 *   - Adversarial / structured inputs
 *   - Differential / linear analysis
 *   - Near-collision clustering or multicollision strategies
 *
 * Usage focus:
 *   Use truncated spaces (e.g. 16–36 bits) to observe collisions and compare
 *   with expected m(m-1)/(2·2^k). Large full-width collisions are infeasible
 *   to witness directly at modest sample sizes.
 *
 * Status:
 *   Experimental research tool; results are advisory only. The core hash
 *   must still undergo deeper cryptanalytic scrutiny before any real-world
 *   security claims.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

/* wall_time_seconds expected from util.c */

/* Simple collision test harness.
 * Generates N random messages of length L and hashes them.
 * Uses a closed-addressing hash table (open addressing linear probing) to detect duplicates.
 */

#ifndef COLLISION_DEFAULT_MESSAGES
#define COLLISION_DEFAULT_MESSAGES 5000
#endif
#ifndef COLLISION_DEFAULT_LEN
#define COLLISION_DEFAULT_LEN 64
#endif

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS; /* imported by core */
int hashLengthInBits = DEFAULT_BIT_SIZE;                     /* internal buffer size */

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;
static uint64_t rng_next(void){ uint64_t x=rng_state; x ^= x>>12; x ^= x<<25; x ^= x>>27; rng_state = x; return x * 0x2545F4914F6CDD1DULL; }
static void rng_seed(uint64_t s){ if(s==0) s=(uint64_t)time(NULL)*0x9e3779b97f4a7c15ULL; rng_state = s; }
static uint32_t rng_u32(void){ return (uint32_t)(rng_next()>>32); }

static void random_buffer(unsigned char* buf, size_t len){ for(size_t i=0;i<len;i++){ buf[i] = (unsigned char)(rng_u32() & 0xFF); } }

/* Very compact hash (FNV-1a style) for the hex hash string to put into table */
static uint64_t hash_hex(const char* s){ uint64_t h=1469598103934665603ULL; while(*s){ h ^= (unsigned char)*s++; h *= 1099511628211ULL; } return h; }

typedef struct Entry { uint64_t key; char* hex; } Entry;

static Entry* table = NULL; static size_t tableCap = 0; static size_t tableCount = 0;

static int table_init(size_t cap){ tableCap = 1; while(tableCap < cap*2) tableCap <<= 1; table = (Entry*)calloc(tableCap, sizeof(Entry)); return table ? 0 : -1; }

/* Provide local strdup fallback if util.h does not offer secasy_strdup */
#ifndef HAVE_SECASY_STRDUP
static char* local_strdup(const char* s){
    size_t len = strlen(s)+1;
    char* d = (char*)malloc(len);
    if(d) memcpy(d,s,len);
    return d;
}
#define secasy_strdup local_strdup
#endif

static int table_insert_or_collision(const char* hex){
    uint64_t k = hash_hex(hex);
    size_t mask = tableCap - 1;
    size_t idx = (size_t)k & mask;
    while(1){
        if(table[idx].hex == NULL){
            table[idx].key = k;
            table[idx].hex = secasy_strdup(hex);
            if(!table[idx].hex){ fprintf(stderr,"OOM storing hash string\n"); exit(EXIT_FAILURE);}            
            tableCount++;
            return 0; /* new */
        }
        if(table[idx].key == k && strcmp(table[idx].hex, hex) == 0){
            return 1; /* collision (same hash string already seen) */
        }
        idx = (idx + 1) & mask;
    }
}

static void usage(const char* prog){
    fprintf(stderr, "Usage: %s [-m messages] [-l lenBytes] [-r rounds] [-n hashBufBits] [-s seed] [-T truncBits] [-F] [-P] [-p pos] [-B nBytes] [-X list]\n", prog);
    fprintf(stderr, "  -T truncBits   : Use only the first <truncBits> bits of the hex hash for collisions (<=256 sensible)\n");
    fprintf(stderr, "  -F             : Output global hex symbol frequencies + Chi^2 (always on full hash)\n");
    fprintf(stderr, "  -P             : Positional hex frequency + Chi^2 per position (slow for large message counts)\n");
    fprintf(stderr, "  -p pos         : Detailed single hex position analysis (0-based)\n");
    fprintf(stderr, "  -B nBytes      : Leading byte frequency (first n bytes, 256 classes) + Chi^2\n");
    fprintf(stderr, "  -X list        : Sweep of multiple truncation bit sizes (comma separated, e.g. 16,20,24,28,32)\n");
}

int main(int argc, char** argv){
    size_t messages = COLLISION_DEFAULT_MESSAGES;
    size_t lenBytes = COLLISION_DEFAULT_LEN;
    uint64_t seed = 0;
    int truncBits = -1; /* -1 = no truncation */
    /* Sweep mode */
    int sweepBits[64];
    int sweepCount = 0; /* >0 means multiple truncation sizes */
    int doFreq = 0;
    int doPos = 0;
    long detailPos = -1;
    long byteAnalyze = 0; /* number of leading bytes to analyze */
    uint64_t hexFreq[16];
    memset(hexFreq, 0, sizeof(hexFreq));

    /* Dynamic matrix for positional analysis (grow to max observed hash length) */
    size_t observedMaxHexLen = 0; /* set after first hash */
    uint64_t* posFreq = NULL;     /* layout: positions * 16 */
    /* Byte analysis (leading bytes) */
    static uint64_t byteFreq[256];
    memset(byteFreq, 0, sizeof(byteFreq));

    int optIndex = 1;
    while(optIndex < argc){
        if(strcmp(argv[optIndex],"-m")==0 && optIndex+1 < argc){ messages = (size_t)strtoull(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-l")==0 && optIndex+1 < argc){ lenBytes = (size_t)strtoull(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-r")==0 && optIndex+1 < argc){ numberOfRounds = strtoul(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-n")==0 && optIndex+1 < argc){ hashLengthInBits = (int)strtoul(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-s")==0 && optIndex+1 < argc){ seed = strtoull(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-T")==0 && optIndex+1 < argc){ truncBits = (int)strtol(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-X")==0 && optIndex+1 < argc){
            char* list = argv[++optIndex];
            /* duplicate since strtok modifies input */
            char* dup = secasy_strdup(list);
            if(!dup){ fprintf(stderr,"OOM parsing -X\n"); return 1; }
            char* tok = strtok(dup, ",");
            while(tok && sweepCount < (int)(sizeof(sweepBits)/sizeof(sweepBits[0]))){
                int b = (int)strtol(tok,NULL,10);
                if(b > 0 && b <= 256){ sweepBits[sweepCount++] = b; }
                tok = strtok(NULL, ",");
            }
            free(dup);
        }
        else if(strcmp(argv[optIndex],"-F")==0){ doFreq = 1; }
        else if(strcmp(argv[optIndex],"-P")==0){ doPos = 1; }
        else if(strcmp(argv[optIndex],"-p")==0 && optIndex+1 < argc){ detailPos = strtol(argv[++optIndex],NULL,10); }
        else if(strcmp(argv[optIndex],"-B")==0 && optIndex+1 < argc){ byteAnalyze = strtol(argv[++optIndex],NULL,10); }
        else { usage(argv[0]); return 1; }
        optIndex++;
    }

    if(messages == 0 || lenBytes == 0){ fprintf(stderr,"Invalid zero parameter\n"); return 1; }

    if(sweepCount>0 && truncBits > 0){
        fprintf(stderr,"Note: -X provided; single -T value (%d) ignored.\n", truncBits);
        truncBits = -1;
    }

    rng_seed(seed);
    int sweepMode = (sweepCount > 0);
    if(!sweepMode){
        if(table_init(messages) != 0){ fprintf(stderr,"Failed to init table\n"); return 1; }
    }

    unsigned char* buf = (unsigned char*)malloc(lenBytes);
    if(!buf){ fprintf(stderr,"OOM buffer\n"); return 1; }

    /* If sweep enabled: store all hashes */
    char** allHashes = NULL;
    if(sweepMode){
        allHashes = (char**)calloc(messages, sizeof(char*));
        if(!allHashes){ fprintf(stderr,"OOM allHashes\n"); return 1; }
    }

    size_t collisions = 0; /* only used in non-sweep mode */
    double start = wall_time_seconds();
    for(size_t m=0; m<messages; ++m){
        random_buffer(buf, lenBytes);
        initFieldWithDefaultNumbers(DEFAULT_MAX_PRIME_INDEX);
        processBuffer(buf, lenBytes);
        char* hv = calculateHashValue();
        if(!hv){ fprintf(stderr,"hash failed\n"); free(buf); return 1; }
        if(doFreq || doPos || detailPos >=0 || byteAnalyze>0){
            size_t len = strlen(hv);
            if((doPos || detailPos>=0) && observedMaxHexLen < len){
                /* Reallocation: create new matrix and copy old rows */
                uint64_t* newMatrix = (uint64_t*)calloc(len * 16, sizeof(uint64_t));
                if(!newMatrix){ fprintf(stderr,"OOM posFreq matrix\n"); exit(EXIT_FAILURE);}                
                if(posFreq){
                    /* copy old rows */
                    for(size_t p=0;p<observedMaxHexLen;p++){
                        memcpy(newMatrix + p*16, posFreq + p*16, 16*sizeof(uint64_t));
                    }
                    free(posFreq);
                }
                posFreq = newMatrix;
                observedMaxHexLen = len;
            }
        }
        if(doFreq){
            for(const char* p = hv; *p; ++p){
                char c = *p;
                int v;
                if(c>='0' && c<='9') v = c-'0';
                else if(c>='a' && c<='f') v = 10 + (c-'a');
                else if(c>='A' && c<='F') v = 10 + (c-'A');
                else continue; /* ignore */
                hexFreq[v]++;
            }
        }
        if(doPos || detailPos>=0){
            for(size_t pos=0; hv[pos]; ++pos){
                char c = hv[pos];
                int v;
                if(c>='0' && c<='9') v = c-'0';
                else if(c>='a' && c<='f') v = 10 + (c-'a');
                else if(c>='A' && c<='F') v = 10 + (c-'A');
                else continue;
                size_t offset = pos*16u + (unsigned)v;
                posFreq[offset]++;
            }
        }
        if(byteAnalyze>0){
            if(byteAnalyze > 64) byteAnalyze = 64; /* safety cap */
            size_t hexNeeded = (size_t)byteAnalyze * 2u;
            size_t hlen = strlen(hv);
            if(hlen >= hexNeeded){
                for(long b=0; b < byteAnalyze; ++b){
                    size_t hiIndex = (size_t)b*2u;
                    size_t loIndex = hiIndex + 1u;
                    if(loIndex >= hlen) break;
                    int hiChar = hv[hiIndex];
                    int loChar = hv[loIndex];
                    int hi = -1, lo = -1;
                    if(hiChar>='0' && hiChar<='9') hi = hiChar - '0'; else if(hiChar>='a'&&hiChar<='f') hi = 10 + (hiChar-'a'); else if(hiChar>='A'&&hiChar<='F') hi = 10 + (hiChar-'A');
                    if(loChar>='0' && loChar<='9') lo = loChar - '0'; else if(loChar>='a'&&loChar<='f') lo = 10 + (loChar-'a'); else if(loChar>='A'&&loChar<='F') lo = 10 + (loChar-'A');
                    if(hi<0 || lo<0) continue;
                    unsigned val = (unsigned)((hi<<4) | lo) & 0xFFu;
                    byteFreq[val]++;
                }
            }
        }
        if(sweepMode){
            allHashes[m] = hv;
        } else {
            if(truncBits > 0){
                int neededHex = (truncBits + 3) / 4; 
                size_t len = strlen(hv);
                if((int)len > neededHex){
                    char saved = hv[neededHex];
                    hv[neededHex] = '\0';
                    int col = table_insert_or_collision(hv);
                    hv[neededHex] = saved; 
                    if(col) collisions++;
                } else {
                    if(table_insert_or_collision(hv)) collisions++;
                }
            } else {
                if(table_insert_or_collision(hv)) collisions++;
            }
            free(hv);
        }
    }
    double elapsed = wall_time_seconds() - start;
    if(!sweepMode){
        printf("Collision test complete\n");
        printf("Messages: %zu\n", messages);
        printf("Length (bytes): %zu\n", lenBytes);
        printf("Rounds: %lu  HashBitsParam: %d\n", (unsigned long)numberOfRounds, hashLengthInBits);
        printf("Unique hashes: %zu\n", tableCount);
    if(truncBits > 0) printf("(Truncation active: %d bits)\n", truncBits);
        printf("Collisions: %zu\n", collisions);
        double collisionRate = messages? (double)collisions / (double)messages : 0.0;
        printf("Collision rate: %.8f\n", collisionRate);
        printf("Elapsed: %.3f s (%.2f msg/s)\n", elapsed, elapsed>0? (double)messages/elapsed:0.0);

        double kbits = (double)(truncBits > 0 ? truncBits : hashLengthInBits);
        double space = pow(2.0, kbits);
        if(kbits <= 60.0){
            #ifndef M_PI
            #define M_PI 3.14159265358979323846
            #endif
            double birthdayApprox = sqrt(M_PI*space/2.0);
            double expectedColl = ((double)messages * (double)(messages-1)) / (2.0 * space);
            double pApprox = expectedColl; /* expected number of collisions */
            printf("Approx space: 2^%.0f  birthday threshold ~%.0f trials  expected collisions ~%.6g\n", kbits, birthdayApprox, pApprox);
        } else {
            printf("(Skipping analytical approximation: kbits=%.0f too large for double pow accuracy)\n", kbits);
        }
    } else {
        printf("Sweep Generation complete: %zu messages hashed in %.3f s (%.2f msg/s)\n", messages, elapsed, elapsed>0? (double)messages/elapsed:0.0);
    printf("Sweep results (messages=%zu lenBytes=%zu rounds=%lu hashParamBits=%d):\n", messages, lenBytes, (unsigned long)numberOfRounds, hashLengthInBits);
        for(int si=0; si<sweepCount; ++si){
            int bits = sweepBits[si];
            /* init neue Tabelle */
            if(table){
                for(size_t i=0;i<tableCap;i++){ free(table[i].hex); }
                free(table); table=NULL; tableCap=0; tableCount=0;
            }
            if(table_init(messages) != 0){ fprintf(stderr,"Failed to init table (sweep)\n"); return 1; }
            size_t localColl = 0;
            for(size_t m=0;m<messages;++m){
                char* hv = allHashes[m];
                int neededHex = (bits + 3)/4;
                size_t len = strlen(hv);
                if((int)len > neededHex){
                    char saved = hv[neededHex];
                    hv[neededHex] = '\0';
                    int col = table_insert_or_collision(hv);
                    hv[neededHex] = saved;
                    if(col) localColl++;
                } else {
                    if(table_insert_or_collision(hv)) localColl++;
                }
            }
            double rate = messages? (double)localColl / (double)messages : 0.0;
            double kbits = (double)bits;
            double expectedColl = 0.0; double birthdayApprox = 0.0; int approxUsed = 0;
            if(kbits <= 60.0){
                double space = pow(2.0, kbits);
                birthdayApprox = sqrt(M_PI*space/2.0);
                expectedColl = ((double)messages * (double)(messages-1)) / (2.0 * space);
                approxUsed = 1;
            }
            if(approxUsed){
                printf("  Bits=%3d  Collisions=%-8zu Unique=%-8zu Rate=%.8f  Expected~%.2f  Birthday~%.0f\n", bits, localColl, tableCount, rate, expectedColl, birthdayApprox);
            } else {
                printf("  Bits=%3d  Collisions=%-8zu Unique=%-8zu Rate=%.8f  (Approx skipped)\n", bits, localColl, tableCount, rate);
            }
        }
    }

    free(buf);
    if(sweepMode){
        for(size_t m=0;m<messages;m++) free(allHashes[m]);
        free(allHashes);
    }
    if(doFreq){
    printf("Hex frequencies (0-f):\n");
        unsigned long long total = 0ULL;
        for(int i=0;i<16;i++) total += hexFreq[i];
        if(total > 0){
            long double expected = (long double)total / 16.0L;
            long double chi2 = 0.0L;
            for(int i=0;i<16;i++){
                long double diff = (long double)hexFreq[i] - expected;
                chi2 += (diff*diff)/expected;
            }
            for(int i=0;i<16;i++){
                long double pct = 100.0L * (long double)hexFreq[i] / (long double)total;
                printf("  %X : %12llu  (%6.2Lf%%)\n", i, (unsigned long long)hexFreq[i], pct);
            }
            /* df = 15 for 16 classes */
            printf("Chi^2 = %.3Lf  (df=15)  Note: p-value lookup external (R, tables)\n", chi2);
        }
    }
    if(doPos && observedMaxHexLen>0){
    printf("Positional analysis (each position separately, Chi^2 per position):\n");
        for(size_t pos=0; pos<observedMaxHexLen; ++pos){
            uint64_t rowTotal = 0ULL;
            for(int s=0;s<16;s++){
                size_t idx = pos*16u + (unsigned)s;
                rowTotal += posFreq[idx];
            }
            if(rowTotal == 0ULL) continue;
            long double expected = (long double)rowTotal / 16.0L;
            long double chi2p = 0.0L;
            for(int s=0;s<16;s++){
                size_t idx = pos*16u + (unsigned)s;
                long double diff = (long double)posFreq[idx] - expected;
                chi2p += (diff*diff)/expected;
            }
            printf("  Pos %03zu: Chi^2=%.3Lf  (df=15)\n", pos, chi2p);
        }
    }
    if(detailPos >=0){
        if(observedMaxHexLen==0 || posFreq==NULL || (size_t)detailPos >= observedMaxHexLen){
            printf("Detail position %ld: No data (hash length < pos or positional stats disabled)\n", detailPos);
        } else {
        printf("Detail position %ld:\n", detailPos);
        uint64_t rowTotal = 0ULL;
        size_t dpos = (size_t)detailPos;
        for(int s=0;s<16;s++) rowTotal += posFreq[dpos*16u + (unsigned)s];
        if(rowTotal>0){
            long double expected = (long double)rowTotal / 16.0L;
            long double chi2p = 0.0L;
            for(int s=0;s<16;s++){
                uint64_t c = posFreq[dpos*16u + (unsigned)s];
                long double diff = (long double)c - expected;
                chi2p += (diff*diff)/expected;
            }
            printf("  Total Nibbles: %llu  Chi^2=%.3Lf\n", (unsigned long long)rowTotal, chi2p);
            printf("  Symbol  Count        %%       Z\n");
            for(int s=0;s<16;s++){
                uint64_t c = posFreq[dpos*16u + (unsigned)s];
                long double pct = 100.0L * (long double)c / (long double)rowTotal;
                long double diff = (long double)c - expected;
                long double z = diff / sqrtl(expected); /* rough normal approximation */
                printf("    %X  %10llu  %6.2Lf%%  %7.3Lf\n", s, (unsigned long long)c, pct, z);
            }
        }
        }
    }
    if(byteAnalyze>0){
    printf("Byte analysis of first %ld bytes (256 classes):\n", byteAnalyze);
        unsigned long long totalB = 0ULL;
        for(int i=0;i<256;i++) totalB += byteFreq[i];
        if(totalB>0){
            long double expectedB = (long double)totalB / 256.0L;
            long double chi2b = 0.0L;
            for(int i=0;i<256;i++){
                long double diff = (long double)byteFreq[i] - expectedB;
                chi2b += (diff*diff)/expectedB;
            }
            printf("  Total samples: %llu  Chi^2=%.3Lf (df=255)\n", (unsigned long long)totalB, chi2b);
            /* Show top absolute deviations */
            int show = 8;
            /* Simple selection: linear scan for largest absolute deviations */
            long double bestDev[8]; int bestIdx[8];
            for(int k=0;k<8;k++){ bestDev[k]=-1.0L; bestIdx[k]=-1; }
            for(int i=0;i<256;i++){
                long double diff = fabsl((long double)byteFreq[i]-expectedB);
                /* Largest absolute deviation */
                for(int k=0;k<show;k++){
                    if(diff > bestDev[k]){ for(int s=show-1;s>k;s--){ bestDev[s]=bestDev[s-1]; bestIdx[s]=bestIdx[s-1]; } bestDev[k]=diff; bestIdx[k]=i; break; }
                }
                /* Small near-expected deviations ignored */
            }
            printf("  Largest deviations (Top %d):\n", show);
            for(int k=0;k<show;k++) if(bestIdx[k]>=0){
                int i = bestIdx[k];
                long double pct = 100.0L * (long double)byteFreq[i] / (long double)totalB;
                long double z = ((long double)byteFreq[i]-expectedB)/sqrtl(expectedB);
                printf("    0x%02X  count=%8llu  %6.3Lf%%  Z=%7.3Lf\n", i, (unsigned long long)byteFreq[i], pct, z);
            }
        }
    }
    free(posFreq);
    /* free table */
    for(size_t i=0;i<tableCap;i++){ free(table[i].hex); }
    free(table);
    return 0;
}
