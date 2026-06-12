#ifndef SECASY_UTIL_H
#define SECASY_UTIL_H

/* Utility helpers for timing, string duplication and validation (no logic change to core pipeline). */

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Wall clock time in seconds (high resolution where available). */
    double wall_time_seconds(void);

    /* Portable strdup replacement (returns newly allocated copy or NULL). */
    char *secasy_strdup(const char *src);

    /*
     * On Windows: sets the console output codepage to UTF-8 (CP_UTF8) so that
     * box-drawing characters render correctly in cmd.exe and PowerShell.
     * No-op on Linux/macOS where UTF-8 is the default.
     */
    void secasy_enable_utf8_console(void);

    /* Returns non-zero if v is a power of two (>0). */
    int is_power_of_two(long v);

    /*
     * Convert one hex character ('0'-'9','a'-'f','A'-'F') to its 4-bit
     * integer value [0..15].  Returns -1 for any other character.
     */
    int secasy_hex_nibble(char c);

    /*
     * Count differing bits between two hex-encoded hash strings.
     * Compares min(strlen(h1), strlen(h2)) * 4 bits.
     */
    int secasy_hamming_hex(const char *h1, const char *h2);

    /*
     * When g_debug_mode is enabled, all printf output is mirrored to this
     * file in addition to stdout.
     * Set once in main() before any output; NULL disables mirroring.
     */
    extern FILE *g_debug_fp;

    /* printf wrapper that writes to both stdout and g_debug_fp. */
    int tee_printf(const char *fmt, ...);

/* Logging macros — fully C11-compliant (no GNU ##__VA_ARGS__ extension). */
#define LOG_INFO(...)                         \
    do                                        \
    {                                         \
        fprintf(stdout, "[INFO] ");           \
        fprintf(stdout, __VA_ARGS__);         \
        fprintf(stdout, "\n");                \
        if (g_debug_fp)                       \
        {                                     \
            fprintf(g_debug_fp, "[INFO] ");   \
            fprintf(g_debug_fp, __VA_ARGS__); \
            fprintf(g_debug_fp, "\n");        \
        }                                     \
    } while (0)
#define LOG_WARNING(...)               \
    do                                 \
    {                                  \
        fprintf(stderr, "[WARNING] "); \
        fprintf(stderr, __VA_ARGS__);  \
        fprintf(stderr, "\n");         \
    } while (0)
#define LOG_ERROR(...)                \
    do                                \
    {                                 \
        fprintf(stderr, "[ERROR] ");  \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
    } while (0)
#define LOG_DEBUG(...)                    \
    do                                    \
    {                                     \
        if (g_debug_mode)                 \
        {                                 \
            fprintf(stdout, "[DEBUG] ");  \
            fprintf(stdout, __VA_ARGS__); \
            fprintf(stdout, "\n");        \
        }                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* SECASY_UTIL_H */
