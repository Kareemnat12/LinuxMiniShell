#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    // Get the output filename (exec_times.log in this case)
    char output_file[256];
    strcpy(output_file, argv[2]);  // The second argument is the output file

    // Check if the output filename ends with ".log"
    if (strlen(output_file) < 4 || strcmp(&output_file[strlen(output_file) - 4], ".log") != 0) {
        strcat(output_file, ".log");  // Append ".log" if missing
    }

    // Open the output file in append mode (create it if it doesn't exist)
    FILE *file = fopen(output_file, "a");
    if (file == NULL) {
        perror("Error opening output file");
        return 1;
    }

    // Some data to write (for example, execution time or stats)
    const char *command = "Some dangerous command";
    int exec_time = 12;  // Example execution time

    // Write formatted data to the file
    fprintf(file, "Command: %s | Execution Time: %d seconds\n", command, exec_time);

    // Close the file
    fclose(file);

    printf("Logged to %s successfully!\n", output_file);
    return 0;
}