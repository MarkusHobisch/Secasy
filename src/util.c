#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/time.h>
  #include <unistd.h>
#endif

int is_power_of_two(long v) {
    return v > 0 && (v & (v - 1)) == 0;
}

char* secasy_strdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1U;
    char* dst = (char*)malloc(len);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    return dst;
}

double wall_time_seconds(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    LARGE_INTEGER now;
    if (!initialized) {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    QueryPerformanceCounter(&now);
    return (double) now.QuadPart / (double) freq.QuadPart;
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
#endif
}

