#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

// Configuration
#define SHELL_PATH "./ex1"
#define DANGEROUS_COMMANDS_PATH "dangerous_commands.txt"
#define EXEC_TIMES_LOG_PATH "exec_times.log"
#define TIMEOUT_SECONDS 5

// Test utilities
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define RESET "\033[0m"

#define PASS(msg) printf("%s✅ PASS: %s%s\n", GREEN, msg, RESET)
#define FAIL(msg) printf("%s❌ FAIL: %s%s\n", RED, msg, RESET)
#define INFO(msg) printf("%s📝 INFO: %s%s\n", BLUE, msg, RESET)
#define WARN(msg) printf("%sⓘ WARNING: %s%s\n", YELLOW, msg, RESET)

// Struct to store shell process info
typedef struct {
    pid_t pid;
    int stdin_pipe[2];  // To write to shell
    int stdout_pipe[2]; // To read from shell
    int stderr_pipe[2]; // To read stderr from shell
} ShellProcess;

// Test case structure
typedef struct {
    const char *name;
    void (*test_func)(void);
} TestCase;

// Global shell process
ShellProcess shell;

// Create dangerous commands file with specified content
void create_dangerous_commands_file(const char *content) {
    FILE *fp = fopen(DANGEROUS_COMMANDS_PATH, "w");
    if (!fp) {
        perror("Failed to create dangerous commands file");
        exit(EXIT_FAILURE);
    }

    fprintf(fp, "%s", content);
    fclose(fp);
}

// Start shell process
void start_shell() {
    // Create pipes
    if (pipe(shell.stdin_pipe) != 0 || pipe(shell.stdout_pipe) != 0 || pipe(shell.stderr_pipe) != 0) {
        perror("Failed to create pipes");
        exit(EXIT_FAILURE);
    }

    // Fork shell process
    shell.pid = fork();
    if (shell.pid < 0) {
        perror("Failed to fork shell process");
        exit(EXIT_FAILURE);
    }

    if (shell.pid == 0) {
        // Child process (shell)

        // Redirect stdin/stdout/stderr
        dup2(shell.stdin_pipe[0], STDIN_FILENO);
        dup2(shell.stdout_pipe[1], STDOUT_FILENO);
        dup2(shell.stderr_pipe[1], STDERR_FILENO);

        // Close unused pipe ends
        close(shell.stdin_pipe[0]);
        close(shell.stdin_pipe[1]);
        close(shell.stdout_pipe[0]);
        close(shell.stdout_pipe[1]);
        close(shell.stderr_pipe[0]);
        close(shell.stderr_pipe[1]);

        // Execute shell
        execl(SHELL_PATH, SHELL_PATH, DANGEROUS_COMMANDS_PATH, EXEC_TIMES_LOG_PATH, NULL);

        // If execl returns, it failed
        perror("Failed to execute shell");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        // Close unused pipe ends
        close(shell.stdin_pipe[0]);
        close(shell.stdout_pipe[1]);
        close(shell.stderr_pipe[1]);

        // Set pipes to non-blocking
        int flags = fcntl(shell.stdout_pipe[0], F_GETFL, 0);
        fcntl(shell.stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

        flags = fcntl(shell.stderr_pipe[0], F_GETFL, 0);
        fcntl(shell.stderr_pipe[0], F_SETFL, flags | O_NONBLOCK);

        // Wait for shell to start
        usleep(100000); // 100ms
    }
}

// Terminate shell process
void stop_shell() {
    // Send "done" command to exit shell
    const char *exit_cmd = "done\n";
    write(shell.stdin_pipe[1], exit_cmd, strlen(exit_cmd));

    // Wait for shell to exit or force kill after timeout
    int status;
    pid_t result = waitpid(shell.pid, &status, WNOHANG);

    if (result == 0) {
        // Shell didn't exit, wait with timeout
        struct timeval start, current;
        gettimeofday(&start, NULL);

        while (1) {
            result = waitpid(shell.pid, &status, WNOHANG);
            if (result != 0) break; // Shell exited

            gettimeofday(&current, NULL);
            double elapsed = (current.tv_sec - start.tv_sec) +
                            (current.tv_usec - start.tv_usec) / 1000000.0;

            if (elapsed > TIMEOUT_SECONDS) {
                WARN("Shell process did not exit, killing it");
                kill(shell.pid, SIGKILL);
                waitpid(shell.pid, &status, 0);
                break;
            }

            usleep(50000); // 50ms
        }
    }

    // Close pipes
    close(shell.stdin_pipe[1]);
    close(shell.stdout_pipe[0]);
    close(shell.stderr_pipe[0]);
}

// Send command to shell
void send_command(const char *cmd) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s\n", cmd);
    write(shell.stdin_pipe[1], buffer, strlen(buffer));
}

