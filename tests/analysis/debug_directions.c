/**
 * Debug the direction extraction for colliding bytes
 */
#include <stdio.h>

#define UP 0
#define RIGHT 1
#define LEFT 2
#define DOWN 3
#define DIRECTIONS 4

static inline int logicalShiftRight(int a, int b) {
    return (int)((unsigned int)a >> b);
}

static void calcAndSetDirections(int byte, int* directions) {
    for (int i = 0; i < DIRECTIONS; i++) directions[i] = 0;
    int index = 0;
    int orig = byte;
    printf("  byte=0x%02X (%d) binary=", orig, orig);
    for (int i = 7; i >= 0; i--) printf("%d", (orig >> i) & 1);
    printf("\n");
    
    while (byte != 0 && index < DIRECTIONS) {
        int dir = byte & 3;
        directions[index] = dir;
        const char* names[] = {"UP", "RIGHT", "LEFT", "DOWN"};
        printf("    [%d] extracted: %d (%s)\n", index, dir, names[dir]);
        index++;
        byte = logicalShiftRight(byte, 2);
    }
    printf("    Total directions extracted: %d\n", index);
}

int main(void) {
    int dirs[DIRECTIONS];
    
    printf("=== Analyzing colliding bytes ===\n\n");
    
    printf("Group 1: 0x66, 0x69, 0x99\n");
    printf("---\n");
    calcAndSetDirections(0x66, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    calcAndSetDirections(0x69, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    calcAndSetDirections(0x99, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    printf("\nGroup 2: 0x5A, 0x96, 0xA5\n");
    printf("---\n");
    calcAndSetDirections(0x5A, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    calcAndSetDirections(0x96, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    calcAndSetDirections(0xA5, dirs);
    printf("  Final dirs: [%d, %d, %d, %d]\n\n", dirs[0], dirs[1], dirs[2], dirs[3]);
    
    return 0;
}
