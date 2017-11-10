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

extern char input_filename_[FILENAME_LEN];
extern unsigned long rounds_;
extern unsigned long prime_index_;

extern int *generatePrimeNumbers(unsigned int n);
extern void generateField();
extern void printAllPrimes();

#endif //SECASY_BLOCKONE_H