// Read shell output with timeout
char* read_shell_output(int timeout_ms) {
    static char buffer[4096]; // Static buffer for output
    char temp_buffer[256];
    int total_bytes = 0;
    buffer[0] = '\0';  // Empty the buffer

    struct timeval start, current;
    gettimeofday(&start, NULL);

    while (1) {
        // Try to read from stdout
        ssize_t bytes_read = read(shell.stdout_pipe[0], temp_buffer, sizeof(temp_buffer) - 1);
        if (bytes_read > 0) {
            temp_buffer[bytes_read] = '\0';
            strncat(buffer, temp_buffer, sizeof(buffer) - total_bytes - 1);
            total_bytes += bytes_read;
        }

        // Check if we've read a complete prompt
        if (strstr(buffer, ">>") != NULL) {
            break;
        }

        // Check timeout
        gettimeofday(&current, NULL);
        double elapsed = (current.tv_sec - start.tv_sec) * 1000 +
                        (current.tv_usec - start.tv_usec) / 1000.0;

        if (elapsed > timeout_ms) {
            WARN("Timeout while reading shell output");
            break;
        }

        usleep(10000); // 10ms
    }

    return buffer;
}

// Read shell error output
char* read_shell_error() {
    static char buffer[4096];
    ssize_t bytes_read = read(shell.stderr_pipe[0], buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[0] = '\0';
    }

    return buffer;
}

// Test cases implementation

// Test basic command execution
void test_basic_command() {
    INFO("Testing basic command execution");

    // Start shell
    start_shell();

    // Read initial prompt
    char *output = read_shell_output(1000);
    INFO("Initial prompt: " + output);

    // Test echo command
    send_command("echo hello world");
    output = read_shell_output(1000);

    if (strstr(output, "hello world") != NULL) {
        PASS("Basic echo command works");
    } else {
        FAIL("Echo command failed");
    }

    // Test simple ls command
    send_command("ls");
    output = read_shell_output(1000);

    if (strlen(output) > 20) { // Assuming ls will output something
        PASS("Basic ls command works");
    } else {
        FAIL("ls command failed");
    }

    // Stop shell
    stop_shell();
}

// Test argument limit
void test_argument_limit() {
    INFO("Testing argument limit (max 6 arguments)");

    // Start shell
    start_shell();

    // Read initial prompt
    read_shell_output(1000);

    // Test with 6 arguments (should work)
    const char *cmd_6args = "echo arg1 arg2 arg3 arg4 arg5 arg6";
    send_command(cmd_6args);
    char *output = read_shell_output(1000);

    if (strstr(output, "arg6") != NULL) {
        PASS("Command with 6 arguments works");
    } else {
        FAIL("Command with 6 arguments failed");
    }

    // Test with 7 arguments (should fail with ERR_ARGS)
    const char *cmd_7args = "echo arg1 arg2 arg3 arg4 arg5 arg6 arg7";
    send_command(cmd_7args);
    output = read_shell_output(1000);

    if (strstr(output, "ERR_ARGS") != NULL) {
        PASS("Command with 7 arguments correctly returns ERR_ARGS");
    } else {
        FAIL("Command with 7 arguments didn't return ERR_ARGS");
    }

    // Stop shell
    stop_shell();
}

