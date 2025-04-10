#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bits/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>

/* Constants */
const int MAX_INPUT_LENGTH = 1024; // Maximum input length
const int MAX_ARGC = 6;           // Maximum number of arguments
const char delim[] = " ";         // Delimiter for splitting commands
const int MONITOR = 2;            // Set to 2 to enable BOTH file and console output

/* Function prototypes */
char* get_string();
char** split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* str);
void free_args(char **args);
int count_lines(const char* filename);
char** read_file_lines(const char* filename, int* num_lines);
int is_dangerous_command(char **user_args, int user_args_len);
double time_diff(struct timespec start, struct timespec end);
void log_message(const char* format, ...);
char* get_current_time_string();
void cleanup(void);

/* Global variables */
char **Danger_CMD;   // Dangerous commands from the file
int args_len;        // Length of splitted string from the user input
int numLines = 0;    // Number of lines in the dangerous commands file
char **args;         // Arguments array
struct timespec start, end;  // For time measurement
FILE* log_file;      // Log file pointer

/**
 * Logging function for monitoring activities
 */
void log_message(const char* format, ...) {
    if (!MONITOR) return;

    // Get current time
    char* time_str = get_current_time_string();

    // Open log file if not already open
    if (!log_file) {
        log_file = fopen("shell_monitor.log", "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file\n");
            free(time_str);
            return;
        }
    }

    // Print timestamp to log file
    fprintf(log_file, "[%s] ", time_str);

    // Print formatted message to log file
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);

    // Also print to console if MONITOR >= 2
    if (MONITOR >= 2) {
        printf("\033[36m[MONITOR %s]\033[0m ", time_str);  // Cyan color for monitoring

        va_list args2;
        va_start(args2, format);
        vprintf(format, args2);
        va_end(args2);

        fflush(stdout);  // Make sure it's displayed immediately
    }

    free(time_str);
}

/**
 * Get current time as formatted string
 */
char* get_current_time_string() {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    char* time_str = malloc(26);
    if (time_str) {
        strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    }
    return time_str;
}

/**
 * Main function - implements a simple shell
 */
int main(int argc, char* argv[]) {
    // Register cleanup function
    atexit(cleanup);

    printf("\033[32m--- Shell Monitor Activated ---\033[0m\n");  // Green text

    log_message("Shell starting up - PID: %d, User: %s\n", getpid(), getenv("USER"));
    log_message("Initializing shell environment\n");

    // Read dangerous commands from file
    log_message("Loading dangerous commands file: ./f.txt\n");
    Danger_CMD = read_file_lines("./f.txt", &numLines);
    log_message("Loaded %d dangerous commands\n", numLines);

    // Process user commands in an infinite loop
    log_message("Entering command processing loop\n");
    while (1) {
        printf("kareem@kareemTest~$ ");
        log_message("Waiting for user input\n");

        char *userInput = get_string();
        log_message("Received input: %s\n", userInput ? userInput : "NULL");

        clock_gettime(CLOCK_MONOTONIC, &start);
        log_message("Started command timer\n");

        // Handle input exceeding maximum length
        if (userInput == NULL) {
            log_message("Input rejected: exceeded maximum length or allocation failed\n");
            free(userInput);
            continue;
        }

        // Check for multiple spaces in input
        log_message("Checking for multiple spaces\n");
        int spaceCheck = checkMultipleSpaces(userInput);
        if (spaceCheck == 1) {
            log_message("Input rejected: contains multiple consecutive spaces\n");
            free(userInput);
            continue;
        }
        log_message("Space check passed\n");

        // Split input into arguments
        log_message("Splitting input into arguments\n");
        args = split_to_args(userInput, delim, &args_len);

        // Handle too many arguments
        if (args == NULL) {
            log_message("Input rejected: too many arguments (max %d)\n", MAX_ARGC);
            free_args(args);
            continue;
        }
        log_message("Arguments successfully parsed, count: %d\n", args_len);

        // Log all arguments
        for (int i = 0; i < args_len; i++) {
            log_message("Arg[%d]: %s\n", i, args[i]);
        }

        // Check for dangerous commands
        log_message("Checking if command is dangerous\n");
        if (is_dangerous_command(args, args_len)) {
            log_message("Command rejected: dangerous command detected\n");
            continue; // Skip execution of dangerous command
        }
        log_message("Command passed danger check\n");

        // Check for exit command
        if (strcmp(args[0], "done") == 0) {
            log_message("Exit command received, terminating shell\n");
            exit(0);
        }

        // Create child process to execute command
        log_message("Creating child process to execute command\n");
        const pid_t pid = fork();

        if (pid < 0) {
            /* Error occurred */
            log_message("Fork failed with error code: %d\n", errno);
            fprintf(stderr, "Fork Failed");
            free_args(args);
            return 1;
        }

        if (pid == 0) {
            /* Child process */
            log_message("Child process started with PID: %d\n", getpid());
            log_message("Executing command: %s\n", args[0]);
            execvp(args[0], args);
            log_message("execvp failed: %s\n", strerror(errno));
            perror("execvp failed");
            free_args(args);
            return 1; // If execvp returns, an error occurred
        }

        // Parent process waits for child to complete
        log_message("Parent process waiting for child (PID: %d) to complete\n", pid);
        int status;
        pid_t wait_result = waitpid(pid, &status, 0);
        log_message("Child process completed with status: %d\n", status);

        // Calculate and display execution time
        clock_gettime(CLOCK_MONOTONIC, &end);
        double total_time = time_diff(start, end);
        log_message("Command execution completed in %.5f seconds\n", total_time);

        free_args(args);
        log_message("Arguments memory freed\n");

        printf("time taken: %.5f seconds\n", total_time);
    }
}

