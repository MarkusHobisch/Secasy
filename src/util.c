#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

void secasy_enable_utf8_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

/*
 * On MinGW-w64 / GCC, this constructor runs automatically before main()
 * in every executable that links util.c.  No call-site changes needed.
 */
#if defined(_WIN32) && defined(__GNUC__)
__attribute__((constructor))
static void win_utf8_init(void)
{
    SetConsoleOutputCP(CP_UTF8);
}
#endif

int secasy_hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int secasy_hamming_hex(const char *h1, const char *h2)
{
    int dist = 0;
    size_t len = strlen(h1) < strlen(h2) ? strlen(h1) : strlen(h2);
    for (size_t i = 0; i < len; i++)
    {
        int n1 = secasy_hex_nibble(h1[i]);
        int n2 = secasy_hex_nibble(h2[i]);
        int xored = (n1 >= 0 && n2 >= 0) ? (n1 ^ n2) : 0;
        while (xored) { dist += (xored & 1); xored >>= 1; }
    }
    return dist;
}
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

FILE *g_debug_fp = NULL;
int g_debug_mode = DEBUG_MODE_DEFAULT;
int g_debug_extended = DEBUG_LOG_EXTENDED_DEFAULT;

int debug_tee_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vprintf(fmt, args);
    va_end(args);
    if (g_debug_fp)
    {
        va_start(args, fmt);
        vfprintf(g_debug_fp, fmt, args);
        va_end(args);
    }
    return n;
}

int is_power_of_two(long v)
{
    return v > 0 && (v & (v - 1)) == 0;
}

char *secasy_strdup(const char *src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1U;
    char *dst = (char *)malloc(len);
    if (!dst)
        return NULL;
    memcpy(dst, src, len);
    return dst;
}

double wall_time_seconds(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int initialized = 0;
    LARGE_INTEGER now;
    if (!initialized)
    {
        QueryPerformanceFrequency(&freq);
        initialized = 1;
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
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
