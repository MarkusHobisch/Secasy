#ifndef SECASY_UTIL_H
#define SECASY_UTIL_H

/* Utility helpers for timing, string duplication and validation (no logic change to core pipeline). */

#include <stddef.h>
#include <stdio.h>
#include "Defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wall clock time in seconds (high resolution where available). */
double wall_time_seconds(void);

/* Portable strdup replacement (returns newly allocated copy or NULL). */
char* secasy_strdup(const char* src);

/* Returns non-zero if v is a power of two (>0). */
int is_power_of_two(long v);

/* Logging macros */
#define LOG_INFO(fmt, ...)  do { fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_ERROR(fmt, ...) do { fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(fmt, ...) do { if (DEBUG_MODE) fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* SECASY_UTIL_H */
