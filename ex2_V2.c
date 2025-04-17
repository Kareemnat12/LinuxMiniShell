#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bits/time.h>
#include <sys/wait.h>

/* Constants */
const int MAX_INPUT_LENGTH = 1024;  // Maximum input length
const int MAX_ARGC = 6;             // Maximum number of arguments
const char delim[] = " ";           // Delimiter for splitting commands

/* Function prototypes */
char* get_string();
char** split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* input);
void free_args(char **args);
char** read_file_lines(const char* filename, int* num_lines);
int is_dangerous_command(char **user_args, int user_args_len);
float time_diff(struct timespec start, struct timespec end);
void append_to_log(const char *filename, char* val1, float val2);
void prompt();
void update_min_max_time(double current_time, double *min_time, double *max_time);
void finish_time_append();

/* Global variables */
char **Danger_CMD;              // Dangerous commands from the file
int args_len;                   // Length of splitted string from the user input
int numLines = 0;               // Number of lines in the dangerous commands file
char **args;                    // Arguments array
struct timespec start, end;     // For time measurement

/* Global variables for command statistics */
int total_cmd_count = 0;               // Total number of commands executed
int dangerous_cmd_blocked_count = 0;   // Number of dangerous commands blocked
double last_cmd_time = 0;              // Time taken for the last successful command
double average_time = 0;               // Average time taken for successful commands
double total_time_all = 0;             // To help calculate the average time by getting the sum of the times
double min_time = -1;                  // Must start negative so it gets overwritten
double max_time = 0;                   // Maximum time for command execution
int semi_dangerous_cmd_count = 0;      // Number of similar commands detected
float time_other=0; // Time for other commands
char *userInput;
const char *output_file;
/**
 * Main function - implements a simple shell
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {  // Check that we have 2 files as argv
        fprintf(stderr, "Usage: %s <dangerous_commands_file> <log_file>\n", argv[0]);
        exit(1);
    }

    // Setup file paths
    output_file = argv[2];  // The output file log
    const char *input_file = argv[1];   // The input file with the dangerous commands

    // Read dangerous commands from file
    Danger_CMD = read_file_lines(input_file, &numLines);
    if (Danger_CMD == NULL) {
        fprintf(stderr, "Failed to load dangerous commands\n");
        exit(1);
    }

    {  // Override the output file which for every new terminal run starts empty
        FILE *clear = fopen(argv[2], "w");  // Truncate the log file at start
        if (clear) fclose(clear);
    }

    // Process user commands in an infinite loop
    while (1) {
        prompt();
        userInput = get_string();
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Handle input exceeding maximum length
        if (userInput == NULL) {// im not sure if we should add here time man
            continue;
        }

        // Check for multiple spaces in input
        int spaceCheck = checkMultipleSpaces(userInput);
        if (spaceCheck == 1) {
           finish_time_append();
            free(userInput);
            continue;
        }

        // Split input into arguments
        args = split_to_args(userInput, delim, &args_len);

        // Handle too many arguments
        if (args == NULL) {
            finish_time_append();

            free_args(args);
            args = NULL;
            free(userInput);
            continue;
        }

        // Check for dangerous commands
        if (is_dangerous_command(args, args_len)) {

            finish_time_append();

            free(userInput);
            free_args(args);
            args = NULL;
            continue;  // Skip execution of dangerous command
        }

        // Check for exit command
        if (strcmp(args[0], "done") == 0) {
            free(userInput);
            free_args(args);
            args = NULL;
            free_args(Danger_CMD);
            Danger_CMD = NULL;
            printf("blocked command: %d\nunblocked commands: %d\n",
                   dangerous_cmd_blocked_count, semi_dangerous_cmd_count);
            exit(0);
        }

        // Create child process to execute command
        const pid_t pid = fork();

        if (pid < 0) {
            /* Error occurred */
            fprintf(stderr, "Fork Failed");
            free_args(args);
            args = NULL;
            return 1;
        }

        if (pid == 0) {
            /* Child process */
            execvp(args[0], args);
            perror("execvp failed");
            exit(127);  // Exit with error code 127 if execvp fails
        }

        // Parent process waits for child to complete
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            // Command execution failed, skip time measurement
            finish_time_append();

            printf("Command not found: %s\n", args[0]);
            free_args(args);
            args = NULL;
            free(userInput);
            continue;
        }
        // Calculate and display execution time
        clock_gettime(CLOCK_MONOTONIC, &end);
         float total_time = time_diff(start, end);  // Total time for one successful command
        append_to_log(output_file, userInput, total_time);
        printf("Processing time: %.5f sec\n", total_time);

        total_cmd_count += 1;
        last_cmd_time = total_time;
        total_time_all += total_time;
        average_time = total_time_all / total_cmd_count;  // Average time for all commands
        update_min_max_time(total_time, &min_time, &max_time);  // Update min and max time

        free(userInput);
        free_args(args);
        args = NULL;
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

        buffer[length++] = (char)c;  // Append the character to the buffer
    }

    // Null-terminate the string
    buffer[length] = '\0';

    // Check if input length exceeds maximum allowed size
    if (length > MAX_INPUT_LENGTH) {
        printf("ERR_MAX_CHAR\n");
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
            free_args(argf);
            argf = NULL;
            free(input_copy);
            exit(1);
        }
        argf = temp;

        // Save a copy of the token
        argf[*count] = strdup(token);
        if (!argf[*count]) {
            perror("Failed to allocate memory for token");
            free(input_copy);
            free_args(argf);
            argf = NULL;
            exit(1);
        }

        (*count)++;
        token = strtok(NULL, delimiter);
    }

    // Null-terminate like a proper argv
    if (argf != NULL) {
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
    }

    free(input_copy);
    return argf;
}

