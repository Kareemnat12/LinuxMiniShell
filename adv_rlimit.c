/**
 * Advanced Shell Implementation with Error Redirection and Resource Limits
 *
 * This shell supports:
 * - Command execution with pipes
 * - Error redirection with 2> operator
 * - Resource limits management (CPU, memory, file size, nofile, nproc)
 * - Resource limits display with rlimit show
 * - Custom commands
 * - Tracking of dangerous commands
 * - Performance statistics
 */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <bits/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <asm-generic/errno-base.h>
#include <errno.h>

/**************************** CONSTANTS ****************************/
const int MAX_INPUT_LENGTH = 1024;   // Maximum length for user input
const int MAX_ARGC = 7;              // Maximum number of command arguments allowed
const char delim[] = " ";            // Delimiter for splitting command strings
#define MAX_INPUT_LENGTHH 1025       // Fixed buffer size for input

/********************** FUNCTION PROTOTYPES **********************/
// Input handling
void get_string(char* buffer, size_t buffer_size);
char** split_to_args(const char *string, const char *delimiter, int *count);
int checkMultipleSpaces(const char* input);
char* trim_inplace(char* str);
void free_args(char **args);
int pipe_split(char *input, char *left_cmd, char *right_cmd);

// File operations
char** read_file_lines(const char* filename, int* num_lines);
void append_to_log(const char *filename, char* val1, float val2);
void write_to_file(const char *filename, const char *content, int append);

// Command processing
int is_dangerous_command(char **user_args, int user_args_len);
float time_diff(struct timespec start, struct timespec end);
void update_min_max_time(double current_time, double *min_time, double *max_time);
void prompt(void);
void check_append_flag(char **args, int args_len, int *append_flg);
void redirect_stderr_to_file(const char *filename);
void check_and_redirect_stderr(char **l_args);

// Signal handlers
void sigchld_handler(int sig);
void sigxcpu_handler(int sig);
void sigxfsz_handler(int sig);

// Resource limiting
int get_resource_type(const char *res_name);
unsigned long long parse_value_with_unit(const char *str);
char **check_rsc_lmt(char **argu, int *args_len);
void show_resource_limit(const char *name, int resource_type);
void show_all_resource_limits(void);

// Error handling
void handle_execvp_errors_in_child(char **args);
void* safe_malloc(size_t size);
void restore_stderr(void);
// Custom commands
int my_tee_handler(void);

// Debug helpers
void print_array(const char **array, int size);

/********************** CUSTOM COMMANDS STRUCTURE **********************/
typedef struct {
    const char *name;          // Command name
    int (*handler)(void);      // Function pointer for command handler
    int requires_pipe;         // Whether command requires pipe input
    int supports_append;       // Whether command supports append flag
    int min_args;              // Minimum number of arguments (excluding command name)
} CustomCommand;

// Define custom commands table
CustomCommand custom_commands[] = {
    {"my_tee", my_tee_handler, 1, 1, 1}, // my_tee requires pipe, supports append, needs at least 1 arg for output file
    {NULL, NULL, 0, 0, 0}                // Terminator
};

/********************** GLOBAL VARIABLES **********************/
// Command handling
char **Danger_CMD = NULL;     // List of dangerous commands loaded from file
int l_args_len = 0;           // Number of arguments in current command
int r_args_len = 0;           // Number of arguments in right command (after pipe)
int numLines = 0;             // Number of dangerous commands in the file
char **l_args = NULL;         // Current command arguments array for left command
char **r_args = NULL;         // Current command arguments array for right command

struct timespec start, end;   // Timestamps for timing command execution
int flag_semi_dangerous = 0;  // Flag for semi-dangerous commands

// Statistics tracking
int total_cmd_count = 0;              // Total number of successful commands
int dangerous_cmd_blocked_count = 0;  // Number of dangerous commands blocked
double last_cmd_time = 0;             // Time taken for the last successful command
double average_time = 0;              // Average execution time for all commands
double total_time_all = 0;            // Running sum of execution times
double min_time = 0;                  // Minimum command execution time
double max_time = 0;                  // Maximum command execution time
int semi_dangerous_cmd_count = 0;     // Number of similar-but-allowed commands