/**
 * Gets user input with dynamic memory allocation
 * Returns NULL if input exceeds MAX_INPUT_LENGTH
 */
char* get_string() {
    log_message("Entered get_string()\n");

    size_t buffer_size = MAX_INPUT_LENGTH;  // Start with an initial buffer size
    size_t length = 0;                      // Keep track of the current input length

    log_message("Allocating initial buffer of size %zu\n", buffer_size);
    char *buffer = malloc(buffer_size);

    if (!buffer) {
        log_message("ERROR: Failed to allocate memory for input buffer\n");
        perror("Failed to allocate memory");
        return NULL;
    }

    int c;  // Variable to read each character
    log_message("Reading input characters\n");

    while ((c = getchar()) != EOF && c != '\n') {
        if (length + 1 >= buffer_size) {
            buffer_size *= 2;  // Double the buffer size as needed
            log_message("Resizing buffer to %zu bytes\n", buffer_size);

            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                log_message("ERROR: Failed to reallocate memory for input buffer\n");
                free(buffer);
                perror("Failed to reallocate memory");
                return NULL;
            }
            buffer = new_buffer;
            log_message("Buffer successfully resized\n");
        }

        buffer[length++] = c;  // Append the character to the buffer
    }

    // Null-terminate the string
    buffer[length] = '\0';
    log_message("Input read complete, length: %zu characters\n", length);

    // Check if input length exceeds maximum allowed size
    if (length > MAX_INPUT_LENGTH) {
        log_message("ERROR: Input exceeded maximum length of %d characters\n", MAX_INPUT_LENGTH);
        printf("Input exceeds the maximum allowed length of %d characters\n", MAX_INPUT_LENGTH);
        free(buffer);
        return NULL;
    }

    log_message("Exiting get_string() with valid input\n");
    return buffer;
}

/**
 * Split string into arguments array (argv style)
 * Returns NULL if the number of arguments exceeds MAX_ARGC
 */
