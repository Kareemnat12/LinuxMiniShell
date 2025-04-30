#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <bits/time.h>
#include <sys/wait.h>

/* ---- Constants ---- */
const int MAX_INPUT_LENGTH = 1024;  // Maximum length for user input
const int MAX_ARGC = 6;             // Maximum number of command arguments allowed
const char delim[] = " ";           // Delimiter for splitting command strings

/* ---- Function prototypes ---- */
// Input handling
void get_string(char* buffer, size_t buffer_size);
char** split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* input);
char* trim_inplace(char* str);
void free_args(char **args);

// File operations
char** read_file_lines(const char* filename, int* num_lines);
void append_to_log(const char *filename, char* val1, float val2);

// Command processing
int is_dangerous_command(char **user_args, int user_args_len);
float time_diff(struct timespec start, struct timespec end);
void update_min_max_time(double current_time, double *min_time, double *max_time);
void prompt();

/* ---- Global variables ---- */
// Command handling
char **Danger_CMD;              // List of dangerous commands loaded from file
int args_len;                   // Number of arguments in current command
int numLines = 0;               // Number of dangerous commands in the file
char **l_args;                  // Current command arguments array
struct timespec start, end;     // Timestamps for timing command execution
int flag_semi_dangerous = 0;    // Flag for semi-dangerous commands

// Statistics tracking
int total_cmd_count = 0;               // Total number of successful commands
int dangerous_cmd_blocked_count = 0;   // Number of dangerous commands blocked
double last_cmd_time = 0;              // Time taken for the last successful command
double average_time = 0;               // Average execution time for all commands
double total_time_all = 0;             // Running sum of execution times
double min_time = 0;                   // Minimum command execution time
double max_time = 0;                   // Maximum command execution time
int semi_dangerous_cmd_count = 0;      // Number of similar-but-allowed commands

/**
 * Main function - Our simple shell implementation
 * Reads dangerous commands from a file, processes user input,
 * blocks dangerous commands, and logs execution times.
 */
int main(int argc, char* argv[]) {
    // Ensure we have the required command line arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dangerous_commands_file> <log_file>\n", argv[0]);
        exit(1);
    }

    // Setup file paths from command line arguments
    const char *output_file = argv[2];  // Log file for command execution times
    const char *input_file = argv[1];   // File containing dangerous commands

    // Load the list of dangerous commands from file
    Danger_CMD = read_file_lines(input_file, &numLines);
    if (Danger_CMD == NULL) {
        fprintf(stderr, "Failed to load dangerous commands\n");
        exit(1);
    }

    // Clear the log file at the start of each session
    {
        FILE *clear = fopen(argv[2], "w");
        if (clear) fclose(clear);
    }

    // Main command processing loop
    while (1) {
        // Display the prompt with current statistics
        prompt();

        // Static buffer for user input
        char userInput[MAX_INPUT_LENGTH + 1];

        // Get user input
        get_string(userInput, sizeof(userInput));
        clock_gettime(CLOCK_MONOTONIC, &start);  // Start timing the command

        // Skip processing if input is empty
        if (userInput[0] == '\0') {
            continue;
        }

        // Clean up the input by removing extra whitespace
        trim_inplace(userInput);

        // Check for multiple consecutive spaces (not allowed)
        int spaceCheck = checkMultipleSpaces(userInput);
        if (spaceCheck == 1) {
            continue;
        }

        // Split the input into command and arguments
        l_args = split_to_args(userInput, delim, &args_len);

        // Skip if too many arguments
        if (l_args == NULL) {
            free_args(l_args);
            l_args = NULL;
            continue;
        }

        // Check if this is a dangerous command we should block
        if (is_dangerous_command(l_args, args_len)) {
            free_args(l_args);
            l_args = NULL;
            continue;  // Skip execution of dangerous command
        }

        // Handle the exit command
        if (strcmp(l_args[0], "done") == 0) {
            free_args(l_args);
            l_args = NULL;
            free_args(Danger_CMD);
            Danger_CMD = NULL;
            printf("%d\n", dangerous_cmd_blocked_count + semi_dangerous_cmd_count);
            exit(0);
        }

        // Fork a child process to execute the command
        const pid_t pid = fork();

        if (pid < 0) {
            // Fork failed
            perror("Fork Failed");
            free_args(l_args);
            l_args = NULL;
            return 1;
        }

        if (pid == 0) {
            // Child process - execute the command
            execvp(l_args[0], l_args);
            perror("execvp failed");
            exit(127);  // Exit with error code 127 if execvp fails
        }

        // Parent process waits for child to complete
        int status;
        waitpid(pid, &status, 0);

        // If command was successful, update statistics and log
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            clock_gettime(CLOCK_MONOTONIC, &end);
            float total_time = time_diff(start, end);
            append_to_log(output_file, userInput, total_time);

            // Update all time statistics
            last_cmd_time = total_time;
            total_time_all += total_time;
            total_cmd_count += 1;
            average_time = total_time_all / total_cmd_count;
            update_min_max_time(total_time, &min_time, &max_time);

            free_args(l_args);
            l_args = NULL;
        }
        else {
            // Command failed
            if (flag_semi_dangerous) {
                semi_dangerous_cmd_count -= 1;
                flag_semi_dangerous = 0;  // Reset the flag
            }
            free_args(l_args);
            l_args = NULL;
        }
    }
}

