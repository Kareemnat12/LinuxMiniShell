#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

const int MAX_INPUT_LENGTH = 1024;
const int MAX_ARGC = 6;

char* get_string();
char **split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* str);
void free_args(char **args);
int count_lines(const char* filename);
char** read_file_lines(const char* filename, int* num_lines);
int is_dangerous_command(char **user_args, int user_args_len);
double time_diff(struct timespec start, struct timespec end);

char **Danger_CMD;
const char delim[] = " ";
int args_len;
int numLines = 0;
char **args;
struct timespec start, end;

int main(int argc, char* argv[]) {
    Danger_CMD = read_file_lines("./f.txt", &numLines);

    while (1) {
        printf("kareem@kareemTest~$ ");
        char *userInput = get_string();
        clock_gettime(CLOCK_MONOTONIC, &start);

        if (userInput == NULL) {
            free(userInput);
            continue;
        }

        if (checkMultipleSpaces(userInput)) {
            free(userInput);
            continue;
        }

        args = split_to_args(userInput, delim, &args_len);

        if (args == NULL) {
            free_args(args);
            continue;
        }

        if (is_dangerous_command(args, args_len)) {
            continue;
        }

        if (strcmp(args[0], "done") == 0) {
            exit(0);
        }

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Fork Failed\n");
            free_args(args);
            return 1;
        }

        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp failed");
            free_args(args);
            return 1;
        }

        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        free_args(args);

        double total_time = time_diff(start, end);
        printf("time taken: %.5f seconds\n", total_time);
    }
}

char* get_string() {
    size_t buffer_size = MAX_INPUT_LENGTH;
    size_t length = 0;
    char *buffer = malloc(buffer_size);

    if (!buffer) {
        perror("Failed to allocate memory");
        return NULL;
    }

    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        if (length + 1 >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                perror("Failed to reallocate memory");
                return NULL;
            }
            buffer = new_buffer;
        }
        buffer[length++] = c;
    }

    buffer[length] = '\0';

    if (length > MAX_INPUT_LENGTH) {
        printf("Input exceeds the maximum allowed length of %d characters\n", MAX_INPUT_LENGTH);
        free(buffer);
        return NULL;
    }

    return buffer;
}

char **split_to_args(const char *string, const char *delimiter, int *count) {
    char *input_copy = strdup(string);
    if (!input_copy) {
        perror("Failed to allocate memory");
        exit(1);
    }

    char **argf = NULL;
    *count = 0;

    char *token = strtok(input_copy, delimiter);
    while (token != NULL) {
        char **temp = realloc(argf, (*count + 2) * sizeof(char *));
        if (!temp) {
            perror("Failed to reallocate memory");
            free(input_copy);
            exit(1);
        }
        argf = temp;

        argf[*count] = strdup(token);
        if (!argf[*count]) {
            perror("Failed to allocate memory for token");
            free(input_copy);
            exit(1);
        }

        (*count)++;
        token = strtok(NULL, delimiter);
    }

    argf[*count] = NULL;

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
            spaceCount = 0;
        }
        str++;
    }
    return 0;
}

void free_args(char **args) {
    if (args == NULL) return;
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

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

char** read_file_lines(const char* filename, int* num_lines) {
    *num_lines = count_lines(filename);
    if (*num_lines <= 0) return NULL;

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
        buffer[strcspn(buffer, "\n")] = '\0';
        lines[i++] = strdup(buffer);
    }

    fclose(file);
    return lines;
}
