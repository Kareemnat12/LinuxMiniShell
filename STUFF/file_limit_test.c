#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

void test_file_limit() {
    int fd_count = 0;
    int *fds = malloc(sizeof(int) * 1024); // Store file descriptors for cleanup

    if (!fds) {
        perror("malloc failed");
        return;
    }

    printf("Opening files until hitting limit...\n");

    while (1) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            if (errno == EMFILE) {
                printf("File descriptor limit reached! Opened %d files\n", fd_count);
            } else {
                printf("Error opening file: %s\n", strerror(errno));
            }
            break;
        }

        fds[fd_count++] = fd;
        if (fd_count % 10 == 0) {
            printf("Opened %d files so far\n", fd_count);
        }
    }

    // Cleanup: close all opened files
    for (int i = 0; i < fd_count; i++) {
        close(fds[i]);
    }

    free(fds);
    printf("All files closed\n");
}

int main() {
    test_file_limit();
    return 0;
}