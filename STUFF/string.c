#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 100

int get_resource_type(const char *res_name) {
    if (strcmp(res_name, "cpu") == 0) return RLIMIT_CPU;
    if (strcmp(res_name, "fsize") == 0) return RLIMIT_FSIZE;
    if (strcmp(res_name, "as") == 0 || strcmp(res_name, "mem") == 0) return RLIMIT_AS;
    if (strcmp(res_name, "nofile") == 0) return RLIMIT_NOFILE;
    return -1;
}

unsigned long long parse_value_with_unit(const char *str) {
    char *endptr;
    double value = strtod(str, &endptr);
    while (isspace(*endptr)) endptr++;

    if (*endptr == '\0') return (unsigned long long)value;
    if (strcasecmp(endptr, "B") == 0) return (unsigned long long)(value);
    if (strcasecmp(endptr, "K") == 0 || strcasecmp(endptr, "KB") == 0)
        return (unsigned long long)(value * 1024);
    if (strcasecmp(endptr, "M") == 0 || strcasecmp(endptr, "MB") == 0)
        return (unsigned long long)(value * 1024 * 1024);
    if (strcasecmp(endptr, "G") == 0 || strcasecmp(endptr, "GB") == 0)
        return (unsigned long long)(value * 1024 * 1024 * 1024);

    fprintf(stderr, "Unknown unit: %s\n", endptr);
    return (unsigned long long)value;
}

char **check_rsc_lmt(char **argu) {
    if (!argu || !argu[0]) return NULL;

    if (strcmp(argu[0], "rlimit") != 0 || !argu[1] || strcmp(argu[1], "set") != 0) {
        return argu;  // Not a rlimit set command → return args unchanged
    }

    int i = 2;
    for (; argu[i]; i++) {
        if (!strchr(argu[i], '=')) break;

        char resource[MAX_INPUT_LENGTH];
        char soft_str[MAX_INPUT_LENGTH], hard_str[MAX_INPUT_LENGTH] = "";

        if (sscanf(argu[i], "%[^=]=%[^:]:%s", resource, soft_str, hard_str) < 2) {
            printf("ERR_FORMAT in: %s\n", argu[i]);
            return NULL;
        }

        rlim_t soft = parse_value_with_unit(soft_str);
        rlim_t hard = strlen(hard_str) ? parse_value_with_unit(hard_str) : soft;

        int rtype = get_resource_type(resource);
        if (rtype == -1) {
            printf("ERR_RESOURCE in: %s\n", resource);
            return NULL;
        }

        struct rlimit lim = { .rlim_cur = soft, .rlim_max = hard };
        if (setrlimit(rtype, &lim) != 0) {
            perror("setrlimit");
            return NULL;
        }

        printf("✓ Resource %-8s → soft: %llu, hard: %llu\n", resource, (unsigned long long)soft, (unsigned long long)hard);
    }

    // Create a new array for the remaining arguments
    char **new_args = malloc((i + 1) * sizeof(char *));
    if (!new_args) {
        perror("malloc failed");
        return NULL;
    }

    int j = 0;
    for (; argu[i]; i++, j++) {
        new_args[j] = argu[i];
    }
    new_args[j] = NULL;  // Null-terminate the array

    return new_args;  // Return the new array with the remaining arguments
}

int main() {
    char *args[] = {
            "rlimit", "set",
            "cpu=2:4", "mem=20M", "nofile=3:4", "echo", "hello", NULL
    };

    pid_t pid = fork();
    if (pid == 0) {
        // In child process
        char **cmd_args = check_rsc_lmt(args);
        if (cmd_args == NULL) {
            exit(1);  // If error, exit the child process
        }

        printf("Executing command with applied limits...\n");

        // Execute the command
        execvp(cmd_args[0], cmd_args);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        // In parent process
        wait(NULL);  // Wait for the child to finish
    } else {
        perror("fork failed");
        return 1;
    }

    return 0;
}
