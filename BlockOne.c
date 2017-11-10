//
// Created by Markus on 10.11.2017.
//

#include <assert.h>
#include <stdio.h>
#include "BlockOne.h"


int field[SIZE][SIZE];
int primeIndexes[SIZE][SIZE];
int colorIndexes[SIZE][SIZE];
int rowSum[SIZE];
int columnSum[SIZE];
int lastPrime = 0;
int colorLen = 5;
int counter = 0;
int primeIndex = 0;
int colorIndex = 0;
int *primeArray;

void generateField(){

    //first check if field size is power of 2!
    assert((SIZE & (SIZE - 1)) == 0); // SIZE is not the power of 2!

    if(primeArray == NULL)
        primeArray = generatePrimeNumbers(prime_index_);

    // init field
    for(int i = 0; i < SIZE; i++){
        for(int j = 0; j < SIZE; j++){
            field[i][j] = 2;
        }
    }

    // process file
    //readAndProcessFile();
}
