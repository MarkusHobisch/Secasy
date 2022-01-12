#ifndef SECASY_INITIALIZATIONPHASE_H
#define SECASY_INITIALIZATIONPHASE_H

#define SIZE 8 // 8 x 8 -> 1.000.000 over 64 = 10^294 >> 2^512

/* Directions:
 00 -> up   --> state 0 ; state == value
 11 -> down  --> state 3
 10 -> left  --> state 2
 01 -> right --> state 1
 */
#define UP 0
#define RIGHT 1
#define LEFT 2
#define DOWN 3

// Prevents the formation of squares. Circulating loops (left or right order) lead to identical results and must therefore be avoided
#define SQUARE_AVOIDANCE_VALUE 1

#define DEBUG_MODE 0

extern unsigned long numberOfRounds;
extern int numberOfBits;
extern int numberOfPrimes;
extern int field[SIZE][SIZE];
extern int colorIndexes[SIZE][SIZE];


extern int *generatePrimeNumbers(unsigned int maxPrimeIndex);

extern int getNumberOfPrimes();

extern void initFieldWithDefaultNumbers(unsigned int maxPrimeIndex);

extern void readAndProcessFile(char* filename);

extern void printAllPrimes();

extern void printField();

extern void printColorIndexes();

extern void printSumsAndValues();

extern char * meltingPot();

extern long long int generateHashValue();

extern void calcSum();

struct Position {
    int x;
    int y;
};

struct Position pos;

#endif //SECASY_INITIALIZATIONPHASE_H