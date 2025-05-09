#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>

void cpu_limit_handler(int signum) {
    printf("CPU time limit exceeded!\n");
    exit(1);
}

int main() {
    struct rlimit rl;
    signal(SIGXCPU, cpu_limit_handler);

    getrlimit(RLIMIT_CPU, &rl);
    printf("Current CPU limit: soft=%ld, hard=%ld sec\n",
           (long)rl.rlim_cur, (long)rl.rlim_max);

    rl.rlim_cur = 2;  /* Soft limit: 2 seconds */
    rl.rlim_max = 3;  /* Hard limit: 3 seconds */
    setrlimit(RLIMIT_CPU, &rl);

    printf("Starting CPU-intensive task...\n");
    while (1) {
        for (int i = 0; i < 10000000; i++) {
            double x = i * 3.14159;
            x = x / 2.71828;
        }
    }

    return 0;
}