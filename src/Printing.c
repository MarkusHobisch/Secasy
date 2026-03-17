#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#include "Calculations.h"
#include "Defines.h"
#include "Printing.h"
#include "util.h"
/* Mirror all printf output to debug.txt when g_debug_fp is set */
#define printf debug_tee_printf

extern Position_t pos;
extern Tile_t field[FIELD_SIZE][FIELD_SIZE];
extern int lastPrime;

void printField(const char *phase)
{
    /* Fixed column width: cap at 8 chars, truncate longer values with '+' */
    const int maxWidth = 8;
    const int colW = maxWidth + 1;

    printf("\n");
    int innerW = 5 + FIELD_SIZE * colW + 24;
    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n");

    int titleLen = printf("  |  Prime Field  (%u x %u)  [%s]", FIELD_SIZE, FIELD_SIZE, phase ? phase : "");
    int pad = innerW + 4 - titleLen - 18;
    printf("%*sCursor: [%2u,%2u]  |\n", pad > 0 ? pad : 1, "", pos.x, pos.y);

    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n");

    /* Column headers */
    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        if ((uint32_t)i == pos.x)
            printf(" %*d*", maxWidth - 1, i);
        else
            printf(" %*d", maxWidth, i);
    }
    printf("  %20s  |\n", "RowSum");
    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        printf(" ");
        for (int k = 0; k < maxWidth; k++) printf("-");
    }
    printf("  --------------------  |\n");

    for (int j = 0; j < FIELD_SIZE; j++)
    {
        uint64_t rowSum = 0;
        if ((uint32_t)j == pos.y)
            printf("  | %2d*|", j);
        else
            printf("  | %2d |", j);
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            uint64_t v = field[i][j].value;
            rowSum += v;
            int isCursor = ((uint32_t)i == pos.x && (uint32_t)j == pos.y);
            char cell[32];
            snprintf(cell, sizeof(cell), "%" PRIu64, v);
            int len = (int)strlen(cell);
            if (isCursor)
            {
                /* Last char is '*' marker */
                if (len >= maxWidth)
                {
                    /* Truncate: first 5 digits + ".." + "*" */
                    cell[5] = '.';
                    cell[6] = '.';
                    cell[7] = '\0';
                    printf(" %7s*", cell);
                }
                else if (len >= maxWidth - 1)
                {
                    /* Value fills all but marker slot */
                    printf(" %s*", cell);
                }
                else
                {
                    /* Right-align value, then '*' */
                    printf(" %*s*", maxWidth - 1, cell);
                }
            }
            else
            {
                if (len > maxWidth)
                {
                    cell[5] = '.';
                    cell[6] = '.';
                    cell[7] = '\0';
                    printf(" %8s", cell);
                }
                else
                {
                    printf(" %*s", maxWidth, cell);
                }
            }
        }
        /* Row sum: also truncate if needed */
        char rbuf[32];
        snprintf(rbuf, sizeof(rbuf), "%" PRIu64, rowSum);
        if ((int)strlen(rbuf) > 20) { rbuf[18] = '.'; rbuf[19] = '.'; rbuf[20] = '\0'; }
        printf("  %20s  |\n", rbuf);
    }

    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n");

    /* Column sums */
    printf("  | CS |");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        uint64_t colSum = 0;
        for (int j = 0; j < FIELD_SIZE; j++)
            colSum += field[i][j].value;
        char buf[32];
        snprintf(buf, sizeof(buf), "%" PRIu64, colSum);
        if ((int)strlen(buf) > maxWidth) { buf[5] = '.'; buf[6] = '.'; buf[7] = '\0'; }
        printf(" %*s", maxWidth, buf);
    }
    printf("  %20s  |\n", "");
    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n\n");
}

void printColorIndexes()
{
    static const char *opName[] = { "ADD", "SUB", "XOR", "AND", "OR ", "INV" };
    printf("  +-----------------------------------------------------------------------+\n");
    printf("  |  Color Indexes  (operation per cell)                                  |\n");
    printf("  +-----------------------------------------------------------------------+\n");
    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        printf(" %3d", i);
    printf("  |\n");
    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        printf(" ---");
    printf("  |\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        printf("  | %2d |", j);
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            int ci = field[i][j].colorIndex;
            printf(" %s", (ci >= 0 && ci <= 5) ? opName[ci] : "???");
        }
        printf("  |\n");
    }
    printf("  +-----------------------------------------------------------------------+\n\n");
}

