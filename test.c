#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// Function to write to file (append or overwrite)
void write_to_file(const char *filename, const char *content, int append) {
    int flags = O_WRONLY | O_CREAT;
    if (append)
        flags |= O_APPEND;
    else
        flags |= O_TRUNC;

    int fd = open(filename, flags, 0644);
    if (fd == -1) {
        perror("open failed");
        return;
    }

    if (write(fd, content, strlen(content)) == -1) {
        perror("write failed");
    }

    close(fd);
}

// Main to test writing
int main() {
    const char *filename = "test_output.txt";

    // Test overwrite mode
    printf("Writing in overwrite mode...\n");
    write_to_file(filename, "First line: Overwritten content!\n", 0);

    // Test append mode
    printf("Appending more content...\n");
    write_to_file(filename, "Second line: Appended content!\n", 1);
    write_to_file(filename, "Third line: Appended again!\n", 1);

    printf("Done. Check the '%s' file!\n", filename);
    return 0;
}