/**
 * Check for multiple consecutive spaces in a string
 * Returns 1 if multiple spaces found, 0 otherwise
 */
int checkMultipleSpaces(const char* input) {
    int i = 0;
    int prevWasSpace = 0;
    int onlySpaces = 1;

    while (input[i] != '\0') {
        if (input[i] != ' ' && input[i] != '\n' && input[i] != '\t') {
            onlySpaces = 0; // Found a real character
        }

        if (input[i] == ' ') {
            if (prevWasSpace && !onlySpaces) {
                // Found multiple consecutive spaces between actual args
                printf("ERR_space\n");
                return 1;
            }
            prevWasSpace = 1;
        } else {
            prevWasSpace = 0;
        }

        i++;
    }

    return 0; // It's fine, let it through
}



/**
 * Free memory allocated for arguments array
 */
void free_args(char **args) {
    if (args != NULL) {  // Ensure the pointer is not NULL
        for (int i = 0; args[i] != NULL; i++) {  // Ensure args[i] is not accessed out of bounds
            free(args[i]);     // Free each string in the array
            args[i] = NULL;    // Set to NULL to prevent accidental access
        }
        free(args);            // Free the array itself
        args = NULL;           // Set the pointer to NULL to avoid further access
    }
}

/**
 * Read lines from a file into a string array and count them in one pass
 * Returns the array of lines and sets num_lines to the count
 */
char** read_file_lines(const char* filename, int* num_lines) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file for reading");
        *num_lines = 0;
        return NULL;
    }

    // Start with a reasonable initial capacity
    int capacity = 64;
    char** lines = malloc(capacity * sizeof(char*));
    if (!lines) {
        perror("Memory allocation failed");
        fclose(file);
        *num_lines = 0;
        return NULL;
    }

    char buffer[MAX_INPUT_LENGTH];
    int count = 0;

    // Read lines and dynamically resize array as needed
    while (fgets(buffer, sizeof(buffer), file)) {
        // If we need more space, double the capacity
        if (count >= capacity - 1) {  // Reserve space for NULL terminator
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                perror("Memory reallocation failed");
                // Free memory allocated so far
                for (int i = 0; i < count; i++) {
                    free(lines[i]);
                }
                free(lines);
                fclose(file);
                *num_lines = 0;
                return NULL;
            }
            lines = new_lines;
        }

        // Remove newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Store the line
        lines[count] = strdup(buffer);
        if (!lines[count]) {
            perror("String duplication failed");
            // Free memory allocated so far
            for (int i = 0; i < count; i++) {
                free(lines[i]);
            }
            free(lines);
            fclose(file);
            *num_lines = 0;
            return NULL;
        }

        count++;
    }

    // Add NULL terminator
    lines[count] = NULL;

    fclose(file);
    *num_lines = count;

    // Optional: Resize array to exact size to save memory
    if (count > 0 && count < capacity - 1) {  // Account for NULL terminator
        char** new_lines = realloc(lines, (count + 1) * sizeof(char*));  // +1 for NULL
        // If realloc fails, we can still use the original array
        if (new_lines) {
            lines = new_lines;
        }
    }

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
            continue;  // Skip if dangerous command couldn't be split properly
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
                    dangerous_cmd_blocked_count += 1;
                    free_args(dangerous_args);
                    return 1;  // Block execution
                }
            }

            // Similar command found (same name but different arguments)
            printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", dangerous_args[0]);
            semi_dangerous_cmd_count += 1;
            free_args(dangerous_args);
            return 0;  // Allow execution with warning
        }

        free_args(dangerous_args);
    }

    return 0;  // No dangerous command found
}

/**
 * Calculate time difference between two timespec structs
 * Returns difference in seconds as a float
 */
float time_diff(struct timespec start, struct timespec end) {
    // Calculate the difference in seconds and nanoseconds
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;

    // If nanoseconds are negative, borrow 1 second
    if (nsec_diff < 0) {
        nsec_diff += 1000000000;  // 1 second = 1 billion nanoseconds
        sec_diff -= 1;
    }

    // Combine the seconds and nanoseconds difference
    float total_time = (float)((double)sec_diff + (double)nsec_diff / 1000000000.0);

    // Return total time in seconds with 5 digits after the decimal
    return total_time;
}

/**
 * Append command and execution time to log file
 */
void append_to_log(const char *filename, char *val1, float val2) {
    FILE *file = fopen(filename, "a");  // Append mode
    if (!file) {
        perror("Error opening log file");
        return;
    }

    fprintf(file, "%s : %.5f sec\n", val1, val2);
    fclose(file);
}

/**
 * Display the shell prompt with statistics
 */
void prompt() {
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>\n",
           total_cmd_count,
           dangerous_cmd_blocked_count,
           last_cmd_time,
           average_time,
           min_time,
           max_time);
}

/**
 * Update minimum and maximum execution times
 */
void update_min_max_time(double current_time, double *min_time, double *max_time) {
    if (*min_time < 0 || current_time < *min_time) {
        *min_time = current_time;
    }

    if (current_time > *max_time) {
        *max_time = current_time;
    }
}

void finish_time_append()
{
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_other = time_diff(start, end);
    append_to_log(output_file, userInput, time_other);
    printf("Processing time: %.5f sec\n", time_other);
}