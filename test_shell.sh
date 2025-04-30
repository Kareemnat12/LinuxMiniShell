#!/bin/bash

# Configuration
SHELL_PROGRAM="./ex2"
DANGEROUS_COMMANDS_FILE="f.txt"
LOG_FILE="cmd_log.txt"
TMP_OUTPUT="test_output.tmp"

# Create dangerous commands file
cat > "$DANGEROUS_COMMANDS_FILE" << EOF
rm -rf /
sudo reboot
shutdown -h now
EOF

# Function to run a test
run_test() {
    local test_num=$1
    local test_name=$2
    local commands=$3
    local expected_pattern=$4

    echo "===========================TEST NO.$test_num=========================="
    echo "Running test: $test_name..."

    # Create a temporary file with commands
    echo "$commands" > test_input.tmp
    echo "done" >> test_input.tmp

    # Run the shell with input
    $SHELL_PROGRAM "$DANGEROUS_COMMANDS_FILE" "$LOG_FILE" < test_input.tmp > "$TMP_OUTPUT" 2>&1

    # Check result
    if grep -q "$expected_pattern" "$TMP_OUTPUT"; then
        echo "PASS"
        echo "Test passed successfully"
    else
        echo "FAIL"
        echo "Expected output containing: $expected_pattern"
        echo "Actual output was:"
        cat "$TMP_OUTPUT"
    fi
    echo "=============================================================="
}

# Basic command execution test
run_test 1 "Basic echo command" \
    "echo hello world" \
    "hello world"

# Multiple command execution test
run_test 2 "Command counter increment" \
    "echo first command
    echo second command" \
    "#cmd:2|"

# Exit with "done" command test
run_test 3 "Done command exit" \
    "" \
    "0"

# Dangerous command test
run_test 4 "Dangerous command blocking" \
    "rm -rf /" \
    "ERR: Dangerous command detected"

# Exit with dangerous command count test
run_test 5 "Dangerous command count on exit" \
    "rm -rf /" \
    "1"

# Similar command test
run_test 6 "Similar command warning" \
    "rm -rf /tmp" \
    "WARNING: Command similar to dangerous command"

# Multiple spaces error test
run_test 7 "Multiple spaces error" \
    "echo hello  world" \
    "ERR_SPACE"

# Too many arguments test
run_test 8 "Too many arguments error" \
    "echo 1 2 3 4 5 6 7" \
    "ERR_ARGS"

# Time measurement test
run_test 9 "Time measurement" \
    "sleep 1" \
    "last_cmd_time:1.0"

# Min time test
run_test 10 "Minimum time tracking" \
    "sleep 2
    sleep 0.1" \
    "min_time:0.1"

# Max time test
run_test 11 "Maximum time tracking" \
    "sleep 0.1
    sleep 2" \
    "max_time:2.0"

# Average time test
run_test 12 "Average time calculation" \
    "sleep 1
    sleep 3" \
    "avg_time:[1-3]"

# Logging test
run_test 13 "Command logging" \
    "echo test logging" \
    "test logging"

# Non-existent command test
run_test 14 "Non-existent command handling" \
    "nonexistingcommand" \
    "execvp failed"

# Empty command test
run_test 15 "Empty command handling" \
    "" \
    "#cmd:0|"

# Multiple dangerous commands test
run_test 16 "Multiple dangerous commands" \
    "rm -rf /
    sudo reboot
    shutdown -h now" \
    "#cmd:0|#dangerous_cmd_blocked:3"

# Prompt format test
run_test 17 "Prompt format" \
    "echo test prompt" \
    "#dangerous_cmd_blocked:[0-9]|last_cmd_time:"

# Mixed command sequence test
run_test 19 "Mixed command sequence" \
    "echo \"Starting a long sequence\"
    sleep 0.7
    ls
    nonexistent_cmd_123
    echo \"Finished all\"" \
    "Finished all"

# Similar command with successful execution test
run_test 20 "Similar command execution" \
    "rm -rf folder1
    echo \"Still running...\"" \
    "Still running..."

# Clean up
rm -f test_input.tmp "$TMP_OUTPUT" "$DANGEROUS_COMMANDS_FILE"

echo "All tests completed!"