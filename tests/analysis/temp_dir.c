#include <stdio.h>
#define DIRECTIONS 4
static inline int logicalShiftRight(int a, int b) { return (int)((unsigned int)a >> b); }
static void show(int byte) {
    const char* names[] = {"UP", "RIGHT", "LEFT", "DOWN"};
    printf("0x%02X: ", byte);
    int b = byte;
    while (b != 0) { printf("%s ", names[b & 3]); b = logicalShiftRight(b, 2); }
    printf("\n");
}
int main(void) {
    printf("Input 1: "); show(0x07); show(0x33);
    printf("Input 2: "); show(0x0d); show(0x63);
    return 0;
}