/**
 * Gets user input using a static buffer
 *
 * Reads characters into a provided buffer.
 * Ensures input doesn't exceed buffer size.
 */
void get_string(char* buffer, size_t buffer_size) {
    // Clear the buffer
    buffer[0] = '\0';

    // Read characters until newline or EOF
    size_t length = 0;
    int c;

    while ((c = getchar()) != EOF && c != '\n') {
        // Check if we still have space in the buffer (save one for null terminator)
        if (length < buffer_size - 1) {
            buffer[length++] = (char)c;
        } else {
            // Buffer overflow - discard remaining input
            while ((c = getchar()) != EOF && c != '\n');
            printf("ERR\n");
            buffer[0] = '\0';  // Clear the buffer
            return;
        }
    }

    // Null-terminate the string
    buffer[length] = '\0';

    // Check if input exceeds max allowed length
    if (length > MAX_INPUT_LENGTH) {
        printf("ERR\n");
        buffer[0] = '\0';  // Clear the buffer
    }
}

/**
 * Split string into an array of arguments (argv style)
 *
 * Takes a command string and splits it at the delimiter.
 * Returns NULL if the number of arguments exceeds MAX_ARGC
 */
char **split_to_args(const char *string, const char *delimiter, int *count) {
    // Make a copy to avoid modifying the original string
    char *input_copy = strdup(string);
    if (!input_copy) {
        perror("Failed to allocate memory");
        exit(1);
    }

    char **argf = NULL;
    *count = 0;

    // Tokenize the string
    char *token = strtok(input_copy, delimiter);
    while (token != NULL) {
        // Expand the arguments array to hold another string + null terminator
        char **temp = realloc(argf, (*count + 2) * sizeof(char *));
        if (!temp) {
            perror("Failed to reallocate memory");
            free_args(argf);
            argf = NULL;
            free(input_copy);
            exit(1);
        }
        argf = temp;

        // Store a copy of the token
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

    // Null-terminate the array like a proper argv
    if (argf != NULL) {
        argf[*count] = NULL;

        // Check if we exceeded the maximum number of arguments
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
 *
 * We don't allow multiple spaces between command arguments.
 * Returns 1 if multiple spaces found, 0 otherwise
 */
int checkMultipleSpaces(const char* input) {
    int i = 0;
    int prevWasSpace = 0;
    int onlySpaces = 1;  // Assume string is only spaces until we find otherwise

    while (input[i] != '\0') {
        // Check if we found a non-whitespace character
        if (input[i] != ' ' && input[i] != '\n' && input[i] != '\t') {
            onlySpaces = 0;  // Found a real character
        }

        if (input[i] == ' ') {
            if (prevWasSpace && !onlySpaces) {
                // Found multiple consecutive spaces between actual args
                printf("ERR_SPACE\n");
                return 1;
            }
            prevWasSpace = 1;
        } else {
            prevWasSpace = 0;
        }

        i++;
    }

    return 0;
}

/**
 * Free memory allocated for arguments array
 *
 * Properly cleans up the memory for our args array
 * to prevent memory leaks.
 */
void free_args(char **args) {
    if (args != NULL) {
        for (int i = 0; args[i] != NULL; i++) {
            free(args[i]);     // Free each string in the array
            args[i] = NULL;    // Prevent accidental reuse
        }
        free(args);            // Free the array itself
        args = NULL;           // Prevent accidental reuse
    }
}

/**
 * Read lines from a file into a string array
 *
 * Used to load the dangerous commands list.
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

    // Read each line from the file
    while (fgets(buffer, sizeof(buffer), file)) {
        // Expand the array if needed
        if (count >= capacity - 1) {  // Leave room for NULL terminator
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                perror("Memory reallocation failed");
                // Clean up allocated memory
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

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Store a copy of the line
        lines[count] = strdup(buffer);
        if (!lines[count]) {
            perror("String duplication failed");
            // Clean up allocated memory
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

    // Add NULL terminator to the array
    lines[count] = NULL;

    fclose(file);
    *num_lines = count;

    // Resize array to exact size to save memory
    if (count > 0 && count < capacity - 1) {
        char** new_lines = realloc(lines, (count + 1) * sizeof(char*));
        // If realloc fails, we can still use the original array
        if (new_lines) {
            lines = new_lines;
        }
    }

    return lines;
}

/**
 * Check if a command is in the list of dangerous commands
 *
 * Compares the user's command with our dangerous commands list.
 * Returns 1 if dangerous (should be blocked), 0 otherwise
 */
int is_dangerous_command(char **user_args, int user_args_len) {
    int found_similar = 0;
    char *similar_command = NULL;  // Store the similar dangerous command

    // Check against each dangerous command
    for (int i = 0; i < numLines; i++) {
        int temp_count;
        char **dangerous_args = split_to_args(Danger_CMD[i], delim, &temp_count);

        if (dangerous_args == NULL) {
            continue;  // Skip if dangerous command couldn't be split properly
        }

        // Compare the command name (first argument)
        if (strcmp(user_args[0], dangerous_args[0]) == 0) {
            // Check for exact match (same command and all arguments)
            if (user_args_len == temp_count) {
                int exact_match = 1;
                for (int j = 0; j < user_args_len; j++) {
                    if (strcmp(user_args[j], dangerous_args[j]) != 0) {
                        exact_match = 0;
                        break;
                    }
                }

                if (exact_match) {
                    // Exact match found - block this command
                    printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n", Danger_CMD[i]);
                    dangerous_cmd_blocked_count += 1;
                    free_args(dangerous_args);
                    return 1;  // Block execution
                }
            }

            // Track that we found a similar command
            found_similar = 1;
            similar_command = Danger_CMD[i];  // Store reference to the similar command
        }

        free_args(dangerous_args);
    }

    // If we found similar commands but no exact match
    if (found_similar && similar_command != NULL) {
        // We only print one warning even if multiple similar commands were found
        printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", similar_command);
        semi_dangerous_cmd_count += 1;
        flag_semi_dangerous = 1;  // Set flag for semi-dangerous command
    }

    return 0;  // Allow execution (with warning if similar command found)
}

/**
 * Calculate time difference between two timespec structs
 *
 * Used to measure command execution time.
 * Returns difference in seconds as a float
 */
float time_diff(struct timespec start, struct timespec end) {
    // Calculate the difference in seconds and nanoseconds
    long sec_diff = end.tv_sec - start.tv_sec;
    long nsec_diff = end.tv_nsec - start.tv_nsec;

    // Handle nanosecond underflow by borrowing a second
    if (nsec_diff < 0) {
        nsec_diff += 1000000000;  // 1 second = 1 billion nanoseconds
        sec_diff -= 1;
    }

    // Combine seconds and nanoseconds for total time
    float total_time = (float)((double)sec_diff + (double)nsec_diff / 1000000000.0);

    return total_time;
}

/**
 * Append command and execution time to log file
 *
 * Records each successful command and its execution time
 * for later analysis.
 */
void append_to_log(const char *filename, char *val1, float val2) {
    FILE *file = fopen(filename, "a");  // Open in append mode
    if (!file) {
        perror("Error opening log file");
        return;
    }

    // Write the command and its execution time
    fprintf(file, "%s : %.5f sec\n", val1, val2);
    fclose(file);
}

/**
 * Display the shell prompt with current statistics
 *
 * Shows a detailed prompt with execution statistics
 * to help users track shell performance.
 */
void prompt() {
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%.5f|avg_time:%.5f|min_time:%.5f|max_time:%.5f>>",
           total_cmd_count,
           dangerous_cmd_blocked_count,
           last_cmd_time,
           average_time,
           min_time,
           max_time);
    fflush(stdout);  // Ensure prompt appears immediately
}

/**
 * Update minimum and maximum execution times
 *
 * Keeps track of the fastest and slowest commands
 * for the statistics display.
 */
void update_min_max_time(double current_time, double *min_time, double *max_time) {
    // Update minimum time (first command or new minimum)
    if (*min_time <= 0 || current_time < *min_time) {
        *min_time = current_time;
    }

    // Update maximum time
    if (current_time > *max_time) {
        *max_time = current_time;
    }
}

/**
 * Trims leading and trailing whitespace from a string in-place
 *
 * Modifies the string directly rather than creating a new one.
 * Returns pointer to the same string memory that was passed in.
 */
char* trim_inplace(char* str) {
    if (str == NULL) {
        return NULL;
    }

    // Handle empty string
    if (*str == '\0') {
        return str;
    }

    // Skip leading whitespace
    char* start = str;
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }

    // If string is all whitespace, make it empty
    if (*start == '\0') {
        *str = '\0';
        return str;
    }

    // Find the last non-whitespace character
    char* end = str + strlen(str) - 1;
    while (end > start && (*end == ' ' || *end == '\t')) {
        end--;
    }

    // Null-terminate after the last non-whitespace character
    end[1] = '\0';

    // If there was leading whitespace, shift everything left
    if (start != str) {
        size_t len = (end - start) + 1;
        memmove(str, start, len + 1);  // +1 for null terminator
    }

    return str;
}