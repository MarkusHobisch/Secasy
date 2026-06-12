#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#include "Calculations.h"
#include "Defines.h"
#include "InitializationPhase.h"
#include "Printing.h"
#include "util.h"

extern Position pos;
extern Tile field[FIELD_SIZE][FIELD_SIZE];

void printField(const char *phase)
{
    const int maxWidth = 8;
    const int colW = maxWidth + 1;

    tee_printf("\n");
    int innerW = 5 + FIELD_SIZE * colW + 24;
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");

    int titleLen = tee_printf("  |  Prime Field  (%u x %u)  [%s]", FIELD_SIZE, FIELD_SIZE, phase ? phase : "");
    int pad = innerW + 4 - titleLen - 18;
    tee_printf("%*sCursor: [%2u,%2u]  |\n", pad > 0 ? pad : 1, "", pos.x, pos.y);

    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");

    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        if ((uint32_t)i == pos.x)
            tee_printf(" %*d*", maxWidth - 1, i);
        else
            tee_printf(" %*d", maxWidth, i);
    }
    tee_printf("  %20s  |\n", "RowSum");
    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        tee_printf(" ");
        for (int k = 0; k < maxWidth; k++)
            tee_printf("-");
    }
    tee_printf("  --------------------  |\n");

    for (int j = 0; j < FIELD_SIZE; j++)
    {
        uint64_t rowSum = 0;
        if ((uint32_t)j == pos.y)
            tee_printf("  | %2d*|", j);
        else
            tee_printf("  | %2d |", j);
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
                if (len >= maxWidth)
                {
                    cell[5] = '.';
                    cell[6] = '.';
                    cell[7] = '\0';
                    tee_printf(" %7s*", cell);
                }
                else if (len >= maxWidth - 1)
                {
                    tee_printf(" %s*", cell);
                }
                else
                {
                    tee_printf(" %*s*", maxWidth - 1, cell);
                }
            }
            else
            {
                if (len > maxWidth)
                {
                    cell[5] = '.';
                    cell[6] = '.';
                    cell[7] = '\0';
                    tee_printf(" %8s", cell);
                }
                else
                {
                    tee_printf(" %*s", maxWidth, cell);
                }
            }
        }
        char rbuf[32];
        snprintf(rbuf, sizeof(rbuf), "%" PRIu64, rowSum);
        if ((int)strlen(rbuf) > 20)
        {
            rbuf[18] = '.';
            rbuf[19] = '.';
            rbuf[20] = '\0';
        }
        tee_printf("  %20s  |\n", rbuf);
    }

    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");

    tee_printf("  | CS |");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        uint64_t colSum = 0;
        for (int j = 0; j < FIELD_SIZE; j++)
            colSum += field[i][j].value;
        char buf[32];
        snprintf(buf, sizeof(buf), "%" PRIu64, colSum);
        if ((int)strlen(buf) > maxWidth)
        {
            buf[5] = '.';
            buf[6] = '.';
            buf[7] = '\0';
        }
        tee_printf(" %*s", maxWidth, buf);
    }
    tee_printf("  %20s  |\n", "");
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n\n");
}