char **split_to_args(const char *string, const char *delimiter, int *count) {
    log_message("Entered split_to_args() with string: \"%s\", delimiter: \"%s\"\n", string, delimiter);

    log_message("Duplicating input string\n");
    char *input_copy = strdup(string);  // Copy input to avoid modifying original
    if (!input_copy) {
        log_message("ERROR: Failed to duplicate input string\n");
        perror("Failed to allocate memory");
        exit(1);
    }

    char **argf = NULL;
    *count = 0;

    log_message("Starting tokenization of string\n");
    char *token = strtok(input_copy, delimiter);
    while (token != NULL) {
        // Expand the argv array to hold another string + null
        log_message("Processing token: \"%s\"\n", token);
        char **temp = realloc(argf, (*count + 2) * sizeof(char *));
        if (!temp) {
            log_message("ERROR: Failed to reallocate memory for arguments array\n");
            perror("Failed to reallocate memory");
            free(input_copy);
            exit(1);
        }
        argf = temp;

        // Save a copy of the token
        argf[*count] = strdup(token);
        if (!argf[*count]) {
            log_message("ERROR: Failed to duplicate token\n");
            perror("Failed to allocate memory for token");
            free(input_copy);
            exit(1);
        }

        (*count)++;
        log_message("Token added to args array at position %d\n", *count - 1);
        token = strtok(NULL, delimiter);
    }

    // Null-terminate like a proper argv
    argf[*count] = NULL;
    log_message("Tokenization complete, found %d arguments\n", *count);

    // Handle maximum number of arguments error
    if (*count - 1 > MAX_ARGC) {
        log_message("ERROR: Too many arguments (max: %d, found: %d)\n", MAX_ARGC, *count - 1);
        printf("ERR_ARGS\n");
        for (int i = 0; i < *count; i++) {
            free(argf[i]);
        }
        free(argf);
        free(input_copy);
        return NULL;
    }

    free(input_copy);
    log_message("Exiting split_to_args() with %d arguments\n", *count);
    return argf;
}

/**
 * Check for multiple consecutive spaces in a string
 * Returns 1 if multiple spaces found, 0 otherwise
 */
int checkMultipleSpaces(const char* str) {
    log_message("Entered checkMultipleSpaces() with string: \"%s\"\n", str);

    int spaceCount = 0;

    while (*str != '\0') {
        if (*str == ' ') {
            spaceCount++;
            log_message("Space character found, current count: %d\n", spaceCount);
            if (spaceCount > 1) {
                log_message("ERROR: Multiple consecutive spaces detected\n");
                printf("ERR_SPC\n");
                return 1;
            }
        } else {
            spaceCount = 0; // Reset on non-space
        }
        str++;
    }

    log_message("Exiting checkMultipleSpaces() with result: 0 (no multiple spaces)\n");
    return 0; // No multiple spaces found
}

/**
 * Free memory allocated for arguments array
 */
void free_args(char **args) {
    log_message("Entered free_args()\n");

    if (args == NULL) {
        log_message("Arguments array is NULL, nothing to free\n");
        return;
    }

    int count = 0;
    for (int i = 0; args[i] != NULL; i++) {
        log_message("Freeing argument at index %d\n", i);
        free(args[i]);
        count++;
    }

    free(args);
    log_message("Freed args array with %d elements\n", count);
    log_message("Exiting free_args()\n");
}

/**
 * Count lines in a file
 */
int count_lines(const char* filename) {
    log_message("Entered count_lines() for file: %s\n", filename);

    FILE* file = fopen(filename, "r");
    if (!file) {
        log_message("ERROR: Failed to open file for counting: %s\n", strerror(errno));
        perror("Error opening file for counting");
        return -1;
    }

    int count = 0;
    char buffer[MAX_INPUT_LENGTH];

    log_message("Counting lines in file\n");
    while (fgets(buffer, sizeof(buffer), file)) {
        count++;
    }

    fclose(file);
    log_message("File contains %d lines\n", count);
    log_message("Exiting count_lines() with result: %d\n", count);
    return count;
}

/**
 * Read lines from a file into a string array
 */
char** read_file_lines(const char* filename, int* num_lines) {
    log_message("Entered read_file_lines() for file: %s\n", filename);

    *num_lines = count_lines(filename);
    if (*num_lines <= 0) {
        log_message("File is empty or could not be read\n");
        return NULL;
    }

    log_message("Opening file for reading\n");
    FILE* file = fopen(filename, "r");
    if (!file) {
        log_message("ERROR: Failed to open file for reading: %s\n", strerror(errno));
        perror("Error opening file for reading");
        return NULL;
    }

    log_message("Allocating memory for %d lines\n", *num_lines);
    char** lines = malloc((*num_lines) * sizeof(char*));
    if (!lines) {
        log_message("ERROR: Failed to allocate memory for lines array\n");
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    char buffer[MAX_INPUT_LENGTH];
    int i = 0;

    log_message("Reading lines from file\n");
    while (fgets(buffer, sizeof(buffer), file) && i < *num_lines) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
        lines[i] = strdup(buffer); // Store line
        log_message("Read line %d: \"%s\"\n", i, buffer);
        i++;
    }

    fclose(file);
    log_message("Successfully read %d lines from file\n", i);
    log_message("Exiting read_file_lines()\n");
    return lines;
}

