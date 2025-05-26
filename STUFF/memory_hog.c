#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE (10 * 1024 * 1024)  // 10 MB chunks

int main() {
    printf("Memory allocation test starting...\n");
    printf("PID: %d\n", getpid());
    
    // Allocate memory until failure
    size_t allocated = 0;
    void *ptr;
    
    while (1) {
        ptr = malloc(CHUNK_SIZE);
        
        if (ptr == NULL) {
            printf("Memory allocation failed after %.2f GB\n", 
                   (double)allocated / (1024 * 1024 * 1024));
            break;
        }
        
        // Fill memory to ensure it's actually allocated
        memset(ptr, 1, CHUNK_SIZE);
        
        allocated += CHUNK_SIZE;
        
        // Print status every 1 GB
        if (allocated % (100 * CHUNK_SIZE) == 0) {
            printf("Allocated %.2f GB\n", (double)allocated / (1024 * 1024 * 1024));
        }
        
        // Intentionally leak memory - don't free it
        // This ensures we keep consuming memory
    }
    
    printf("Memory allocation test complete\n");
    return 0;
}