// Pipe and command state
int pip_flag = 0;             // Flag for pipe existence
int pipefd[2];                // For pipe communication
int append_flg = 0;           // Flag for append mode
int left_status;              // Exit status of left command
int right_status;             // Exit status of right command
char userInput[MAX_INPUT_LENGTHH]; // Buffer for user input
int background_flag = 0;      // Flag for background execution
char current_command[MAX_INPUT_LENGTHH]; // Current command string for logging
const char *output_file = NULL;  // Path to output log file
int original_stderr_fd = -1;
int stderr_redirected = 0;
/**
 * Safe memory allocation with error handling
 */
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        exit(1);
    }
    return ptr;
}

/**
 * Error handling function for child processes when exec fails
 */
void handle_execvp_errors_in_child(char **args) {
    if (!args || !args[0]) {
        fprintf(stderr, "ERR\n");
        exit(1);
    }

    // Try to execute the command
    execvp(args[0], args);

    // If execvp returns, there was an error
    if (errno == EMFILE) {
        fprintf(stderr, "Too many open files!\n");
    } else if (errno == ENOMEM) {
        fprintf(stderr, "Memory allocation failed!\n"); 
    } else {
        perror("exec failed");
    }
    exit(127);  // Standard exit code for command not found/exec error
}

/**
 * Handler for SIGXCPU signal (CPU time limit exceeded)
 */
void sigxcpu_handler(int sig) {
    fprintf(stderr, "CPU time limit exceeded!\n");
    exit(1); // Ensure the process terminates
}

/**
 * Handler for SIGXFSZ signal (File size limit exceeded)
 */
void sigxfsz_handler(int sig) {
    fprintf(stderr, "File size limit exceeded!\n");
    exit(1); // Ensure the process terminates
}

/********************** HELPER FUNCTIONS **********************/
/**
 * Find a custom command by name
 *
 * @param cmd_name Name of the command to find
 * @return Pointer to command structure or NULL if not found
 */
const CustomCommand* find_custom_command(const char *cmd_name) {
    if (!cmd_name) return NULL;

    for (int i = 0; custom_commands[i].name != NULL; i++) {
        if (strcmp(custom_commands[i].name, cmd_name) == 0) {
            return &custom_commands[i];
        }
    }
    return NULL;
}

/**
 * Debug helper: Print contents of string array
 */
void print_array(const char **array, int size) {
    printf("//////DEBUG????????[");
    for (int i = 0; i < size; i++) {
        printf("\"%s\"", array[i]);
        if (i < size - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

/**
 * Handler for SIGCHLD signal
 * Updates statistics when child processes terminate
 */
void sigchld_handler(int sig) {
    int cmd_succeeded;

    if (pip_flag) {
        cmd_succeeded = WIFEXITED(left_status) && WEXITSTATUS(left_status) == 0 &&
                        WIFEXITED(right_status) && WEXITSTATUS(right_status) == 0;
    } else {
        cmd_succeeded = WIFEXITED(left_status) && WEXITSTATUS(left_status) == 0;
    }

    if (cmd_succeeded) {
        clock_gettime(CLOCK_MONOTONIC, &end);
        float total_time = time_diff(start, end);
    last_cmd_time = total_time;
    total_time_all += total_time;
    average_time = total_time_all / total_cmd_count;
    update_min_max_time(total_time, &min_time, &max_time);

        total_cmd_count += 1;
        append_to_log(output_file, current_command, total_time);
    } else {
        // Check for signal termination
        if (pip_flag) {
            if (WIFSIGNALED(left_status)) {
                printf("Terminated by signal: SIG%s\n", strsignal(WTERMSIG(left_status)));
            } else if (!WIFEXITED(left_status) || WEXITSTATUS(left_status) != 0) {
                printf("Process exited with error code: %d\n", WEXITSTATUS(left_status));
            } else if (WIFSIGNALED(right_status)) {
                printf("Terminated by signal: SIG%s\n", strsignal(WTERMSIG(right_status)));
            } else {
                printf("Process exited with error code: %d\n", WEXITSTATUS(right_status));
            }
        } else {
            if (WIFSIGNALED(left_status)) {
                printf("Terminated by signal: SIG%s\n", strsignal(WTERMSIG(left_status)));
            } else {
                printf("Process exited with error code: %d\n", WEXITSTATUS(left_status));
            }
        }

        if (flag_semi_dangerous) {
            semi_dangerous_cmd_count -= 1;
            flag_semi_dangerous = 0;
        }
    }
}

/********************** ERROR REDIRECTION FUNCTIONS **********************/
/**
 * Redirect stderr to a file
 * Opens the specified file and redirects STDERR_FILENO to it
 */
void redirect_stderr_to_file(const char *filename) {
    // Save original stderr if we haven't already
    if (original_stderr_fd == -1) {
        original_stderr_fd = dup(STDERR_FILENO);
    }

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open for stderr redirection");
        return;
    }

    if (dup2(fd, STDERR_FILENO) < 0) {
        perror("dup2 for stderr redirection");
        close(fd);
        return;
    }

    close(fd);
    stderr_redirected = 1;
}



void restore_stderr(void) {
    if (stderr_redirected && original_stderr_fd != -1) {
        dup2(original_stderr_fd, STDERR_FILENO);
        close(original_stderr_fd);
        original_stderr_fd = -1;
        stderr_redirected = 0;
    }
}
/**
 * Check command arguments for 2> redirection and handle it
 * Searches for the 2> operator and redirects stderr if found
 */
void check_and_redirect_stderr(char **args) {
    if (!args) return;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "2>") == 0 && args[i+1] != NULL) {
            // Redirect stderr to file
            redirect_stderr_to_file(args[i+1]);

            // Remove the redirection operator and filename from arguments
            // by shifting all subsequent arguments left
            int j;
            for (j = i; args[j+2] != NULL; j++) {
                args[j] = args[j+2];
            }
            args[j] = NULL; // Properly terminate the array
            break;
        }
    }
}

