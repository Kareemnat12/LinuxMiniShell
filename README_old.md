# Mini Shell Implementation

## Overview
This project implements a simple shell (command-line interface) in C under Linux. The shell reads user commands, executes them using system calls, and provides additional features including command timing, dangerous command detection, and detailed statistics tracking.

## Features

### 1. Basic Command Execution
- Reads and executes system commands using `fork()` and `execvp()`
- Supports command input of up to 1024 characters
- Handles up to 6 command-line arguments (excluding the command name)
- Terminates on the `done` command

### 2. Informative Prompt
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

### 3. Command Timing
- Measures execution time for each command using `clock_gettime()`
- Logs detailed timing data to a specified output file
- Tracks and displays statistics about execution times

### 4. Dangerous Commands Protection
- Reads potential dangerous commands from a specified input file
- Blocks execution of exact matches to dangerous commands
- Warns about similar commands (same command name but different arguments)
- Tracks statistics on blocked and flagged commands

## Usage

```
./ex1 <dangerous_commands_file> <execution_times_log>
```

### Parameters:
- `dangerous_commands_file`: Text file containing dangerous commands (one per line)
- `execution_times_log`: File where command execution times will be logged

### Example:
```
./ex1 dangerous_commands.txt exec_times.log
```

## Input Validation
- Maximum command length: 1024 characters
- Maximum number of arguments: 6 (excluding the command name)
- Multiple consecutive spaces between arguments are not allowed

## Error Messages
- `ERR_ARGS`: Displayed when more than 6 arguments are provided
- `ERR_space`: Displayed when multiple spaces are found between arguments
- `ERR_MAX_CHAR`: Displayed when input exceeds maximum length
- `ERR: Dangerous command detected`: Displayed when a dangerous command is blocked
- Command-specific error messages are shown using `perror()`

## Dangerous Commands Format
The dangerous commands file should contain one command per line, with the exact command and arguments. For example:
```
rm -rf /
sudo reboot
shutdown -h now
mkfs.ext4 /dev/sda
```

## Execution Time Log Format
Each executed command is logged with its execution time in the specified log file:
```
ls -l : 0.00234 sec
sleep 5 : 5.00012 sec
grep "pattern" file.txt : 0.12345 sec
```

## Exiting the Shell
Type `done` to exit the shell. Upon exit, the shell will display:
```
blocked command: <count>
unblocked commands: <count>
```
Indicating how many dangerous commands were blocked and how many similar but unblocked commands were detected.

## Implementation Details

### Key Functions
- `get_string()`: Reads user input with dynamic memory allocation
- `split_to_args()`: Splits command string into argument array
- `checkMultipleSpaces()`: Validates spacing between arguments
- `read_file_lines()`: Reads dangerous commands from file
- `is_dangerous_command()`: Checks if a command is dangerous
- `time_diff()`: Calculates execution time difference
- `prompt()`: Displays the detailed shell prompt

### Memory Management
- Dynamic memory allocation for input and command arguments
- Proper cleanup to prevent memory leaks
- Error handling for memory allocation failures

## Compilation
```
gcc -Wall -o ex1 ex1.c
```

## Notes
- The shell waits for each child process to complete before displaying the prompt again
- The log file is cleared at the start of each shell execution
- Initial minimum time is set to -1 to ensure it gets updated on the first command execution
- The programme won't run if argv files are not provided, will see error ```"Usage: %s <dangerous_commands_file> <log_file>"```

