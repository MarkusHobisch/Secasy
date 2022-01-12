#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "BlockOne.h"


int field_[SIZE][SIZE];
int primeIndexes[SIZE][SIZE];
int colorIndexes_[SIZE][SIZE];
int rowSum[SIZE];
int columnSum[SIZE];

int lastPrime = 0;
int colorLen = 5;
int counter = 0;
int primeIndex = 0;
int colorIndex = 0;
int *primeArray;


void readAndProcessFile();

void calcDirections(int ops, int *dir);

int logical_shift_right(int a, int b);

void fillField(int *dir);

int nextPrimeNumber();

void writeOnMove(int direction);

int fastMod(int dividend, int divisor);

long long getFieldSum();

long long getMultipliedSums();

void clearArray(int dir[4]);

void generateField() {

    //first check if field_ size is power of 2!
    assert((SIZE & (SIZE - 1)) == 0); // SIZE is not the power of 2!

    if (primeArray == NULL)
        primeArray = generatePrimeNumbers(prime_index_);

    // init field_
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            field_[i][j] = 2;
        }
    }

    // process file
    readAndProcessFile();
}

void readAndProcessFile() {
    FILE *file = NULL;
    const unsigned int BUFFER_SIZE = 1 * 1024 * 1024;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytesRead = 0;
    int dir[4] = {0};
    int block = 0;

    file = fopen(input_filename_, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file with filename: %s\n", input_filename_);
        exit(EXIT_FAILURE);
    }


    while ((bytesRead = fread(buffer, sizeof (char), sizeof (buffer), file)) > 0) {
        int byte; // must be int!
        for (int i = 0; i < bytesRead; ++i) {
            byte = buffer[i];
            if (byte != 0) {
                block = byte & 0xFF; // byte to int conversation
                calcDirections(block, dir);
            } else {
                dir[0] = 0;
                dir[1] = 0;
                dir[2] = 0;
                dir[3] = 0;
            }
            fillField(dir);
            clearArray(dir);
        }
    }
    // update last position
    field_[pos.x][pos.y] = nextPrimeNumber();
    lastPrime = field_[pos.x][pos.y];
}

void clearArray(int dir[4]) {
    for (int i = 0; i < 4; ++i) {
        dir[i] = 0;
    }
}

void fillField(int *dir) {
    if (DEBUG_MODE)
        printf("start by pos[%d,%d]\n", pos.x, pos.y);
    for (int i = 0; i < 4; i++) {
        writeOnMove(dir[i]);
    }
}

void writeOnMove(int direction) {
    int oldPrime = field_[pos.x][pos.y];
    int nextPrime = nextPrimeNumber();
    field_[pos.x][pos.y] = nextPrime;
    if (DEBUG_MODE) {
        printf("%d prime -> new value: %d ", oldPrime, nextPrime);
        printf("dir: %d", direction);
    }

    int newPos = 0;
    switch (direction) {
        case UP:
            newPos = pos.y - oldPrime + SQUARE_AVOIDANCE_FACTOR;
            pos.y = fastMod(fastMod(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" UP\n");
            break;
        case DOWN:
            newPos = pos.y + oldPrime;
            pos.y = fastMod(fastMod(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" DOWN\n");
            break;
        case LEFT:
            newPos = pos.x - oldPrime;
            pos.x = fastMod(fastMod(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" LEFT\n");
            break;
        case RIGHT:
            newPos = pos.x + oldPrime + SQUARE_AVOIDANCE_FACTOR;
            pos.x = fastMod(fastMod(newPos, SIZE) + SIZE, SIZE);
            if (DEBUG_MODE)
                printf(" RIGHT\n");
            break;
        default:
            printf("UNKNOWN POSITION !!\n");
            break;
    }
}

int fastMod(int dividend, int divisor) {
    return dividend & (divisor - 1);
}

int nextPrimeNumber() {
    primeIndex = primeIndexes[pos.x][pos.y];
    colorIndex = colorIndexes_[pos.x][pos.y];

    primeIndex = ++primeIndex < primesLength_ ? primeIndex : 0;
    colorIndex = (++colorIndex < colorLen) && primeIndex != 0 ? colorIndex : 0;

    primeIndexes[pos.x][pos.y] = primeIndex;
    colorIndexes_[pos.x][pos.y] = colorIndex;
    return primeArray[primeIndex];
}

void calcDirections(int ops, int *dir) {
    int index = 0;
    while (ops != 0) {
        dir[index++] = (ops & 3); // max ops length is always 8 bits, otherwise assertion will fail!
        ops = logical_shift_right(ops, 2); // is the same as ops >>>= 2 in Java
        //assert(index <= 4 && "index is negative!");
        counter++;
    }

}

int logical_shift_right(int a, int b) {
    return (int) ((unsigned int) a >> b);
}

void calcSum() {
    int sum;
    for (int j = 0; j < SIZE; j++) {
        sum = 0;
        for (int i = 0; i < SIZE; i++) {
            sum += field_[i][j];
        }
        rowSum[j] = sum;
    }

    for (int j = 0; j < SIZE; j++) {
        sum = 0;
        for (int i = 0; i < SIZE; i++) {
            sum += field_[j][i];
        }
        columnSum[j] = sum;
    }
}

long long getFieldSum() {
    long long sum = 0;
    for (int i = 0; i < SIZE; ++i) {
        for (int j = 0; j < SIZE; ++j) {
            sum += field_[i][j];
        }
    }
    return sum;
}

long long getMultipliedSums() {
    long long muliply1 = 1;
    long long muliply2 = 1;
    for (int i = 0; i < SIZE; i++) {
        muliply1 *= rowSum[i];
    }

    for (int i = 0; i < SIZE; i++) {
        muliply2 *= columnSum[i];
    }
    return (muliply1 + muliply2);
}

void printField() {
    printf("-------------- Prime Field ------------\n");
    printf("\n\t Origin matrix - ");
    printf("last position: [%d,%d] \n", pos.x, pos.y);
    calcSum();
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            printf("%15d ", field_[i][j]);
        }
        printf("sum row: %d\n", rowSum[j]);
    }

    printf("\n\t Transposed matrix\n");
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            printf("%15d ", field_[j][i]);
        }
        printf("sum column: %d\n", columnSum[j]);
    }
    printf("\n\n");
}

void printColorIndexes() {
    printf("-------------- ColorIndexes ------------\n");
    for (int j = 0; j < SIZE; j++) {
        for (int i = 0; i < SIZE; i++) {
            printf("%4d ", colorIndexes_[i][j]);
        }
        printf("\n");
    }
}

void printSumsAndValues() {
    printf("\n");
    printf("- Print row sums: \n");
    calcSum();
    for (int i = 0; i < SIZE; ++i) {
        printf("  Row: %d\n", rowSum[i]);
    }
    printf("\n");
    printf("- Print column sums: \n");
    for (int j = 0; j < SIZE; ++j) {
        printf("  Column: %d\n", columnSum[j]);
    }
    printf("\n");
    printf("- Last prime was %d\n", lastPrime);
    printf("- Last position was [%d,%d]\n", pos.x, pos.y);
    printf("- Get multiplied sums: %lli\n", getMultipliedSums());
    printf("- Number of iterations: %4d \n", counter);
}

long long generateHashValue() {
    calcSum();
    const long long checksum = getMultipliedSums() ^lastPrime;
    //printf("checksum = %lld\n", checksum);

    const long long fieldSum = getFieldSum();
    //printf("fieldSum = %lld\n", fieldSum);

    return checksum ^ fieldSum; // hash value

}

