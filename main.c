#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bits/time.h>
#include <sys/wait.h>
#include <errno.h>

/* Constants */
const int MAX_INPUT_LENGTH = 1024; // Maximum input length
const int MAX_ARGC = 6;           // Maximum number of arguments
const char delim[] = " ";         // Delimiter for splitting commands

/* Function prototypes */
char* get_string();
char** split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* str);
void free_args(char **args);
int count_lines(const char* filename);
char** read_file_lines(const char* filename, int* num_lines);
int is_dangerous_command(char **user_args, int user_args_len);
double time_diff(struct timespec start, struct timespec end);
void append_to_log(const char *filename, char* val1, float val2);
void promt();


/* Global variables */
char **Danger_CMD;   // Dangerous commands from the file
int args_len;        // Length of splitted string from the user input
int numLines = 0;    // Number of lines in the dangerous commands file
char **args;         // Arguments array
struct timespec start, end;  // For time measurement

/**
 * Main function - implements a simple shell
 */
int main(int argc, char* argv[]) {
    // Read dangerous commands from file
   // Danger_CMD = read_file_lines(argv[1], &numLines);
    const char *output_file = argv[2];// the output file log
    Danger_CMD = read_file_lines("./f.txt", &numLines);// to debug pls delete it
    {// to ovveride the output file wich for every new terminal run it starts empty
        //this will be temp untill i write the done functoin
        FILE *clear = fopen(argv[2], "w"); // Truncate the log file at start
        if (clear) fclose(clear);
    }
    // Process user commands in an infinite loop
    while (1) {
        promt();
        char *userInput = get_string();
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Handle input exceeding maximum length
        if (userInput == NULL) {
            free(userInput);
            continue;
        }

        // Check for multiple spaces in input
        int spaceCheck = checkMultipleSpaces(userInput);
        if (spaceCheck == 1) {
            free(userInput);
            continue;
        }

        // Split input into arguments
        args = split_to_args(userInput, delim, &args_len);

        // Handle too many arguments
        if (args == NULL) {
            free_args(args);
            continue;
        }

        // Check for dangerous commands
        if (is_dangerous_command(args, args_len)) {
            continue; // Skip execution of dangerous command
        }

        // Check for exit command
        if (strcmp(args[0], "done") == 0) {
            exit(0);
        }

        // Create child process to execute command
        const pid_t pid = fork();

        if (pid < 0) {
            /* Error occurred */
            fprintf(stderr, "Fork Failed");
            free_args(args);
            return 1;
        }

        if (pid == 0) {
            /* Child process */
            execvp(args[0], args);
            perror("execvp failed");
            exit(127); // Exit with error code 127 if execvp fails

        }

        // Parent process waits for child to complete
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            // Command execution failed, skip time measurement
            printf("Command not found: %s\n", args[0]);
            free_args(args);
            continue;
        }
        // Calculate and display execution time
        clock_gettime(CLOCK_MONOTONIC, &end);
        double total_time = time_diff(start, end);
        append_to_log(output_file, userInput, total_time);


        free_args(args);
        printf("time taken: %.5f seconds\n", total_time);
    }
}

/**
 * Gets user input with dynamic memory allocation
 * Returns NULL if input exceeds MAX_INPUT_LENGTH
 */
char* get_string() {
    size_t buffer_size = MAX_INPUT_LENGTH;  // Start with an initial buffer size
    size_t length = 0;                      // Keep track of the current input length

    char *buffer = malloc(buffer_size);
    if (!buffer) {
        perror("Failed to allocate memory");
        return NULL;
    }

    int c;  // Variable to read each character
    while ((c = getchar()) != EOF && c != '\n') {
        if (length + 1 >= buffer_size) {
            buffer_size *= 2;  // Double the buffer size as needed

            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                perror("Failed to reallocate memory");
                return NULL;
            }
            buffer = new_buffer;
        }

        buffer[length++] = c;  // Append the character to the buffer
    }

    // Null-terminate the string
    buffer[length] = '\0';

    // Check if input length exceeds maximum allowed size
    if (length > MAX_INPUT_LENGTH) {
        printf("Input exceeds the maximum allowed length of %d characters\n", MAX_INPUT_LENGTH);
        free(buffer);
        return NULL;
    }

    return buffer;
}

/**
 * Split string into arguments array (argv style)
 * Returns NULL if the number of arguments exceeds MAX_ARGC
 */
