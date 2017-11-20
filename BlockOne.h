//
// Created by Markus on 10.11.2017.
//

#ifndef SECASY_BLOCKONE_H
#define SECASY_BLOCKONE_H

#define FILENAME_LEN 100
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
#define SQUARE_AVOIDANCE_FACTOR 1 // Prevents the formation of squares

#define DEBUG_MODE 0

extern char input_filename_[FILENAME_LEN];
extern unsigned long rounds_;
extern unsigned long prime_index_;
extern int bit_size_;
extern int primesLength_;
extern int field_[SIZE][SIZE];
extern int colorIndexes_[SIZE][SIZE];


extern int *generatePrimeNumbers(unsigned int n);

extern void generateField();

extern void printAllPrimes();

extern void printField();

extern void printColorIndexes();

extern void printSumsAndValues();

extern char * meltingPot();

extern long long int generateHashValue();

extern void calcSum();

struct Position
{
    int x;
    int y;
};

struct Position pos;

#endif //SECASY_BLOCKONE_H