/**
 * Check if command is in the list of dangerous commands
 * Returns 1 if dangerous, 0 otherwise
 */
int is_dangerous_command(char **user_args, int user_args_len) {
    log_message("Entered is_dangerous_command()\n");
    log_message("Checking command \"%s\" against %d dangerous commands\n", user_args[0], numLines);

    for (int i = 0; i < numLines; i++) {
        log_message("Comparing with dangerous command %d: \"%s\"\n", i, Danger_CMD[i]);

        int temp_count;
        char **dangerous_args = split_to_args(Danger_CMD[i], delim, &temp_count);

        if (dangerous_args == NULL) {
            log_message("Failed to parse dangerous command %d, skipping\n", i);
            continue; // Skip if dangerous command couldn't be split properly
        }

        // Compare the command name (first argument)
        log_message("Comparing command names: \"%s\" vs \"%s\"\n", user_args[0], dangerous_args[0]);
        if (strcmp(user_args[0], dangerous_args[0]) == 0) {
            log_message("Command name match found\n");

            // Exact match check (same command and arguments)
            if (user_args_len == temp_count) {
                log_message("Argument count matches, checking for exact match\n");

                int exact_match = 1;
                for (int j = 0; j < user_args_len; j++) {
                    log_message("Comparing arg %d: \"%s\" vs \"%s\"\n", j, user_args[j], dangerous_args[j]);
                    if (strcmp(user_args[j], dangerous_args[j]) != 0) {
                        exact_match = 0;
                        log_message("Argument mismatch at position %d\n", j);
                        break;
                    }
                }

                if (exact_match) {
                    log_message("EXACT MATCH found with dangerous command\n");
                    printf("\033[31mERR: Dangerous command detected (\"%s\"). Execution prevented.\033[0m\n", user_args[0]);
                    free_args(dangerous_args);
                    log_message("Exiting is_dangerous_command() with result: 1 (dangerous)\n");
                    return 1; // Block execution
                }
            }

            // Similar command found (same name but different arguments)
            log_message("Similar command found (same name, different args)\n");
            printf("\033[33mWARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\033[0m\n", dangerous_args[0]);
            free_args(dangerous_args);
            log_message("Exiting is_dangerous_command() with result: 0 (caution)\n");
            return 0; // Allow execution with warning
        }

        free_args(dangerous_args);
    }

    log_message("No dangerous command match found\n");
    log_message("Exiting is_dangerous_command() with result: 0 (safe)\n");
    return 0; // No dangerous command found
}

/**
 * Calculate time difference between two timespec structs
 * Returns difference in seconds as a double
 */
double time_diff(struct timespec start, struct timespec end) {
    log_message("Entered time_diff()\n");
    log_message("Start time: %ld.%09ld\n", start.tv_sec, start.tv_nsec);
    log_message("End time: %ld.%09ld\n", end.tv_sec, end.tv_nsec);

    // Calculate the difference in seconds and nanoseconds
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;

    // If nanoseconds are negative, borrow 1 second
    if (nsec_diff < 0) {
        nsec_diff += 1000000000; // 1 second = 1 billion nanoseconds
        sec_diff -= 1;
        log_message("Nanosecond adjustment applied\n");
    }

    // Combine the seconds and nanoseconds difference
    double total_time = sec_diff + nsec_diff / 1000000000.0;
    log_message("Calculated time difference: %.5f seconds\n", total_time);

    log_message("Exiting time_diff() with result: %.5f\n", total_time);
    // Return total time in seconds with 5 digits after the decimal
    return total_time;
}

/**
 * Cleanup function to be called on program exit
 */
void cleanup(void) {
    if (log_file) {
        log_message("Shell shutting down\n");
        fclose(log_file);
    }
    printf("\033[32m--- Shell Monitor Deactivated ---\033[0m\n");  // Green text
}