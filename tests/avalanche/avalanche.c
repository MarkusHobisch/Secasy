/*
 * Secasy Avalanche / Diffusion Harness (Experimental)
 * ---------------------------------------------------
 * Purpose:
 *   Measure first-order diffusion: the probability each output bit flips
 *   when a single input bit is inverted (ideal ≈ 0.5), and sanity‑check
 *   small multi-bit perturbation behavior.
 *
 * What it reports:
 *   - Global mean avalanche rate
 *   - (Extended -X) Per-bit flip frequencies (bias band detection)
 *   - (Extended -X) Multi-bit flip diffusion (k = 2,4,8) for saturation check
 *   - (Extended -X) Experimental entropy sampler (currently unreliable)
 *
 * Security intent:
 *   Strong avalanche is a necessary diffusion property for a modern hash
 *   but not sufficient to establish collision, preimage, or second-preimage
 *   resistance. Results here are only an early health indicator.
 *
 * Not covered:
 *   - Strict Avalanche Criterion (full input-bit ↔ output-bit matrix)
 *   - Higher-order correlations / linear or differential cryptanalysis
 *   - Structured / adversarial input classes
 *   - Formal statistical independence or uniformity proofs
 *
 * Usage notes:
 *   Use sampled flips (-B > 0) for speed or exhaustive (-B 0) for precision.
 *   Increase rounds to test diffusion depth scaling. Interpret entropy field
 *   cautiously until the sampler is replaced.
 *
 * Status:
 *   Research diagnostic only; passing metrics DO NOT imply production-grade
 *   cryptographic strength. Further analyses required before any claims.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <getopt.h>
#include <math.h>
#include "Defines.h"
#include "InitializationPhase.h"
#include "ProcessingPhase.h"
#include "Calculations.h"
#include "util.h"

#if defined(_MSC_VER)
  #define LD_FMT "%0.6f"
  #define PRINT_LD(x) (double)(x)
#else
  #define LD_FMT "%0.6Lf"
  #define PRINT_LD(x) (long double)(x)
#endif

unsigned long numberOfRounds = DEFAULT_NUMBER_OF_ROUNDS;
int numberOfBits = DEFAULT_BIT_SIZE;
static unsigned long maximumPrimeIndex = DEFAULT_MAX_PRIME_INDEX;

static unsigned int g_messages = 50;
static size_t g_inputLen = 64;
static unsigned int g_sampledBitFlips = 64;
static unsigned long long g_seed = 0;
static int g_flagHistogram = 0;
static int g_flagQuiet = 0;
static int g_flagExtended = 0;

static unsigned long long g_totalFlipsPerformed = 0ULL;
static unsigned long long g_totalHammingBits = 0ULL;
static unsigned long long g_totalBitsCompared = 0ULL;
static double g_sumRatios = 0.0;
static double g_sumSqRatios = 0.0;
static unsigned long long g_histBuckets[10] = {0};

static uint64_t rng_state = 0;
static void rng_seed(uint64_t s) { if (s==0) { s = 0x9e3779b97f4a7c15ULL ^ (uint64_t)time(NULL); } rng_state = s; }
static uint64_t rng_next(void) { uint64_t x = rng_state; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; rng_state = x; return x * 0x2545F4914F6CDD1DULL; }
static uint32_t rng_u32(void) { return (uint32_t)(rng_next() >> 32); }
static size_t rng_index(size_t maxExclusive) { return (size_t)(rng_next() % (uint64_t)maxExclusive); }

static unsigned long long *g_bitChanged = NULL;
static unsigned long long *g_bitCompared = NULL;
static size_t g_bitCapacity = 0;
static unsigned long long g_byteFreq[256] = {0};
static unsigned long long g_totalHistogramBytes = 0ULL;
static const int g_multiKCount = 3;
static const int g_multiKVals[3] = {2,4,8};
static unsigned long long g_multiTotalFlips[3] = {0};
static unsigned long long g_multiBitsCompared[3] = {0};
static unsigned long long g_multiHammingBits[3] = {0};

static void ensure_bit_capacity(size_t bits) {
    if (bits <= g_bitCapacity) return;
    size_t newCap = bits;
    unsigned long long *nc = (unsigned long long*)realloc(g_bitChanged, newCap * sizeof(unsigned long long));
    if (!nc) { fprintf(stderr, "OOM bitChanged realloc\n"); exit(EXIT_FAILURE); }
    unsigned long long *nr = (unsigned long long*)realloc(g_bitCompared, newCap * sizeof(unsigned long long));
    if (!nr) { fprintf(stderr, "OOM bitCompared realloc\n"); exit(EXIT_FAILURE); }
    for (size_t i=g_bitCapacity; i<newCap; ++i) { nc[i]=0; nr[i]=0; }
    g_bitChanged = nc; g_bitCompared = nr; g_bitCapacity = newCap;
}

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -m <messages>   Number of random base messages (default %u)\n"
        "  -l <lenBytes>   Length of each input message in bytes (default %zu)\n"
        "  -B <bitFlips>   Bit flips sampled per message (0=all, default %u)\n"
        "  -r <rounds>     Number of rounds for hash core (default %lu)\n"
        "  -n <hashBuf>    Hash internal buffer size (characters, default %d)\n"
        "  -i <primeIdx>   Max prime index (default %lu)\n"
        "  -s <seed>       Seed for RNG (default time-based)\n"
        "  -H              Print histogram buckets of per-flip avalanche ratios\n"
        "  -q              Quiet (omit qualitative assessment line)\n"
        "  -X              Extended analysis (per-bit bias, byte entropy, multi-bit flips)\n"
        "  -h              Help\n",
        prog, g_messages, g_inputLen, g_sampledBitFlips, numberOfRounds, numberOfBits, maximumPrimeIndex);
}

static void parse_args(int argc, char** argv) {
    int opt; while ((opt = getopt(argc, argv, "m:l:B:r:n:i:s:HqXh")) != -1) {
        switch (opt) {
            case 'm': g_messages = (unsigned)strtoul(optarg, NULL, 10); break;
            case 'l': g_inputLen = (size_t)strtoull(optarg, NULL, 10); break;
            case 'B': g_sampledBitFlips = (unsigned)strtoul(optarg, NULL, 10); break;
            case 'r': numberOfRounds = strtoul(optarg, NULL, 10); break;
            case 'n': numberOfBits = (int)strtoul(optarg, NULL, 10); break;
            case 'i': maximumPrimeIndex = strtoul(optarg, NULL, 10); break;
            case 's': g_seed = strtoull(optarg, NULL, 10); break;
            case 'H': g_flagHistogram = 1; break;
            case 'q': g_flagQuiet = 1; break;
            case 'X': g_flagExtended = 1; break;
            case 'h': usage(argv[0]); exit(EXIT_SUCCESS);
            default: usage(argv[0]); exit(EXIT_FAILURE);
        }
    }
    if (g_messages == 0 || g_inputLen == 0 || numberOfRounds == 0) { fprintf(stderr, "Invalid zero parameter\n"); exit(EXIT_FAILURE); }
    if (numberOfBits < MIN_HASH_BITS) { fprintf(stderr, "Hash buffer size < min (%d)\n", MIN_HASH_BITS); exit(EXIT_FAILURE); }
    rng_seed(g_seed);
}

static void random_buffer(unsigned char* buf, size_t len) { for (size_t i=0;i<len;i++) buf[i] = (unsigned char)(rng_u32() & 0xFF); }

static int popcount8(unsigned int x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount((unsigned)x & 0xFF);
#elif defined(_MSC_VER)
    x &= 0xFFu; x = x - ((x >> 1) & 0x55u); x = (x & 0x33u) + ((x >> 2) & 0x33u); return (int)((((x + (x >> 4)) & 0x0Fu) * 0x01u) & 0x3Fu);
#else
    int c=0; x &= 0xFFu; while (x){ c += (int)(x & 1u); x >>= 1; } return c;
#endif
}

static int hamming_bits(const unsigned char* a, const unsigned char* b, size_t bytes) {
    int d=0; for (size_t i=0;i<bytes;i++){ unsigned char x = (unsigned char)(a[i]^b[i]); d += popcount8(x); } return d; }

static size_t hex_to_bits(const char* hex, unsigned char** outBytes) {
    size_t len = strlen(hex); size_t outLen = (len + 1) / 2; unsigned char* bytes = (unsigned char*)malloc(outLen?outLen:1); if (!bytes) return 0;
    size_t i=0,j=0; if (len & 1) { char c = hex[0]; int val; if (c>='0'&&c<='9') val=c-'0'; else if (c>='a'&&c<='f') val=c-'a'+10; else if (c>='A'&&c<='F') val=c-'A'+10; else val=0; bytes[j++]=(unsigned char)val; i=1; }
    for (; i<len; i+=2) { char c1=hex[i], c2=hex[i+1]; int v1,v2; if (c1>='0'&&c1<='9') v1=c1-'0'; else if (c1>='a'&&c1<='f') v1=c1-'a'+10; else if (c1>='A'&&c1<='F') v1=c1-'A'+10; else v1=0; if (c2>='0'&&c2<='9') v2=c2-'0'; else if (c2>='a'&&c2<='f') v2=c2-'a'+10; else if (c2>='A'&&c2<='F') v2=c2-'A'+10; else v2=0; bytes[j++]=(unsigned char)((v1<<4)|v2);} *outBytes=bytes; return outLen*8; }

static void normalize_hex(const char* a, const char* b, char** outA, char** outB, size_t* outLen) {
    size_t la=strlen(a), lb=strlen(b); size_t L = la>lb?la:lb; char* na=(char*)malloc(L+1); char* nb=(char*)malloc(L+1); if(!na||!nb){fprintf(stderr,"OOM\n"); exit(EXIT_FAILURE);} memset(na,'0',L); memset(nb,'0',L); memcpy(na+(L-la),a,la); memcpy(nb+(L-lb),b,lb); na[L]='\0'; nb[L]='\0'; *outA=na; *outB=nb; *outLen=L; }

static void single_hash(const unsigned char* data, size_t len, char** outHex) { initFieldWithDefaultNumbers(maximumPrimeIndex); processBuffer(data,len); char* h = calculateHashValue(); *outHex=h; }

static void record_sample(int hd, int bitsCompared) {
    if (bitsCompared <= 0) {
        return;
    }
    double ratio = (double)hd / (double)bitsCompared;
    g_sumRatios += ratio;
    g_sumSqRatios += ratio * ratio;
    int bucket = (int)(ratio * 10.0);
    if (bucket > 9) bucket = 9;
    if (bucket < 0) bucket = 0;
    g_histBuckets[bucket]++;
}

static void extended_record_bitwise(const unsigned char* a, const unsigned char* b, size_t bitsUsed) {
    if (!g_flagExtended || bitsUsed == 0) {
        return;
    }
    ensure_bit_capacity(bitsUsed);
    size_t fullBytes = bitsUsed / 8;
    size_t remBits = bitsUsed % 8;
    size_t bitIndex = 0;
    for (size_t i = 0; i < fullBytes; i++) {
        unsigned char d = (unsigned char)(a[i] ^ b[i]);
        for (int bb = 0; bb < 8; bb++) {
            g_bitCompared[bitIndex]++;
            if (d & (1u << bb)) g_bitChanged[bitIndex]++;
            bitIndex++;
        }
    }
    if (remBits) {
        unsigned char d = (unsigned char)(a[fullBytes] ^ b[fullBytes]);
        for (size_t bb = 0; bb < remBits; bb++) {
            g_bitCompared[bitIndex]++;
            if (d & (1u << bb)) g_bitChanged[bitIndex]++;
            bitIndex++;
        }
    }
}

static void extended_record_bytes(const unsigned char* a, const unsigned char* b, size_t bytes) {
    if (!g_flagExtended) {
        return;
    }
    for (size_t i = 0; i < bytes; i++) {
        g_byteFreq[a[i]]++;
        g_byteFreq[b[i]]++;
        g_totalHistogramBytes += 2ULL;
    }
}

static void perform_multi_bit_trials(const unsigned char* base, size_t lenBytes, const char* baseHex) {
    if (!g_flagExtended) {
        return;
    }
    unsigned int totalBits = (unsigned int)(lenBytes * 8);
    const int maxTrials = 32;
    for (int ki = 0; ki < g_multiKCount; ++ki) {
        int k = g_multiKVals[ki];
        if ((int)totalBits < k) continue;
        for (int t = 0; t < maxTrials; t++) {
            unsigned char* temp = (unsigned char*)malloc(lenBytes);
            if (!temp) { fprintf(stderr, "OOM temp multi\n"); exit(EXIT_FAILURE); }
            memcpy(temp, base, lenBytes);
            for (int flips = 0; flips < k; ++flips) {
                unsigned int bitPos = (unsigned int)rng_index(totalBits);
                unsigned int byteIndex = bitPos / 8;
                unsigned int bitInByte = bitPos % 8;
                temp[byteIndex] ^= (unsigned char)(1u << bitInByte);
            }
            char* modHex = NULL;
            single_hash(temp, lenBytes, &modHex);
            if (!modHex) { free(temp); continue; }
            char *normA = NULL, *normB = NULL; size_t hexLen = 0;
            normalize_hex(baseHex, modHex, &normA, &normB, &hexLen);
            unsigned char *bytesA = NULL, *bytesB = NULL;
            size_t bitsA = hex_to_bits(normA, &bytesA);
            size_t bitsB = hex_to_bits(normB, &bytesB);
            int hd; int usedBits;
            if (bitsA != bitsB) {
                size_t minBits = bitsA < bitsB ? bitsA : bitsB;
                size_t minBytes = (minBits + 7) / 8;
                hd = hamming_bits(bytesA, bytesB, minBytes);
                usedBits = (int)(minBytes * 8);
            } else {
                size_t bytes = (bitsA + 7) / 8;
                hd = hamming_bits(bytesA, bytesB, bytes);
                usedBits = (int)bitsA;
            }
            g_multiHammingBits[ki] += (unsigned long long)hd;
            g_multiBitsCompared[ki] += (unsigned long long)usedBits;
            g_multiTotalFlips[ki]++;
            free(bytesA); free(bytesB); free(normA); free(normB); free(modHex); free(temp);
        }
    }
}

int main(int argc, char** argv) {
    parse_args(argc, argv);
    unsigned char* base=(unsigned char*)malloc(g_inputLen); if(!base){fprintf(stderr,"OOM base\n"); return 1;}
    double start=wall_time_seconds();
    for (unsigned int m=0; m<g_messages; ++m) {
        random_buffer(base, g_inputLen);
        char* baseHex=NULL; single_hash(base,g_inputLen,&baseHex); if(!baseHex){fprintf(stderr,"hash failed base\n"); free(base); return 1;}
        unsigned int totalBits=(unsigned int)(g_inputLen*8); unsigned int flipsThisMsg = g_sampledBitFlips==0 ? totalBits : (g_sampledBitFlips < totalBits ? g_sampledBitFlips : totalBits);
        for (unsigned int f=0; f<flipsThisMsg; ++f) {
            unsigned int bitPos = (g_sampledBitFlips==0)? f : (unsigned int)rng_index(totalBits);
            unsigned int byteIndex = bitPos/8; unsigned int bitInByte = bitPos%8; unsigned char original=base[byteIndex]; base[byteIndex]^=(unsigned char)(1u<<bitInByte);
            char* modHex=NULL; single_hash(base,g_inputLen,&modHex); if(!modHex){fprintf(stderr,"hash failed mod\n"); free(baseHex); free(base); return 1;}
            char *normA=NULL,*normB=NULL; size_t hexLen=0; normalize_hex(baseHex,modHex,&normA,&normB,&hexLen); unsigned char *bytesA=NULL,*bytesB=NULL; size_t bitsA=hex_to_bits(normA,&bytesA); size_t bitsB=hex_to_bits(normB,&bytesB); int hd; int usedBits; if (bitsA!=bitsB){ size_t minBits=bitsA<bitsB?bitsA:bitsB; size_t minBytes=(minBits+7)/8; hd=hamming_bits(bytesA,bytesB,minBytes); usedBits=(int)(minBytes*8);} else { size_t bytes=(bitsA+7)/8; hd=hamming_bits(bytesA,bytesB,bytes); usedBits=(int)bitsA;} g_totalHammingBits += (unsigned long long)hd; g_totalBitsCompared += (unsigned long long)usedBits; g_totalFlipsPerformed++; record_sample(hd, usedBits); if (g_flagExtended){ size_t bytesUsed=(size_t)usedBits/8; if ((size_t)usedBits%8) bytesUsed++; extended_record_bitwise(bytesA,bytesB,(size_t)usedBits); extended_record_bytes(bytesA,bytesB,bytesUsed);} free(bytesA); free(bytesB); free(normA); free(normB); free(modHex); base[byteIndex]=original; }
        if (g_flagExtended) perform_multi_bit_trials(base,g_inputLen,baseHex);
        free(baseHex);
    }
    double elapsed=wall_time_seconds()-start; double meanAvalanche=(g_totalBitsCompared>0)? ((double)g_totalHammingBits/(double)g_totalBitsCompared):0.0; double meanRatio=(g_totalFlipsPerformed>0)? (g_sumRatios/(double)g_totalFlipsPerformed):0.0; double variance=0.0; if (g_totalFlipsPerformed>1){ double m=meanRatio; variance=(g_sumSqRatios/(double)g_totalFlipsPerformed)-(m*m); if(variance<0) variance=0;} double stddev = (variance>0)? sqrt(variance):0.0; double p=meanAvalanche; double stderrBits = (g_totalBitsCompared>0)? sqrt(p*(1.0-p)/(double)g_totalBitsCompared):0.0; double ci95_low = p - 1.96*stderrBits; double ci95_high = p + 1.96*stderrBits; if (ci95_low<0) ci95_low=0; if (ci95_high>1) ci95_high=1; double zScore = (stderrBits>0)? (p-0.5)/stderrBits:0.0;
    printf("=== Avalanche Test Report ===\n"); printf("Messages: %u\n", g_messages); printf("Input length (bytes): %zu\n", g_inputLen); printf("Bit flips per message: %u\n", g_sampledBitFlips); printf("Rounds: %lu\n", numberOfRounds); printf("Hash buffer size parameter (chars): %d\n", numberOfBits); printf("Total flips performed: %llu\n", (unsigned long long)g_totalFlipsPerformed); printf("Total bits compared: %llu\n", (unsigned long long)g_totalBitsCompared); printf("Total flipped bits observed: %llu\n", (unsigned long long)g_totalHammingBits); printf("Mean avalanche rate (bit-level): %.6f\n", meanAvalanche); printf("Mean per-flip ratio: %.6f\n", meanRatio); printf("Stddev per-flip ratio: %.6f\n", stddev); printf("95%% CI (bit-level p): [%.6f , %.6f]\n", ci95_low, ci95_high); printf("Z-score vs 0.5: %.6f\n", zScore); printf("(DEBUG) raw_fraction = %llu / %llu = %.6f\n", (unsigned long long)g_totalHammingBits, (unsigned long long)g_totalBitsCompared, meanAvalanche); printf("Time: %.3f s (%.2f flips/s)\n", elapsed, elapsed>0? (double)g_totalFlipsPerformed/elapsed:0.0);
    if (!g_flagQuiet){ if (meanAvalanche < 0.40) printf("Assessment: Low diffusion under tested parameters (substantially below 0.5).\n"); else if (meanAvalanche < 0.47) printf("Assessment: Moderate diffusion (below ideal).\n"); else if (meanAvalanche < 0.53) printf("Assessment: Near target diffusion.\n"); else printf("Assessment: >0.53 (could be acceptable or indicate structural artifacts).\n"); }
    if (g_flagHistogram){ printf("Histogram (ratio buckets 0.0-0.1 ... 0.9-1.0):\n"); unsigned long long total=g_totalFlipsPerformed?g_totalFlipsPerformed:1ULL; for(int i=0;i<10;i++){ double pct=(double)g_histBuckets[i]*100.0/(double)total; printf("  [%d] %.2f%% (%llu)\n", i, pct, (unsigned long long)g_histBuckets[i]); } }
    if (g_flagExtended){ if (g_bitCapacity>0){ double sumP=0.0,sumSqP=0.0; double minP=1.0,maxP=0.0; unsigned long long countedBits=0; unsigned long long outOfBand=0; for (size_t i=0;i<g_bitCapacity;i++){ unsigned long long comp=g_bitCompared[i]; if(!comp) continue; double pv=(double)g_bitChanged[i]/(double)comp; sumP += pv; sumSqP += pv*pv; if (pv<minP) minP=pv; if(pv>maxP) maxP=pv; countedBits++; if (pv<0.45||pv>0.55) outOfBand++; } if (countedBits>0){ double meanP=sumP/(double)countedBits; double varP=(sumSqP/(double)countedBits)-meanP*meanP; if (varP<0) varP=0; double sdP=sqrt(varP); printf("--- Extended: Per-bit bias ---\n"); printf("Bits observed: %llu\n", (unsigned long long)countedBits); printf("Min bit flip rate: %.4f Max: %.4f Mean: %.4f SD: %.4f Out-of-[0.45,0.55]: %llu\n", minP, maxP, meanP, sdP, (unsigned long long)outOfBand); } }
    if (g_totalHistogramBytes>0){ long double H=0.0L; for(int i=0;i<256;i++){ unsigned long long c=g_byteFreq[i]; if(!c) continue; long double pbyte=(long double)c/(long double)g_totalHistogramBytes; H -= pbyte*(logl(pbyte)/logl(2.0L)); } long double maxH=logl(256.0L)/logl(2.0L); printf("--- Extended: Output byte distribution ---\n"); printf("Bytes sampled: %llu Entropy: %.4Lf / 8.0000 (%.2Lf%% of max)\n", (unsigned long long)g_totalHistogramBytes, H, (H/maxH)*100.0L); }
        int anyMulti=0; for(int i=0;i<g_multiKCount;i++) if (g_multiTotalFlips[i]>0){ anyMulti=1; break; } if (anyMulti){ printf("--- Extended: Multi-bit flip diffusion ---\n"); for(int i=0;i<g_multiKCount;i++){ if(g_multiTotalFlips[i]==0) continue; double ratio=(g_multiBitsCompared[i]>0)? (double)g_multiHammingBits[i]/(double)g_multiBitsCompared[i]:0.0; printf("k=%d trials=%llu mean_ratio=%.6f\n", g_multiKVals[i], (unsigned long long)g_multiTotalFlips[i], ratio); } } }
    printf("Note: Ratios are influenced by variable hex length; treat results as heuristic.\n");
    free(base); free(g_bitChanged); free(g_bitCompared); return 0; }