void printPrimeIndexes()
{
    /* Determine max width */
    int maxWidth = 1;
    for (int j = 0; j < FIELD_SIZE; j++)
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            int v = (int)field[i][j].primeIndex;
            int w = 1;
            while (v >= 10) { v /= 10; w++; }
            if (w > maxWidth) maxWidth = w;
        }
    if (maxWidth < 6) maxWidth = 6;
    int colW = maxWidth + 1;

    int innerW = 5 + FIELD_SIZE * colW + 9;
    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n");
    int titleLen = printf("  |  Prime Indexes  (next-prime offset per cell)");
    int pad = innerW + 4 - titleLen - 1;
    printf("%*s|\n", pad > 0 ? pad : 1, "");
    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n");

    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        printf(" %*d", maxWidth, i);
    printf("         |\n");
    printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        printf(" ");
        for (int k = 0; k < maxWidth; k++) printf("-");
    }
    printf("         |\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        printf("  | %2d |", j);
        for (int i = 0; i < FIELD_SIZE; i++)
            printf(" %*d", maxWidth, field[i][j].primeIndex);
        printf("         |\n");
    }
    printf("  +");
    for (int i = 0; i < innerW; i++) printf("-");
    printf("+\n\n");
}

void printSumsAndValues()
{
    printf("  +------------------------------------------------------+\n");
    printf("  |  Row Sums               Column Sums                  |\n");
    printf("  +------------------------------------------------------+\n");
    for (int j = 0; j < FIELD_SIZE; ++j)
    {
        uint64_t rowSum = 0, colSum = 0;
        for (int i = 0; i < FIELD_SIZE; ++i)
        {
            rowSum += field[i][j].value;
            colSum += field[j][i].value;
        }
        printf("  | R%2d %20" PRIu64 "  C%2d %20" PRIu64 "   |\n", j, rowSum, j, colSum);
    }
    printf("  +------------------------------------------------------+\n");
    printf("  |  Last prime    : %10d                          |\n", lastPrime);
    printf("  |  Last position : [%2u,%2u]                             |\n", pos.x, pos.y);
    printf("  |  Cell hash     : %20" PRIu64 "                |\n", hashValue(0));
    printf("  +------------------------------------------------------+\n\n");
}

void printDatatypeMaxValues()
{
    printf("\n\n**************/////// MAX VALUES OF DATATYPES ///////**************\n");
    printf("+ LONG_MAX                 %ld\n", LONG_MAX);
    printf("+ INT_MAX                  %i\n", INT_MAX);
    printf("+ LONG_LONG_MAX            %" PRId64 "\n", (int64_t)LLONG_MAX);
    printf("*******************************************************************\n\n");
}

void printInputBits(const unsigned char *data, size_t len)
{
    static const char *dirLabel[] = { "UP", "RI", "LE", "DO" };

    printf("\n");
    printf("  +-----------------------------------------------------------------+\n");
    int n = printf("  |  Input Bit Decomposition  (%zu byte%s = %zu bits)",
                   len, len == 1 ? "" : "s", len * 8);
    printf("%*s|\n", 69 - n - 1, "");
    printf("  +-----------------------------------------------------------------+\n");
    printf("  |  Byte  Char  Hex   Bits       Dir1 Dir2 Dir3 Dir4               |\n");
    printf("  |  ----  ----  ---   --------   ---- ---- ---- ----               |\n");

    for (size_t i = 0; i < len; i++)
    {
        unsigned char b = data[i];
        char ch = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';

        int d1 =  b       & 3;
        int d2 = (b >> 2) & 3;
        int d3 = (b >> 4) & 3;
        int d4 = (b >> 6) & 3;

        printf("  |  %3zu    '%c'  0x%02X  %d%d%d%d%d%d%d%d   %-2s   %-2s   %-2s   %-2s                 |\n",
               i, ch, b,
               (b >> 7) & 1, (b >> 6) & 1, (b >> 5) & 1, (b >> 4) & 1,
               (b >> 3) & 1, (b >> 2) & 1, (b >> 1) & 1, b & 1,
               dirLabel[d1], dirLabel[d2], dirLabel[d3], dirLabel[d4]);
    }

    printf("  +-----------------------------------------------------------------+\n\n");
}