// Test multiple spaces between arguments
void test_multiple_spaces() {
    INFO("Testing multiple spaces between arguments");

    // Start shell
    start_shell();

    // Read initial prompt
    read_shell_output(1000);

    // Test with single spaces (should work)
    const char *cmd_single_spaces = "echo hello world";
    send_command(cmd_single_spaces);
    char *output = read_shell_output(1000);

    if (strstr(output, "hello world") != NULL) {
        PASS("Command with single spaces works");
    } else {
        FAIL("Command with single spaces failed");
    }

    // Test with multiple spaces (should fail with ERR_SPACE)
    const char *cmd_multiple_spaces = "echo  hello  world";  // Note double spaces
    send_command(cmd_multiple_spaces);
    output = read_shell_output(1000);

    if (strstr(output, "ERR_SPACE") != NULL) {
        PASS("Command with multiple spaces correctly returns ERR_SPACE");
    } else {
        FAIL("Command with multiple spaces didn't return ERR_SPACE");
    }

    // Stop shell
    stop_shell();
}

// Test dangerous commands
void test_dangerous_commands() {
    INFO("Testing dangerous commands handling");

    // Create dangerous commands file
    const char *dangerous_cmds =
        "rm -rf /\n"
        "sudo reboot\n"
        "echo dangerous text\n";
    create_dangerous_commands_file(dangerous_cmds);

    // Start shell
    start_shell();

    // Read initial prompt
    read_shell_output(1000);

    // Test exact dangerous command (should be blocked)
    const char *dangerous_cmd = "rm -rf /";
    send_command(dangerous_cmd);
    char *output = read_shell_output(1000);

    if (strstr(output, "Dangerous command detected") != NULL) {
        PASS("Exact dangerous command correctly blocked");
    } else {
        FAIL("Exact dangerous command not blocked");
    }

    // Test similar but not exact command (should show warning but execute)
    const char *similar_cmd = "rm -rf ./temp";
    send_command(similar_cmd);
    output = read_shell_output(1000);

    if (strstr(output, "Command similar to dangerous command") != NULL) {
        PASS("Similar command shows warning");
    } else {
        FAIL("Similar command doesn't show warning");
    }

    // Test non-dangerous command (should work normally)
    const char *safe_cmd = "echo safe text";
    send_command(safe_cmd);
    output = read_shell_output(1000);

    if (strstr(output, "safe text") != NULL &&
        strstr(output, "dangerous") == NULL) {
        PASS("Safe command works normally");
    } else {
        FAIL("Safe command not working normally");
    }

    // Stop shell
    stop_shell();
}

// Test time measurement
void test_time_measurement() {
    INFO("Testing time measurement functionality");

    // Start shell
    start_shell();

    // Read initial prompt
    char *output = read_shell_output(1000);

    // Check initial time values are zero
    if (strstr(output, "last_cmd_time:0.00000") != NULL &&
        strstr(output, "avg_time:0.00000") != NULL) {
        PASS("Initial time values are correctly zero");
    } else {
        FAIL("Initial time values are not zero");
    }

    // Run a simple command
    send_command("echo test");
    output = read_shell_output(1000);

    // Check time values are updated
    if (strstr(output, "last_cmd_time:0.00000") == NULL) {
        PASS("Time values are updated after command");
    } else {
        FAIL("Time values not updated after command");
    }

    // Run a command with predictable timing
    send_command("sleep 1");
    output = read_shell_output(2000);

    // Check if time is around 1 second
    if (strstr(output, "last_cmd_time:1.") != NULL) {
        PASS("Sleep command timing is correct");
    } else {
        FAIL("Sleep command timing is incorrect");
        INFO(output);
    }

    // Stop shell
    stop_shell();

    // Check log file exists and has content
    FILE *log = fopen(EXEC_TIMES_LOG_PATH, "r");
    if (log) {
        char buffer[1024];
        size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, log);
        buffer[bytes_read] = '\0';
        fclose(log);

        if (strstr(buffer, "echo test") != NULL &&
            strstr(buffer, "sleep 1") != NULL) {
            PASS("Log file contains correct command entries");
        } else {
            FAIL("Log file doesn't contain correct command entries");
        }
    } else {
        FAIL("Log file doesn't exist or can't be read");
    }
}