void printColorIndexes()
{
    static const char *opName[] = {"ADD", "SUB", "XOR", "RLX", "RRA", "INV"};
    tee_printf("  +-----------------------------------------------------------------------+\n");
    tee_printf("  |  Color Indexes  (operation per cell)                                  |\n");
    tee_printf("  +-----------------------------------------------------------------------+\n");
    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        tee_printf(" %3d", i);
    tee_printf("  |\n");
    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        tee_printf(" ---");
    tee_printf("  |\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        tee_printf("  | %2d |", j);
        for (int i = 0; i < FIELD_SIZE; i++)
        {
            int ci = field[i][j].colorIndex;
            tee_printf(" %s", (ci >= 0 && ci <= 5) ? opName[ci] : "???");
        }
        tee_printf("  |\n");
    }
    tee_printf("  +-----------------------------------------------------------------------+\n\n");
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
            while (v >= 10)
            {
                v /= 10;
                w++;
            }
            if (w > maxWidth)
                maxWidth = w;
        }
    if (maxWidth < 6)
        maxWidth = 6;
    int colW = maxWidth + 1;

    int innerW = 5 + FIELD_SIZE * colW + 9;
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");
    int titleLen = tee_printf("  |  Prime Indexes  (next-prime offset per cell)");
    int pad = innerW + 4 - titleLen - 1;
    tee_printf("%*s|\n", pad > 0 ? pad : 1, "");
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");

    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
        tee_printf(" %*d", maxWidth, i);
    tee_printf("         |\n");
    tee_printf("  |     ");
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        tee_printf(" ");
        for (int k = 0; k < maxWidth; k++)
            tee_printf("-");
    }
    tee_printf("         |\n");
    for (int j = 0; j < FIELD_SIZE; j++)
    {
        tee_printf("  | %2d |", j);
        for (int i = 0; i < FIELD_SIZE; i++)
            tee_printf(" %*d", maxWidth, field[i][j].primeIndex);
        tee_printf("         |\n");
    }
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n\n");
}

void printSumsAndValues()
{
    tee_printf("  +------------------------------------------------------+\n");
    tee_printf("  |  Row Sums               Column Sums                  |\n");
    tee_printf("  +------------------------------------------------------+\n");
    uint64_t totalRowSum = 0, totalColSum = 0;
    for (int j = 0; j < FIELD_SIZE; ++j)
    {
        uint64_t rowSum = 0, colSum = 0;
        for (int i = 0; i < FIELD_SIZE; ++i)
        {
            rowSum += field[i][j].value;
            colSum += field[j][i].value;
        }
        totalRowSum += rowSum;
        totalColSum += colSum;
        tee_printf("  | R%2d %20" PRIu64 "  C%2d %20" PRIu64 "   |\n", j, rowSum, j, colSum);
    }
    tee_printf("  +------------------------------------------------------+\n");
    tee_printf("  | R*  %20" PRIu64 "  C*  %20" PRIu64 "   |\n", totalRowSum, totalColSum);
    tee_printf("  +------------------------------------------------------+\n");
    tee_printf("  |  Last position : [%2u,%2u]                             |\n", pos.x, pos.y);
    tee_printf("  |  Cell hash     : %20" PRIu64 "                |\n", hashValue(0));
    tee_printf("  +------------------------------------------------------+\n\n");
}

void printDatatypeMaxValues()
{
    tee_printf("\n\n**************/////// MAX VALUES OF DATATYPES ///////**************\n");
    tee_printf("+ LONG_MAX                 %ld\n", LONG_MAX);
    tee_printf("+ INT_MAX                  %i\n", INT_MAX);
    tee_printf("+ LONG_LONG_MAX            %" PRId64 "\n", (int64_t)LLONG_MAX);
    tee_printf("*******************************************************************\n\n");
}

void printInputBits(const unsigned char *data, size_t len)
{
    static const char *dirLabel[] = {"UP", "RI", "LE", "DO"};

    tee_printf("\n");
    tee_printf("  +-----------------------------------------------------------------+\n");
    int n = tee_printf("  |  Input Bit Decomposition  (%zu byte%s = %zu bits)",
                   len, len == 1 ? "" : "s", len * 8);
    tee_printf("%*s|\n", 69 - n - 1, "");
    tee_printf("  +-----------------------------------------------------------------+\n");
    tee_printf("  |  Byte  Char  Hex   Bits       Dir1 Dir2 Dir3 Dir4               |\n");
    tee_printf("  |  ----  ----  ---   --------   ---- ---- ---- ----               |\n");

    for (size_t i = 0; i < len; i++)
    {
        unsigned char b = data[i];
        char ch = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';

        int d1 = b & DIRECTION_MASK;
        int d2 = (b >> BITS_PER_DIRECTION) & DIRECTION_MASK;
        int d3 = (b >> (2 * BITS_PER_DIRECTION)) & DIRECTION_MASK;
        int d4 = (b >> (3 * BITS_PER_DIRECTION)) & DIRECTION_MASK;

        tee_printf("  |  %3zu    '%c'  0x%02X  %d%d%d%d%d%d%d%d   %-2s   %-2s   %-2s   %-2s                 |\n",
               i, ch, b,
               (b >> 7) & 1, (b >> 6) & 1, (b >> 5) & 1, (b >> 4) & 1,
               (b >> 3) & 1, (b >> 2) & 1, (b >> 1) & 1, b & 1,
               dirLabel[d1], dirLabel[d2], dirLabel[d3], dirLabel[d4]);
    }

    tee_printf("  +-----------------------------------------------------------------+\n\n");
}

