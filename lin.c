#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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
int pipe_split(char *input, char *left_cmd, char *right_cmd);

// File operations
char** read_file_lines(const char* filename, int* num_lines);

// Command processing
int is_dangerous_command(char **user_args, int user_args_len);
void prompt();

/* ---- Global variables ---- */
// Command handling
char **Danger_CMD;              // List of dangerous commands loaded from file
int l_args_len;                 // Number of arguments in current command
int r_args_len;                 // Number of arguments in right command
int numLines = 0;               // Number of dangerous commands in the file
char **l_args;                  // Current command arguments array for left command
char **r_args;                  // Current command arguments array for right command

/**
 * Main function - Our simple shell implementation
 * Reads dangerous commands from a file, processes user input,
 * and blocks dangerous commands.
 */
int main(int argc, char* argv[]) {
    // Ensure we have the required command line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dangerous_commands_file>\n", argv[0]);
        exit(1);
    }

    // Setup file path from command line arguments
    const char *input_file = argv[1];   // File containing dangerous commands
    char left_cmd[MAX_INPUT_LENGTH];
    char right_cmd[MAX_INPUT_LENGTH];
    int pipefd[2]; // for pipe stuff
    pid_t right_pid = 0;

    // Load the list of dangerous commands from file
    Danger_CMD = read_file_lines(input_file, &numLines);
    if (Danger_CMD == NULL) {
        fprintf(stderr, "Failed to load dangerous commands\n");
        exit(1);
    }

    // Main command processing loop
    while (1) {
        // Display the prompt
        prompt();

        // Static buffer for user input
        char userInput[MAX_INPUT_LENGTH + 1];

        // Get user input
        get_string(userInput, sizeof(userInput));

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

        // Split into left and right command
        int pip_flag = pipe_split(userInput, left_cmd, right_cmd);
        trim_inplace(left_cmd);
        trim_inplace(right_cmd);

        // Split the input into command and arguments
        l_args = split_to_args(left_cmd, delim, &l_args_len);
        r_args = split_to_args(right_cmd, delim, &r_args_len);

        // Skip if too many arguments
        if (l_args == NULL || (r_args == NULL && pip_flag)) {
            free_args(l_args);
            free_args(r_args);
            continue;
        }

        // dangerous check for left and right stuff args
        // Check if this is a dangerous command we should block
        if (is_dangerous_command(l_args, l_args_len)) {
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;  // Skip execution of dangerous command
        }

        if (pip_flag && is_dangerous_command(r_args, r_args_len)) {
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;  // Skip execution of dangerous command
        }

        // Handle the exit command
        if (strcmp(l_args[0], "done") == 0) {
            free_args(l_args);
            free_args(r_args);
            free_args(Danger_CMD);
            l_args = NULL;
            r_args = NULL;
            Danger_CMD = NULL;
            exit(0);
        }

        // Setup pipe if needed
        if (pip_flag) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                free_args(l_args);
                free_args(r_args);
                l_args = NULL;
                r_args = NULL;
                continue;
            }
        }

        // Fork a child process to execute the left command
        const pid_t left_pid = fork();

        if (left_pid < 0) {
            // Fork failed
            perror("Fork Failed");
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;
        }

        if (left_pid == 0) {
            // Child process - execute the left command
            if (pip_flag == 0){
                execvp(l_args[0], l_args);
                perror("execvp failed");
                exit(127);  // Exit with error code 127 if execvp fails
            }

            // If using pipe, redirect stdout to the pipe
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to write end of pipe
            close(pipefd[0]);               // Close unused read end of pipe
            close(pipefd[1]);               // Close write end of pipe after dup2
            execvp(l_args[0], l_args);
            perror("execvp failed");
            exit(127);  // Exit with error code 127 if execvp fails
        }

        // If we have a pipe, create a second process for the right command
        if (pip_flag) {
            right_pid = fork();
            if (right_pid < 0) {
                // Fork failed
                perror("Fork Failed");
                free_args(l_args);
                free_args(r_args);
                l_args = NULL;
                r_args = NULL;
                close(pipefd[0]);
                close(pipefd[1]);
                waitpid(left_pid, NULL, 0);  // Clean up the left process
                continue;
            }

            if (right_pid == 0) {
                // Child process - execute the right command
                dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to read end of pipe
                close(pipefd[1]);               // Close unused write end of pipe
                close(pipefd[0]);               // Close read end of pipe after dup2
                execvp(r_args[0], r_args);
                perror("execvp failed");
                exit(127);  // Exit with error code 127 if execvp fails
            }
        }

        // Parent process waits for child processes to complete
        if (pip_flag) {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(left_pid, NULL, 0);
            waitpid(right_pid, NULL, 0);
        } else {
            waitpid(left_pid, NULL, 0);
        }

        // Free argument memory
        free_args(l_args);
        free_args(r_args);
        l_args = NULL;
        r_args = NULL;
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
    if (user_args == NULL || user_args[0] == NULL) {
        return 0;  // No command to check
    }

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
                    free_args(dangerous_args);
                    return 1;  // Block execution
                }
            }

            // Print warning for similar commands
            printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", Danger_CMD[i]);
        }

        free_args(dangerous_args);
    }

    return 0;  // Allow execution
}

/**
 * Display the shell prompt
 *
 * Shows a simple prompt for the shell.
 */
void prompt() {
    printf("kareem@kareem ~$ ");
    fflush(stdout);  // Ensure prompt appears immediately
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

/**
 * Split input by pipe character
 *
 * @param input The input string to split
 * @param left_cmd Buffer to store the left command
 * @param right_cmd Buffer to store the right command
 * @return 1 if pipe exists, 0 otherwise
 */
int pipe_split(char *input, char *left_cmd, char *right_cmd) {
    char *delimiter = strchr(input, '|');

    if (delimiter) {
        // Calculate length of left part
        size_t leftLen = delimiter - input;
        if (leftLen >= MAX_INPUT_LENGTH) {
            leftLen = MAX_INPUT_LENGTH - 1; // Leave room for null terminator
        }

        // Copy left part
        strncpy(left_cmd, input, leftLen);
        left_cmd[leftLen] = '\0';

        // Copy right part
        strncpy(right_cmd, delimiter + 1, MAX_INPUT_LENGTH - 1);
        right_cmd[MAX_INPUT_LENGTH - 1] = '\0'; // Ensure null termination

        return 1; // Pipe exists
    } else {
        // No pipe found, copy entire input to left
        strncpy(left_cmd, input, MAX_INPUT_LENGTH - 1);
        left_cmd[MAX_INPUT_LENGTH - 1] = '\0'; // Ensure null termination

        // Right string remains empty
        right_cmd[0] = '\0';

        return 0; // No pipe
    }
}