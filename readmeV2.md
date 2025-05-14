# Mini Shell Implementation (v2)

## Overview
This project implements a simple shell (command-line interface) in C under Linux. The shell reads user commands, executes them using system calls, and provides additional features including command timing, dangerous command detection, detailed statistics tracking, piping, background processes, error redirection, and resource limits.

## Features

### Core Features from v1

#### 1. Basic Command Execution
- Reads and executes system commands using `fork()` and `execvp()`
- Supports command input of up to 1024 characters
- Handles up to 6 command-line arguments
- Terminates on the `done` command with statistics output
- Provides error handling for failed commands with `ERR_CMD` message

#### 2. Informative Prompt
The shell displays a detailed prompt with the following information:
```
#cmd:<count>|#dangerous_cmd_blocked:<count>|last_cmd_time:<time>|avg_time:<time>|min_time:<time>|max_time:<time>>
```
Where:
- `#cmd` - Number of successfully executed commands
- `#dangerous_cmd_blocked` - Number of dangerous commands that were blocked
- `last_cmd_time` - Execution time of the last successful command (in seconds)
- `avg_time` - Average execution time of all successful commands
- `min_time` - Minimum command execution time observed
- `max_time` - Maximum command execution time observed

#### 3. Command Timing
- Measures execution time for each command using `clock_gettime(CLOCK_MONOTONIC)`
- Logs detailed timing data to a specified output file in format: `<command> : <time> sec`
- Uses a high-precision timer with results in 5 decimal places

#### 4. Dangerous Commands Protection
- Reads potential dangerous commands from a specified input file
- Blocks execution of exact matches to dangerous commands with message: `ERR: Dangerous command detected ("<command>"). Execution prevented.`
- Warns about similar commands (same command name but different arguments) with: `WARNING: Command similar to dangerous command ("<command>"). Proceed with caution.`
- Tracks statistics on blocked and similar commands

### New Features in v2

#### 1. Pipe Mechanism (`|`)
- Supports piping output from one command to another using the `|` character
- Requires exactly one space before and after the pipe character
- Supports single pipe operations (command1 | command2)

#### 2. Internal `my_tee` Command
- Custom implementation of the `tee` command as an internal shell command
- Reads from standard input and writes to both standard output and specified files
- Supports the `-a` (append) option to add content to the end of files rather than overwriting
- Can write to multiple files simultaneously
- Example usage:
  ```
  command | my_tee file.txt          # Write output to screen and file.txt
  command | my_tee -a file.txt       # Append output to file.txt
  command | my_tee file1.txt file2.txt  # Write to multiple files
  ```

#### 3. Resource Limit Mechanism
- Implements a resource limitation system via the internal `rlimit` command
- Allows setting and displaying resource limits for processes
- Supports four types of resources:
  - `cpu`: CPU time in seconds
  - `mem`: Memory size in B, KB, MB, GB
  - `fsize`: Maximum file size
  - `nofile`: Maximum number of open files
- Syntax:
  - `rlimit set resource=soft_value[:hard_value] command [args...]`
  - `rlimit show [resource]`
- Example usage:
  ```
  rlimit set cpu=2:3 sleep 10           # Limit CPU time to 2s (soft) and 3s (hard)
  rlimit set mem=50M ./memory_program   # Limit memory to 50MB
  rlimit show                           # Show all current resource limits
  rlimit show cpu                       # Show only CPU resource limits
  ```

#### 4. Background Process Support (`&`)
- Executes commands in the background when followed by `&`
- Returns the prompt immediately, allowing the user to execute other commands while the background process runs
- Example usage:
  ```
  sleep 5 &    # Runs sleep in the background
  ```

#### 5. Error Output Redirection (`2>`)
- Redirects standard error output to a file using `2>`
- Example usage:
  ```
  command 2> error.log    # Redirects error messages to error.log
  ```

#### 6. Enhanced Error Handling
- Comprehensive error detection and reporting based on process exit status
- Distinguishes between normal termination and signal-based termination
- Reports specific error signals when processes are terminated abnormally
- Error messages for different termination scenarios:
  - Normal termination with non-zero exit code
  - Termination due to signals (with signal name displayed)

## Usage

```
./ex2 <dangerous_commands_file> <log_file>
```

### Parameters:
- `dangerous_commands_file`: Text file containing dangerous commands (one per line)
- `log_file`: File where command execution times will be logged

### Example:
```
./ex2 dangerous_commands.txt exec_times.log
```

## Input Validation
- Maximum command length: 1024 characters (defined by `MAX_INPUT_LENGTH`)
- Maximum number of arguments: 6 (defined by `MAX_ARGC`)
- Multiple consecutive spaces between arguments are not allowed
- Empty inputs and input containing only spaces are handled properly

## Error Messages
- `ERR_ARGS`: Displayed when more than 6 arguments are provided
- `ERR_SPACE`: Displayed when multiple spaces are found between arguments
- `ERR_MAX_CHAR`: Displayed when input exceeds maximum length
- `ERR_CMD`: Displayed when a command cannot be executed
- `ERR: Dangerous command detected ("<command>")`: Displayed when a dangerous command is blocked
- Signal-specific messages for abnormal process termination (e.g., "Terminated by signal: SIGSEGV")

## Implementation Details

### Key Functions
#### Core Functions (from v1)
- `get_string()`: Reads user input with dynamic memory allocation
- `split_to_args()`: Splits command string into argument array
- `checkMultipleSpaces()`: Validates spacing between arguments
- `read_file_lines()`: Reads dangerous commands from file
- `is_dangerous_command()`: Checks if a command is dangerous
- `time_diff()`: Calculates execution time difference
- `prompt()`: Displays the detailed shell prompt
- `append_to_log()`: Records command execution time to log file
- `update_min_max_time()`: Updates minimum and maximum execution times

#### New Functions (v2)
- `handle_pipe()`: Processes commands containing pipes
- `my_tee()`: Implements the internal tee command
- `setup_resource_limits()`: Configures resource limits for processes
- `parse_rlimit_command()`: Parses resource limit specifications
- `handle_background()`: Manages background process execution
- `redirect_stderr()`: Handles redirection of standard error
- `check_process_status()`: Enhanced error checking for process termination

### Memory Management
- Dynamic memory allocation for input and command arguments
- Proper cleanup using `free_args()` to prevent memory leaks
- Error handling for memory allocation failures
- Proper handling of file descriptors for pipes and redirections

## Compilation
```
gcc -Wall -o ex2 ex2.c
```

## Notes
- The shell clears the log file at the start of each execution
- Initial minimum time is set to -1 to ensure it gets properly updated on first command
- Both dangerous and semi-dangerous commands are tracked separately
- The program uses `fork()` and `waitpid()` to ensure proper process management
- The `my_tee` implementation uses basic system calls (`read()`, `write()`) rather than stdio functions
- Resource limits are implemented using the `setrlimit()` and `getrlimit()` system calls