/**
 * Check if the input contains the -a (append) flag
 */
void check_append_flag(char **args, int args_len, int *flg) {
    // Initialize flag to 0
    *flg = 0;

    if (args == NULL) return;

    // Check each argument for exact match with "-a"
    for (int i = 0; i < args_len && args[i]; i++) {
        if (strcmp(args[i], "-a") == 0) {
            *flg = 1;
            break;
        }
    }
}

/**
 * Write content to a file, either appending or truncating
 */
void write_to_file(const char *filename, const char *content, int append) {
    int fd;

    // Open the file based on the append flag
    if (append) {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (fd == -1) {
        perror("open failed");
        return;
    }

    // Write content to the file
    if (write(fd, content, strlen(content)) == -1) {
        perror("write failed");
    }

    // Close the file
    close(fd);
}

/********************** RESOURCE MANAGEMENT FUNCTIONS **********************/
/**
 * Get the resource ID for a named resource
 */
int get_resource_type(const char *res_name) {
    if (strcmp(res_name, "cpu") == 0) return RLIMIT_CPU;
    if (strcmp(res_name, "fsize") == 0) return RLIMIT_FSIZE;
    if (strcmp(res_name, "as") == 0 || strcmp(res_name, "mem") == 0) return RLIMIT_AS;
    if (strcmp(res_name, "nofile") == 0) return RLIMIT_NOFILE;
    if (strcmp(res_name, "nproc") == 0) return RLIMIT_NPROC;
    return -1;
}

/**
 * Display a resource limit in a human-readable format
 */
void show_resource_limit(const char *name, int resource_type) {
    struct rlimit limit;
    if (getrlimit(resource_type, &limit) != 0) {
        perror("getrlimit");
        return;
    }
    
    const char* res_name;
    if (resource_type == RLIMIT_CPU) res_name = "CPU time";
    else if (resource_type == RLIMIT_AS) res_name = "Memory";
    else if (resource_type == RLIMIT_FSIZE) res_name = "File size";
    else if (resource_type == RLIMIT_NOFILE) res_name = "Open files";
    else if (resource_type == RLIMIT_NPROC) res_name = "Process count";
    else res_name = name;
    
    printf("%s: ", res_name);
    
    // Format soft limit
    if (limit.rlim_cur == RLIM_INFINITY) {
        printf("soft=unlimited, ");
    } else {
        if (resource_type == RLIMIT_CPU) {
            printf("soft=%lus, ", (unsigned long)limit.rlim_cur);
        } else if (resource_type == RLIMIT_AS || resource_type == RLIMIT_FSIZE) {
            // Format memory-related resources in human-readable form
            if (limit.rlim_cur >= 1024*1024*1024) {
                printf("soft=%.1fG, ", (double)limit.rlim_cur / (1024*1024*1024));
            } else if (limit.rlim_cur >= 1024*1024) {
                printf("soft=%.1fM, ", (double)limit.rlim_cur / (1024*1024));
            } else if (limit.rlim_cur >= 1024) {
                printf("soft=%.1fK, ", (double)limit.rlim_cur / 1024);
            } else {
                printf("soft=%luB, ", (unsigned long)limit.rlim_cur);
            }
        } else {
            printf("soft=%lu, ", (unsigned long)limit.rlim_cur);
        }
    }
    
    // Format hard limit
    if (limit.rlim_max == RLIM_INFINITY) {
        printf("hard=unlimited\n");
    } else {
        if (resource_type == RLIMIT_CPU) {
            printf("hard=%lus\n", (unsigned long)limit.rlim_max);
        } else if (resource_type == RLIMIT_AS || resource_type == RLIMIT_FSIZE) {
            // Format memory-related resources in human-readable form
            if (limit.rlim_max >= 1024*1024*1024) {
                printf("hard=%.1fG\n", (double)limit.rlim_max / (1024*1024*1024));
            } else if (limit.rlim_max >= 1024*1024) {
                printf("hard=%.1fM\n", (double)limit.rlim_max / (1024*1024));
            } else if (limit.rlim_max >= 1024) {
                printf("hard=%.1fK\n", (double)limit.rlim_max / 1024);
            } else {
                printf("hard=%luB\n", (unsigned long)limit.rlim_max);
            }
        } else {
            printf("hard=%lu\n", (unsigned long)limit.rlim_max);
        }
    }
}

/**
 * Show all resource limits
 */
void show_all_resource_limits(void) {
    show_resource_limit("cpu", RLIMIT_CPU);
    show_resource_limit("mem", RLIMIT_AS);
    show_resource_limit("fsize", RLIMIT_FSIZE);
    show_resource_limit("nofile", RLIMIT_NOFILE);
    show_resource_limit("nproc", RLIMIT_NPROC);
}

/**
 * Parse a value with optional unit (B, K/KB, M/MB, G/GB)
 * Returns the value in bytes
 */
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

/**
 * Check and apply resource limits specified in command arguments
 * Also handles the rlimit show command
 */
 /*
char **check_rsc_lmt(char **argu) {
    if (!argu || !argu[0]) return NULL;

    if (strcmp(argu[0], "rlimit") != 0) {
        return argu;  // Not a rlimit command at all
    }

    // Handle 'rlimit show' command
    if (argu[1] && strcmp(argu[1], "show") == 0) {
        if (argu[2] == NULL) {
            // Show all resource limits when no specific resource is mentioned
            show_all_resource_limits();
        } else {
            // Show limit for a specific resource
            int rtype = get_resource_type(argu[2]);
            if (rtype == -1) {
                printf("ERR_RESOURCE: Unknown resource '%s'\n", argu[2]);
            } else {
                show_resource_limit(argu[2], rtype);
            }
        }

        // Return empty command array to indicate we've handled the command
        char **empty_cmd = safe_malloc(sizeof(char*));
        empty_cmd[0] = NULL;
        return empty_cmd;
    }

    // Handle 'rlimit set' command
    if (!argu[1] || strcmp(argu[1], "set") != 0) {
        printf("ERR: Unknown rlimit command. Use 'rlimit set' or 'rlimit show'\n");
        return NULL;
    }

    // Set up signal handlers for resource limit violations
    signal(SIGXCPU, sigxcpu_handler);  // CPU time limit
    signal(SIGXFSZ, sigxfsz_handler);  // File size limit

    // Allocate new_args array here to avoid potential memory leak
    char **new_args = NULL;
    int i = 2;

    for (; argu[i]; i++) {
        if (!strchr(argu[i], '=')) break;

        char resource[MAX_INPUT_LENGTH];
        char soft_str[MAX_INPUT_LENGTH], hard_str[MAX_INPUT_LENGTH];

        // Initialize hard_str to empty to handle the case when only soft limit is provided
        hard_str[0] = '\0';

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

        // Set the resource limit
        struct rlimit lim = { .rlim_cur = soft, .rlim_max = hard };
        if (setrlimit(rtype, &lim) != 0) {
            // Specific error handling based on errno
            switch (errno) {
                case EPERM:
                    printf("ERR: Permission denied setting %s limit\n", resource);
                    break;
                case EINVAL:
                    printf("ERR: Invalid value for %s limit\n", resource);
                    break;
                default:
                    perror("setrlimit");
            }
            return NULL;
        }

        // Print confirmation with appropriate units based on the resource type
        const char* unit = "";
        unsigned long long soft_display = soft;
        unsigned long long hard_display = hard;

        if (rtype == RLIMIT_FSIZE || rtype == RLIMIT_AS) {
            // Display memory/file sizes in human-readable format
            if (soft >= 1024*1024*1024) {
                soft_display = soft / (1024*1024*1024);
                unit = "GB";
            } else if (soft >= 1024*1024) {
                soft_display = soft / (1024*1024);
                unit = "MB";
            } else if (soft >= 1024) {
                soft_display = soft / 1024;
                unit = "KB";
            } else {
                unit = "B";
            }

            printf("✓ Resource %-8s → soft: %llu %s, hard: ",
                   resource, soft_display, unit);

            // Handle hard limit display with appropriate units
            if (hard >= 1024*1024*1024) {
                hard_display = hard / (1024*1024*1024);
                unit = "GB";
            } else if (hard >= 1024*1024) {
                hard_display = hard / (1024*1024);
                unit = "MB";
            } else if (hard >= 1024) {
                hard_display = hard / 1024;
                unit = "KB";
            } else {
                unit = "B";
            }
            printf("%llu %s\n", hard_display, unit);
        } else if (rtype == RLIMIT_CPU) {
            // CPU time is displayed in seconds
            printf("✓ Resource %-8s → soft: %llu sec, hard: %llu sec\n",
                   resource, soft_display, hard_display);
        } else {
            // Other resources like nofile/nproc don't need special unit handling
            printf("✓ Resource %-8s → soft: %llu, hard: %llu\n",
                   resource, soft_display, hard_display);
        }
    }

    // Create a new array for the remaining arguments
    new_args = safe_malloc((i + 1) * sizeof(char *));

    int j = 0;
    for (; argu[i]; i++, j++) {
        new_args[j] = argu[i];
    }
    new_args[j] = NULL;  // Null-terminate the array
    return new_args;  // Return the new array with the remaining arguments
}
*/
 char **check_rsc_lmt(char **argu, int *args_len) {
     // Basic validation
     if (!argu || !argu[0]) {
         fprintf(stderr, "[DEBUG] check_rsc_lmt: NULL or empty arguments\n");
         return NULL;
     }

     fprintf(stderr, "[DEBUG] check_rsc_lmt called with command: %s\n", argu[0]);

     // Check if this is a rlimit command
     if (strcmp(argu[0], "rlimit") != 0) {
         fprintf(stderr, "[DEBUG] Not a rlimit command, passing through\n");
         return argu;  // Not a rlimit command
     }

     // Handle 'rlimit show' command
     if (argu[1] && strcmp(argu[1], "show") == 0) {
         fprintf(stderr, "[DEBUG] Processing 'rlimit show' command\n");
         show_all_resource_limits();
         return NULL;
     }

     // Handle 'rlimit set' command
     if (!argu[1] || strcmp(argu[1], "set") != 0) {
         fprintf(stderr, "[DEBUG] Unknown rlimit subcommand: %s\n", argu[1] ? argu[1] : "NULL");
         return NULL;
     }

     fprintf(stderr, "[DEBUG] Processing 'rlimit set' command\n");

     // Process resource limit settings
     int i = 2;
     for (; argu[i]; i++) {
         if (!strchr(argu[i], '=')) break;

         fprintf(stderr, "[DEBUG] Processing limit: %s\n", argu[i]);

         char resource[MAX_INPUT_LENGTH];
         char soft_str[MAX_INPUT_LENGTHH], hard_str[MAX_INPUT_LENGTHH] = "";

         if (sscanf(argu[i], "%[^=]=%[^:]:%s", resource, soft_str, hard_str) < 2) {
             fprintf(stderr, "[DEBUG] Format error in: %s\n", argu[i]);
             return NULL;
         }

         rlim_t soft = parse_value_with_unit(soft_str);
         rlim_t hard = strlen(hard_str) ? parse_value_with_unit(hard_str) : soft;

         fprintf(stderr, "[DEBUG] Resource: %s, soft: %llu, hard: %llu\n",
                 resource, (unsigned long long)soft, (unsigned long long)hard);

         int rtype = get_resource_type(resource);
         if (rtype == -1) {
             fprintf(stderr, "[DEBUG] Unknown resource: %s\n", resource);
             return NULL;
         }

         // Set the new limit
         struct rlimit lim = { .rlim_cur = soft, .rlim_max = hard };
         if (setrlimit(rtype, &lim) != 0) {
             fprintf(stderr, "[DEBUG] setrlimit failed for %s: %s (errno: %d)\n",
                     resource, strerror(errno), errno);
             return NULL;
         }

         // For CPU limits, make sure to install signal handler
         if (rtype == RLIMIT_CPU) {
             fprintf(stderr, "[DEBUG] Installing SIGXCPU handler for CPU limit\n");
             struct sigaction sa;
             sa.sa_handler = sigxcpu_handler;
             sigemptyset(&sa.sa_mask);
             sa.sa_flags = 0;
             if (sigaction(SIGXCPU, &sa, NULL) != 0) {
                 fprintf(stderr, "[DEBUG] Failed to set SIGXCPU handler: %s\n", strerror(errno));
             } else {
                 fprintf(stderr, "[DEBUG] SIGXCPU handler installed successfully\n");
             }
         }
     }

     // Create new args array
     fprintf(stderr, "[DEBUG] Creating new args array, starting at index %d\n", i);
     if (!argu[i]) {
         fprintf(stderr, "[DEBUG] No command after rlimit set\n");
         return NULL;
     }

     // Count remaining arguments
     int remaining_args = 0;
     for (int j = i; argu[j] != NULL; j++) {
         remaining_args++;
     }

     // Update the args length
     if (args_len) {
         *args_len = remaining_args;
         fprintf(stderr, "[DEBUG] Updating args_len to %d\n", remaining_args);
     }

     char **new_args = malloc((remaining_args + 1) * sizeof(char *));
     if (!new_args) {
         fprintf(stderr, "[DEBUG] malloc failed for new args\n");
         return NULL;
     }

     int j = 0;
     for (; argu[i]; i++, j++) {
         new_args[j] = argu[i];
         fprintf(stderr, "[DEBUG] Added arg[%d]: %s\n", j, argu[i]);
     }
     new_args[j] = NULL;

     fprintf(stderr, "[DEBUG] Returning new args array with %d elements\n", remaining_args);
     return new_args;
 }
/********************** CUSTOM COMMAND IMPLEMENTATIONS **********************/
/**
 * Implementation of my_tee command handler
 *
 * Reads from pipe, outputs to stdout, and writes to specified files
 */
int my_tee_handler(void) {
    // Close write end (only reading)
    close(pipefd[1]);

    // Buffer for reading from pipe
    int bytes;
    char buffer[MAX_INPUT_LENGTHH];
    char result[MAX_INPUT_LENGTHH]; // Initialize result buffer
    result[0] = '\0';             // Initialize as empty string

    // Read all data from pipe
    while ((bytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        strcat(result, buffer); // Append to result string
    }

    // Close read end after we're done
    close(pipefd[0]);

    // Output to stdout
    write(STDOUT_FILENO, result, strlen(result));

    // Write to all specified files (skip the first arg which is the command name)
    for (int i = 1; i < r_args_len; i++) {
        write_to_file(r_args[i], result, append_flg);
    }

    return 0;
}

/********************** INPUT HANDLING FUNCTIONS **********************/
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
        fprintf(stderr, "Memory allocation failed!\n");
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
            fprintf(stderr, "Memory allocation failed!\n");
            free_args(argf);
            free(input_copy);
            exit(1);
        }
        argf = temp;

        // Store a copy of the token
        argf[*count] = strdup(token);
        if (!argf[*count]) {
            fprintf(stderr, "Memory allocation failed!\n");
            free(input_copy);
            free_args(argf);
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
 * Split input string into left and right commands based on pipe symbol
 *
 * Parses the input string and separates it into two commands if a pipe exists.
 * Returns 1 if pipe exists, 0 otherwise
 */
int pipe_split(char *input, char *left_cmd, char *right_cmd) {
    // Make a copy of the input string to avoid modifying the original
    char *input_copy = strdup(input);
    if (!input_copy) {
        return -1; // Memory allocation failed
    }

    char *token = strtok(input_copy, "|");

    if (token) {
        strcpy(left_cmd, token);

        token = strtok(NULL, "|");
        if (token) {
            strcpy(right_cmd, token);
            free(input_copy); // Free the allocated memory
            return 1; // pipe exists
        } else {
            right_cmd[0] = '\0';
            free(input_copy); // Free the allocated memory
            return 0; // no second command after pipe
        }
    } else {
        strcpy(left_cmd, input);
        right_cmd[0] = '\0';
        free(input_copy); // Free the allocated memory
        return 0; // no pipe at all
    }
}

/********************** FILE AND COMMAND PROCESSING **********************/
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
    char** lines = safe_malloc(capacity * sizeof(char*));

    char buffer[MAX_INPUT_LENGTH];
    int count = 0;

    // Read each line from the file
    while (fgets(buffer, sizeof(buffer), file)) {
        // Expand the array if needed
        if (count >= capacity - 1) {  // Leave room for NULL terminator
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                fprintf(stderr, "Memory allocation failed!\n");
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
            fprintf(stderr, "Memory allocation failed!\n");
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
    // Return early if no arguments to check
    if (user_args == NULL || user_args_len == 0) {
        return 0;
    }

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
void append_to_log(const char *filename, char* val1, float val2) {
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
void prompt(void) {
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

/********************** MAIN FUNCTION **********************/
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
    output_file = argv[2];  // Log file for command execution times
    const char *input_file = argv[1];   // File containing dangerous commands
    char left_cmd[MAX_INPUT_LENGTH];
    char right_cmd[MAX_INPUT_LENGTH];
    pid_t right_pid = 0;

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

    // Set up signal handlers
    signal(SIGCHLD, sigchld_handler);
    signal(SIGXCPU, sigxcpu_handler);
    signal(SIGXFSZ, sigxfsz_handler);

    // Main command processing loop
    while (1) {
        // Display the prompt with current statistics
        prompt();

        // Get user input
        get_string(userInput, sizeof(userInput));
        clock_gettime(CLOCK_MONOTONIC, &start);  // Start timing the command

        // Skip processing if input is empty
        if (userInput[0] == '\0') {
            continue;
        }
        strcpy(current_command, userInput);
        // Clean up the input by removing extra whitespace
        trim_inplace(userInput);

        // Check for multiple consecutive spaces (not allowed)
        int spaceCheck = checkMultipleSpaces(userInput);
        if (spaceCheck == 1) {
            continue;
        }

        ///////////////////// COMMAND PARSING /////////////////////
        // Split input into left and right commands if pipe exists
        pip_flag = pipe_split(userInput, left_cmd, right_cmd);
        trim_inplace(left_cmd);
        trim_inplace(right_cmd);

        // Split commands into arguments
        l_args = split_to_args(left_cmd, delim, &l_args_len);
        r_args = split_to_args(right_cmd, delim, &r_args_len);

        // Skip if too many arguments
        if (l_args == NULL || (r_args == NULL && pip_flag)) {
            free_args(l_args);
            free_args(r_args);
            continue;
        }

        // Check for resource limits in the parent process
        if (l_args && l_args[0] && strcmp(l_args[0], "rlimit") == 0) {
            char **new_cmd = check_rsc_lmt(l_args, &l_args_len);
            if (new_cmd != NULL) {
                free_args(l_args);
                l_args = new_cmd;
            } else {
                free_args(l_args);
                free_args(r_args);
                l_args = NULL;
                r_args = NULL;
                continue;  // Skip if rlimit command was handled or invalid
            }
        }

        // Also check right command for resource limits if we have a pipe
        if (pip_flag && r_args && r_args[0] && strcmp(r_args[0], "rlimit") == 0) {
            char **new_cmd = check_rsc_lmt(r_args, &r_args_len);
            if (new_cmd != NULL) {
                free_args(r_args);
                r_args = new_cmd;
            } else {
                free_args(l_args);
                free_args(r_args);
                l_args = NULL;
                r_args = NULL;
                continue;  // Skip if rlimit command was handled or invalid
            }
        }
        if(l_args_len > MAX_ARGC || r_args_len > MAX_ARGC) {
            printf("ERR_ARGS\n");
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;  // Skip if too many arguments
        }

        ///////////////////// COMMAND SECURITY CHECK /////////////////////
        // Check if commands are dangerous - check both left and right commands
        if (is_dangerous_command(l_args, l_args_len)) {
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;  // Skip execution of dangerous command
        }

        if (is_dangerous_command(r_args, r_args_len)) {
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            continue;  // Skip execution of dangerous command
        }

        // Check if command has background flag
        if (l_args_len > 0 && strcmp(l_args[l_args_len - 1], "&") == 0) {
            background_flag = 1;
            l_args[l_args_len - 1] = NULL; // Remove "&"
            l_args_len--;                  // Decrease arg count
        }

        // Handle the exit command
        if (strcmp(l_args[0], "done") == 0) {
            free_args(l_args);
            free_args(r_args);
            free_args(Danger_CMD);
            l_args = NULL;
            r_args = NULL;
            Danger_CMD = NULL;
            printf("%d\n", dangerous_cmd_blocked_count + semi_dangerous_cmd_count);
            exit(0);
        }



        ///////////////////// COMMAND EXECUTION /////////////////////
        // Create pipe for command communication
        if (pipe(pipefd) == -1) {
            perror("pipe");
            exit(1);
        }

        // Execute left command
        const pid_t left_pid = fork();
        if (left_pid < 0) {
            // Fork failed
            if (errno == EAGAIN) {
                fprintf(stderr, "Process creation limit exceeded!\n");
            } else {
                perror("Fork Failed");
            }
            free_args(l_args);
            free_args(r_args);
            l_args = NULL;
            r_args = NULL;
            return 1;
        }

        if (left_pid == 0) {
            check_and_redirect_stderr(l_args);

            // Set up signal handlers in the child process
//            struct sigaction sa;
//
//            // CPU limit handler
//            sa.sa_handler = sigxcpu_handler;
//            sigemptyset(&sa.sa_mask);
//            sa.sa_flags = 0;
//            sigaction(SIGXCPU, &sa, NULL);
//
//            // File size limit handler
//            sa.sa_handler = sigxfsz_handler;
//            sigemptyset(&sa.sa_mask);
//            sa.sa_flags = 0;
//            sigaction(SIGXFSZ, &sa, NULL);

            char **cmd = check_rsc_lmt(l_args,&l_args_len);  // Check for resource limits
            if (cmd != NULL) {
                l_args = cmd;
            }
            // Child process for left command
            if (pip_flag == 0) {
                // No pipe, just execute the command
                handle_execvp_errors_in_child(l_args);
                // We never reach here if exec succeeds
            }

            // Set up pipe for output redirection
            if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                if (errno == EMFILE) {
                    fprintf(stderr, "Too many open files!\n");
                    exit(1);
                }
                perror("dup2");
                exit(1);
            }
            
            close(pipefd[0]);                // Close unused read end of pipe
            close(pipefd[1]);                // Close write end of pipe after dup2
            handle_execvp_errors_in_child(l_args);
        }

        // Execute right command if pipe exists
        if (pip_flag) {
            check_append_flag(r_args,r_args_len, &append_flg);

            // Check if right command is a custom command
            if (r_args && r_args[0]) {
                const CustomCommand* cmd = find_custom_command(r_args[0]);

                if (cmd != NULL) {
                    // It's a custom command
                    // Validate minimum arguments requirement
                    if (r_args_len - 1 < cmd->min_args) { // -1 because first arg is the command itself
                        printf("ERR: Not enough arguments for %s\n", cmd->name);
                    } else {
                        // Execute the custom command handler
                        cmd->handler();
                    }
                } else {
                    // Standard pipe to external command
                    right_pid = fork();
                    if (right_pid < 0) {
                        // Fork failed
                        if (errno == EAGAIN) {
                            fprintf(stderr, "Process creation limit exceeded!\n");
                        } else {
                            perror("Fork Failed");
                        }
                        free_args(l_args);
                        free_args(r_args);
                        l_args = NULL;
                        r_args = NULL;
                        return 1;
                    }

                    if (right_pid == 0) {
                        // Set up signal handlers in the child process
//                        struct sigaction sa;
//
//                        // CPU limit handler
//                        sa.sa_handler = sigxcpu_handler;
//                        sigemptyset(&sa.sa_mask);
//                        sa.sa_flags = 0;
//                        sigaction(SIGXCPU, &sa, NULL);
//
//                        // File size limit handler
//                        sa.sa_handler = sigxfsz_handler;
//                        sigemptyset(&sa.sa_mask);
//                        sa.sa_flags = 0;
//                        sigaction(SIGXFSZ, &sa, NULL);

                        char **cmd2 = check_rsc_lmt(r_args,&r_args_len);  // Check for resource limits
                        if (cmd2 != NULL) {
                            r_args = cmd2;
                        }
                        // Child process for right command
                        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                            if (errno == EMFILE) {
                                fprintf(stderr, "Too many open files!\n");
                                exit(1);
                            }
                            perror("dup2");
                            exit(1);
                        }
                        
                        close(pipefd[1]);               // Close unused write end of pipe
                        close(pipefd[0]);               // Close read end of pipe after dup2
                        handle_execvp_errors_in_child(r_args);
                    }
                }
            }
        }

        // Parent process waits for child processes to complete
        close(pipefd[0]);
        close(pipefd[1]);

        if (pip_flag) {
            // Wait for both processes if pipe was used
            waitpid(left_pid, &left_status, 0);
            if (right_pid > 0) { // Only wait if we forked a right process
                waitpid(right_pid, &right_status, 0);
            }
        } else {
            // Wait only for left process if no pipe
            if (!background_flag) // if not background process
                waitpid(left_pid, &left_status, 0);

            background_flag = 0; // Reset background flag for next command
        }

        // Clean up argument arrays
        free_args(l_args);
        free_args(r_args);
        l_args = NULL;
        r_args = NULL;
    }
}