// Test very long command
void test_long_command() {
    INFO("Testing command length limits");

    // Start shell
    start_shell();

    // Read initial prompt
    read_shell_output(1000);

    // Create a command just under 1024 characters
    char long_cmd[1024];
    strcpy(long_cmd, "echo ");
    for (int i = 5; i < 1020; i++) {
        long_cmd[i] = 'a' + (i % 26);
    }
    long_cmd[1020] = '\0';

    // Send the long command
    send_command(long_cmd);
    char *output = read_shell_output(1000);

    // Check if command was processed
    if (strstr(output, "ERR") == NULL) {
        PASS("Long command (just under 1024) works");
    } else {
        FAIL("Long command failed");
    }

    // Create a command over 1024 characters
    char too_long_cmd[1100];
    strcpy(too_long_cmd, "echo ");
    for (int i = 5; i < 1050; i++) {
        too_long_cmd[i] = 'a' + (i % 26);
    }
    too_long_cmd[1050] = '\0';

    // Send the too long command
    send_command(too_long_cmd);
    output = read_shell_output(1000);

    // Since exact behavior for >1024 chars isn't specified,
    // we just check that something happened (prompt appeared)
    if (strlen(output) > 0) {
        INFO("Command over 1024 chars handled (exact behavior depends on implementation)");
    }

    // Stop shell
    stop_shell();
}

// Test prompt statistics
void test_prompt_statistics() {
    INFO("Testing prompt statistics");

    // Start shell
    start_shell();

    // Read initial prompt
    char *output = read_shell_output(1000);

    // Check initial statistics
    if (strstr(output, "#cmd:0") != NULL &&
        strstr(output, "#dangerous_cmd_blocked:0") != NULL) {
        PASS("Initial command counts are zero");
    } else {
        FAIL("Initial command counts are not zero");
    }

    // Run several commands and check statistics
    const char *cmds[] = {
        "echo test1",
        "echo test2",
        "ls"
    };

    for (int i = 0; i < 3; i++) {
        send_command(cmds[i]);
        output = read_shell_output(1000);
    }

    // Check command count after 3 commands
    char expected_cmd_count[] = "#cmd:3";
    if (strstr(output, expected_cmd_count) != NULL) {
        PASS("Command count correctly increased to 3");
    } else {
        FAIL("Command count incorrect after 3 commands");
        INFO(output);
    }

    // Stop shell
    stop_shell();
}

// Main test runner
int main() {
    printf("\n%s=== MINI SHELL TEST SUITE ===%s\n\n", BLUE, RESET);

    // Define all test cases
    TestCase tests[] = {
        {"Basic Command Execution", test_basic_command},
        {"Argument Limit", test_argument_limit},
        {"Multiple Spaces", test_multiple_spaces},
        {"Dangerous Commands", test_dangerous_commands},
        {"Time Measurement", test_time_measurement},
        {"Long Command", test_long_command},
        {"Prompt Statistics", test_prompt_statistics}
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    // Run all tests
    for (int i = 0; i < num_tests; i++) {
        printf("\n%s=== TEST %d/%d: %s ===%s\n", BLUE, i+1, num_tests, tests[i].name, RESET);
        tests[i].test_func();
        passed++; // Note: this is optimistic, assumes tests mark failures but continue
    }

    // Print summary
    printf("\n%s=== TEST SUMMARY ===%s\n", BLUE, RESET);
    printf("Ran %d tests\n", num_tests);
    printf("%s%d tests completed%s\n\n", GREEN, passed, RESET);

    return 0;
}