char **split_to_args(const char *string, const char *delimiter, int *count) {
    char *input_copy = strdup(string);  // Copy input to avoid modifying original
    if (!input_copy) {
        perror("Failed to allocate memory");
        exit(1);
    }

    char **argf = NULL;
    *count = 0;

    char *token = strtok(input_copy, delimiter);
    while (token != NULL) {
        // Expand the argv array to hold another string + null
        char **temp = realloc(argf, (*count + 2) * sizeof(char *));
        if (!temp) {
            perror("Failed to reallocate memory");
            free(input_copy);
            exit(1);
        }
        argf = temp;

        // Save a copy of the token
        argf[*count] = strdup(token);
        if (!argf[*count]) {
            perror("Failed to allocate memory for token");
            free(input_copy);
            exit(1);
        }

        (*count)++;
        token = strtok(NULL, delimiter);
    }

    // Null-terminate like a proper argv
    argf[*count] = NULL;

    // Handle maximum number of arguments error
    if (*count - 1 > MAX_ARGC) {
        printf("ERR_ARGS\n");
        for (int i = 0; i < *count; i++) {
            free(argf[i]);
        }
        free(argf);
        free(input_copy);
        return NULL;
    }

    free(input_copy);
    return argf;
}

/**
 * Check for multiple consecutive spaces in a string
 * Returns 1 if multiple spaces found, 0 otherwise
 */
int checkMultipleSpaces(const char* str) {
    int spaceCount = 0;

    while (*str != '\0') {
        if (*str == ' ') {
            spaceCount++;
            if (spaceCount > 1) {
                printf("ERR_SPC\n");
                return 1;
            }
        } else {
            spaceCount = 0; // Reset on non-space
        }
        str++;
    }

    return 0; // No multiple spaces found
}

/**
 * Free memory allocated for arguments array
 */
void free_args(char **args) {
    if (args == NULL) {
        return;
    }

    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }

    free(args);
}

/**
 * Count lines in a file
 */
int count_lines(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for counting");
        return -1;
    }

    int count = 0;
    char buffer[MAX_INPUT_LENGTH];

    while (fgets(buffer, sizeof(buffer), file)) {
        count++;
    }

    fclose(file);
    return count;
}

/**
 * Read lines from a file into a string array
 */
char** read_file_lines(const char* filename, int* num_lines) {
    *num_lines = count_lines(filename);
    if (*num_lines <= 0) {
        return NULL;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for reading");
        return NULL;
    }

    char** lines = malloc((*num_lines) * sizeof(char*));
    if (!lines) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    char buffer[MAX_INPUT_LENGTH];
    int i = 0;

    while (fgets(buffer, sizeof(buffer), file) && i < *num_lines) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
        lines[i] = strdup(buffer); // Store line
        i++;
    }

    fclose(file);
    return lines;
}

/**
 * Check if command is in the list of dangerous commands
 * Returns 1 if dangerous, 0 otherwise
 */
int is_dangerous_command(char **user_args, int user_args_len) {
    for (int i = 0; i < numLines; i++) {
        int temp_count;
        char **dangerous_args = split_to_args(Danger_CMD[i], delim, &temp_count);

        if (dangerous_args == NULL) {
            continue; // Skip if dangerous command couldn't be split properly
        }

        // Compare the command name (first argument)
        if (strcmp(user_args[0], dangerous_args[0]) == 0) {
            // Exact match check (same command and arguments)
            if (user_args_len == temp_count) {
                int exact_match = 1;
                for (int j = 0; j < user_args_len; j++) {
                    if (strcmp(user_args[j], dangerous_args[j]) != 0) {
                        exact_match = 0;
                        break;
                    }
                }

                if (exact_match) {
                    // Exact match found
                    printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n", user_args[0]);
                    free_args(dangerous_args);
                    return 1; // Block execution
                }
            }

            // Similar command found (same name but different arguments)
            printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", dangerous_args[0]);
            free_args(dangerous_args);
            return 0; // Allow execution with warning
        }

        free_args(dangerous_args);
    }

    return 0; // No dangerous command found
}

/**
 * Calculate time difference between two timespec structs
 * Returns difference in seconds as a double
 */
double time_diff(struct timespec start, struct timespec end) {
    // Calculate the difference in seconds and nanoseconds
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;

    // If nanoseconds are negative, borrow 1 second
    if (nsec_diff < 0) {
        nsec_diff += 1000000000; // 1 second = 1 billion nanoseconds
        sec_diff -= 1;
    }

    // Combine the seconds and nanoseconds difference
    double total_time = sec_diff + nsec_diff / 1000000000.0;

    // Return total time in seconds with 5 digits after the decimal
    return total_time;
}




void append_to_log(const char *filename, char * val1, float val2) {
    FILE *file = fopen(filename, "a");  // Append mode
    if (!file) {
        perror("Error opening log file");
        return;
    }

    fprintf(file, "%s : %.5f sec\n", val1, val2);
    fclose(file);
}

void promt() {
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>\n",
           total_cmd_count,
           dangerous_cmd_blocked_count,
           last_cmd_time,
           average_time,
           min_time,
           max_time);
}