#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FIELD_SIZE 8
#define FIRST_PRIME 2

// Simplified hash simulation
uint64_t simple_hash(const unsigned char* data, size_t len) {
    int field[FIELD_SIZE][FIELD_SIZE];
    for(int i=0; i<FIELD_SIZE; i++)
        for(int j=0; j<FIELD_SIZE; j++)
            field[i][j] = FIRST_PRIME;
    
    int x = 0, y = 0;
    int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53};
    int pi = 0;
    
    for(size_t idx = 0; idx < len; idx++) {
        int byte = data[idx] & 0xFF;
        // Apply the fix
        byte ^= (int)((idx * 37 + 17) & 0xFF);
        
        for(int d = 0; d < 4; d++) {
            int dir = byte & 3;
            byte >>= 2;
            
            int oldVal = field[x][y];
            field[x][y] = primes[(++pi) % 16];
            
            switch(dir) {
                case 0: y = (y - oldVal + 1) & 7; break;
                case 1: x = (x + oldVal + 1) & 7; break;
                case 2: x = (x - oldVal) & 7; break;
                case 3: y = (y + oldVal) & 7; break;
            }
        }
    }
    
    uint64_t hash = 0;
    for(int i=0; i<FIELD_SIZE; i++)
        for(int j=0; j<FIELD_SIZE; j++)
            hash ^= ((uint64_t)field[i][j] * (i+1) * (j+1) * 31);
    return hash;
}

int main() {
    printf("Searching for collisions with fix applied...\n");
    printf("Testing all 1-byte pairs (256 x 256)...\n");
    
    int collisions_1byte = 0;
    for(int a = 0; a < 256; a++) {
        for(int b = a+1; b < 256; b++) {
            unsigned char da[1] = {a};
            unsigned char db[1] = {b};
            if(simple_hash(da,1) == simple_hash(db,1)) {
                printf("  1-byte collision: 0x%02X == 0x%02X\n", a, b);
                collisions_1byte++;
            }
        }
    }
    printf("1-byte collisions found: %d\n\n", collisions_1byte);
    
    printf("Testing all 2-byte pairs (65536 x 65536) - this takes a moment...\n");
    
    // Use hash table for 2-byte test
    #define TABLE_SIZE 1000003
    uint64_t* table = calloc(TABLE_SIZE, sizeof(uint64_t));
    uint32_t* inputs = calloc(TABLE_SIZE, sizeof(uint32_t));
    
    int collisions_2byte = 0;
    for(int i = 0; i < 65536 && collisions_2byte < 20; i++) {
        unsigned char d[2] = {i & 0xFF, (i >> 8) & 0xFF};
        uint64_t h = simple_hash(d, 2);
        uint32_t idx = h % TABLE_SIZE;
        
        // Linear probing
        while(table[idx] != 0) {
            if(table[idx] == h) {
                // Verify
                unsigned char d2[2] = {inputs[idx] & 0xFF, (inputs[idx] >> 8) & 0xFF};
                if(simple_hash(d2, 2) == h && inputs[idx] != (uint32_t)i) {
                    printf("  2-byte collision: [0x%02X,0x%02X] == [0x%02X,0x%02X]\n",
                           d[0], d[1], d2[0], d2[1]);
                    collisions_2byte++;
                }
            }
            idx = (idx + 1) % TABLE_SIZE;
        }
        table[idx] = h;
        inputs[idx] = i;
        
        if(i % 10000 == 0) printf("  Progress: %d/65536\n", i);
    }
    
    printf("\n2-byte collisions found: %d\n", collisions_2byte);
    printf("\nDone! Fix appears %s\n", 
           (collisions_1byte == 0 && collisions_2byte == 0) ? "EFFECTIVE" : "INCOMPLETE");
    
    free(table);
    free(inputs);
    return 0;
}
