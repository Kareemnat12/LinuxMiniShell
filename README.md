# Linux Mini Shell (ex1.c)

## Overview
This program implements a custom Linux shell for the Operating Systems course assignment. It demonstrates key OS concepts including process creation, execution, and command handling with safety mechanisms to prevent execution of dangerous commands.

## Technical Implementation
This mini shell is built using the following core components:
- **Process Creation**: Uses `fork()` to create child processes
- **Command Execution**: Implements `execvp()` to execute commands with arguments
- **Command Parsing**: Parses user input to separate command and arguments
- **Command Validation**: Checks commands against a list of dangerous commands
- **Execution Logging**: Records command execution time and status

## Required Files
The program **will not run** without the following files in the same directory:

1. **dangerous_cmd.txt**
   - Contains a list of commands that are not allowed to execute
   - Each command should be on a separate line
   - Example content:
     ```
     rm
     mkfs
     dd
     format
     ```

2. **exec_time.log**
   - Log file where the shell records command execution times
   - The shell will create this file if it doesn't exist
   - The shell will append to this file if it already exists
   - Format: `[TIMESTAMP] COMMAND - Execution time: X ms`

## How to Compile
```bash
gcc -o mini_shell ex1.c
```

## How to Run
```bash
./mini_shell
```

## Using execvp in the Implementation

The program uses `execvp()` to execute commands with their arguments. Here's how it's implemented:

```c
int executeCommand(char* argv[]) {
    pid_t pid = fork();
    
    if (pid < 0) {
        // Fork failed
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        // Child process
        if (execvp(argv[0], argv) == -1) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return status;
    }
    return 0;
}
```

## Command Execution Flow

1. The shell prompts for user input
2. Input is parsed into command (argv[0]) and arguments (argv[1], argv[2], etc.)
3. The command is checked against dangerous_cmd.txt
   - If found, execution is denied with a warning message
   - If not found, execution proceeds
4. Execution time is measured using clock functions
5. The command is executed using execvp()
6. Execution time and status are logged to exec_time.log

## Handling Command Arguments (argv)

The program parses the input line to create the argv array:

```c
char* argv[MAX_ARGS];
int argc = 0;

// Tokenize the input
char* token = strtok(input, " \t\n");
while (token != NULL && argc < MAX_ARGS - 1) {
    argv[argc++] = token;
    token = strtok(NULL, " \t\n");
}
argv[argc] = NULL; // The last element must be NULL for execvp
```

## Special Commands

- **exit**: Terminates the shell
- **cd**: Changes the current directory
- All other commands are executed using execvp()

## Error Handling

The shell handles various error conditions:
- Command not found
- Permission denied
- Dangerous command attempted
- Fork failure
- Memory allocation errors

## Sample Session
```
MyShell> ls -la
drwxr-xr-x  5 user user  4096 Apr 16 18:08 .
drwxr-xr-x 18 user user  4096 Apr 16 18:08 ..
-rw-r--r--  1 user user   220 Apr 16 18:08 dangerous_cmd.txt
-rw-r--r--  1 user user  8980 Apr 16 18:08 ex1.c
-rw-r--r--  1 user user   450 Apr 16 18:08 exec_time.log
-rwxr-xr-x  1 user user 16784 Apr 16 18:08 mini_shell

MyShell> cat dangerous_cmd.txt
rm
mkfs
dd
format

MyShell> rm -rf *
ERROR: 'rm' is listed as a dangerous command and cannot be executed.

MyShell> echo "Hello World"
Hello World

MyShell> exit
Shell terminated.
```

## Author
- [Kareemnat12](https://github.com/Kareemnat12)
- Created: 2025-04-16