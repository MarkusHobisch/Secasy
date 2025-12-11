#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char cmd[256], hash[256];
    char hashes[256][65] = {0};
    int collisions = 0;
    
    printf("Testing all 256 single-byte inputs...\n");
    
    for(int i = 0; i < 256; i++) {
        // Create file with single byte
        FILE* f = fopen("temp_byte.bin", "wb");
        unsigned char byte = (unsigned char)i;
        fwrite(&byte, 1, 1, f);
        fclose(f);
        
        // Run secasy and capture output
        FILE* p = popen("./secasy.exe -f temp_byte.bin -n 64 2>&1", "r");
        char line[512];
        while(fgets(line, sizeof(line), p)) {
            // Look for hash line (32 hex chars)
            if(strlen(line) >= 32) {
                int is_hash = 1;
                for(int j = 0; j < 32; j++) {
                    char c = line[j];
                    if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                        is_hash = 0;
                        break;
                    }
                }
                if(is_hash) {
                    strncpy(hash, line, 32);
                    hash[32] = '\0';
                    break;
                }
            }
        }
        pclose(p);
        
        // Check for collision
        for(int j = 0; j < i; j++) {
            if(strcmp(hashes[j], hash) == 0) {
                printf("COLLISION: 0x%02X == 0x%02X (hash: %s)\n", j, i, hash);
                collisions++;
            }
        }
        strcpy(hashes[i], hash);
        
        if((i+1) % 64 == 0) printf("  Progress: %d/256\n", i+1);
    }
    
    printf("\n=== RESULT: %d collisions found in 256 single-byte inputs ===\n", collisions);
    return 0;
}
