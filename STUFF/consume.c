// Function that consumes CPU intensively
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>

void sigxcpu_handler(int sig) {
    printf("CPU time limit exceeded!\n");
    exit(1);
}
void consume_cpu() {
    // Volatile to prevent compiler optimization
    volatile unsigned long long i = 0;

    // Infinite loop of CPU-intensive calculations
    while(1) {
        i++;

        // Some math operations to ensure CPU usage
        double result = 0;
        for(int j = 0; j < 1000; j++) {
            result += sqrt(j * i) / (i % 100 + 1);
        }

        // Print occasionally to show progress
        if(i % 10000000 == 0) {
            printf("Still running... (iterations: %llu)\n", i);
        }
    }
}
void main() {
    signal(SIGXCPU, sigxcpu_handler);
    consume_cpu();
}