void printPathMap(void)
{
    int nSteps = getPathStepCount();
    if (nSteps == 0)
        return;

    static const char dirArrow[] = {'^', '>', '<', 'v'};

    int cellStep[FIELD_SIZE][FIELD_SIZE];
    memset(cellStep, 0, sizeof(cellStep));
    int cellDir[FIELD_SIZE][FIELD_SIZE];
    memset(cellDir, -1, sizeof(cellDir));

    for (int s = 0; s < nSteps; s++)
    {
        uint32_t fX, fY, tX, tY;
        int dir;
        getPathStep(s, &fX, &fY, &tX, &tY, &dir);

        if (cellStep[fX][fY] == 0)
        {
            cellStep[fX][fY] = s + 1;
            cellDir[fX][fY] = dir;
        }
    }

    if (nSteps > 0)
    {
        uint32_t fX, fY, tX, tY;
        int dir;
        getPathStep(nSteps - 1, &fX, &fY, &tX, &tY, &dir);
        if (cellStep[tX][tY] == 0)
        {
            cellStep[tX][tY] = nSteps + 1;
            cellDir[tX][tY] = -1;
        }
    }

    const int cellW = 5;
    int innerW = 6 + FIELD_SIZE * cellW;

    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");
    int tl = tee_printf("  |  Path Map  (%d steps)", nSteps);
    tee_printf("%*s|\n", innerW - tl + 2, "");
    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");

    tee_printf("      ");
    for (int i = 0; i < FIELD_SIZE; i++)
        tee_printf(" %2d  ", i);
    tee_printf("\n");

    for (int y = 0; y < FIELD_SIZE; y++)
    {
        tee_printf("  %2d |", y);
        for (int x = 0; x < FIELD_SIZE; x++)
        {
            int s = cellStep[x][y];
            int d = cellDir[x][y];
            if (s > 0 && d >= 0 && d < 4)
                tee_printf(" %2d%c ", s, dirArrow[d]);
            else if (s > 0)
                tee_printf(" %2d* ", s);
            else
                tee_printf("  .  ");
        }
        tee_printf("|\n");
    }

    tee_printf("  +");
    for (int i = 0; i < innerW; i++)
        tee_printf("-");
    tee_printf("+\n");
    tee_printf("  (Number=step, ^v<>=direction, *=final position)\n\n");
}

void exportGridCSV(const char *filename, const char *phase)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        LOG_WARNING("Could not open %s for writing", filename);
        return;
    }

    fprintf(fp, "# phase: %s\n", phase ? phase : "unknown");
    fprintf(fp, "# cursor: %u,%u\n", pos.x, pos.y);
    fprintf(fp, "x,y,value,primeIndex,colorIndex\n");

    for (uint32_t y = 0; y < FIELD_SIZE; y++)
    {
        for (uint32_t x = 0; x < FIELD_SIZE; x++)
        {
            const Tile *t = &field[x][y];
            fprintf(fp, "%u,%u,%" PRIu64 ",%u,%d\n",
                    x, y, t->value, t->primeIndex, (int)t->colorIndex);
        }
    }

    fclose(fp);
    LOG_INFO("Grid state exported to %s", filename);
}
