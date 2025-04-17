#!/bin/bash

# Test Script for Linux Mini Shell (ex1.c)
# Date: 2025-04-16
# Author: GitHub Copilot for Kareemnat12

# Color definitions for output formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test log file
TEST_LOG="mini_shell_test_$(date +%Y%m%d_%H%M%S).log"
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Initialize log file
echo "===== MINI SHELL TEST LOG =====" > "$TEST_LOG"
echo "Date: $(date)" >> "$TEST_LOG"
echo "============================" >> "$TEST_LOG"

# Function to log test results
log_test() {
    local test_name="$1"
    local status="$2"
    local details="$3"
    
    echo -e "\n[TEST] $test_name" >> "$TEST_LOG"
    echo -e "[STATUS] $status" >> "$TEST_LOG"
    echo -e "[DETAILS] $details" >> "$TEST_LOG"
    echo -e "-----------------------------" >> "$TEST_LOG"
    
    if [ "$status" = "PASS" ]; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    elif [ "$status" = "WARN" ]; then
        echo -e "${YELLOW}⚠ WARNING${NC}: $test_name"
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name"
        echo -e "  Details: $details"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

# Function to check if required files exist
check_required_files() {
    if [ ! -f "dangerous_commands.txt" ]; then
        echo -e "${RED}ERROR: Required file 'dangerous_commands.txt' is missing.${NC}"
        echo -e "[ERROR] Required file 'dangerous_commands.txt' is missing." >> "$TEST_LOG"
        exit 1
    fi
}

# Create a mini dangerous commands file for testing
create_test_dangerous_commands.txt() {
    echo "rm" > dangerous_commands.txt
    echo "mkfs" >> dangerous_commands.txt
    echo "dd" >> dangerous_commands.txt
    echo "format" >> dangerous_commands.txt
    echo "shutdown" >> dangerous_commands.txt
    echo "test_dangerous_command" >> dangerous_commands.txt
}

# Compile the program
compile_program() {
    echo -e "${BLUE}Compiling ex1.c...${NC}"
    gcc -o mini_shell ex1.c 2> compile_errors.txt
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Compilation failed! See details below:${NC}"
        cat compile_errors.txt
        echo -e "[COMPILE ERROR]" >> "$TEST_LOG"
        cat compile_errors.txt >> "$TEST_LOG"
        exit 1
    else
        echo -e "${GREEN}Compilation successful!${NC}"
        echo -e "[COMPILE SUCCESS]" >> "$TEST_LOG"
        rm compile_errors.txt
    fi
}

# Run a command in the mini shell and capture output
run_shell_command() {
    local command="$1"
    local expected_output="$2"
    local expected_exit_code="$3"
    local description="$4"
    local timeout_duration="$5"
    
    if [ -z "$timeout_duration" ]; then
        timeout_duration=5
    fi
    
    echo "$command" > test_input.txt
    echo "exit" >> test_input.txt
    
    # Run with timeout to prevent hanging
    timeout "$timeout_duration" ./mini_shell < test_input.txt > test_output.txt 2> test_error.txt
    exit_code=$?
    
    # Check for timeout
    if [ $exit_code -eq 124 ]; then
        log_test "$description" "FAIL" "Command timed out after ${timeout_duration} seconds. Possible infinite loop or program hang."
        return 1
    fi
    
    # Check exit code if specified
    if [ ! -z "$expected_exit_code" ] && [ $exit_code -ne "$expected_exit_code" ]; then
        log_test "$description" "FAIL" "Expected exit code ${expected_exit_code}, got ${exit_code}"
        return 1
    fi
    
    # Check output if specified
    if [ ! -z "$expected_output" ]; then
        if grep -q "$expected_output" test_output.txt || grep -q "$expected_output" test_error.txt; then
            log_test "$description" "PASS" "Command: '$command' produced expected output."
        else
            output=$(cat test_output.txt)
            errors=$(cat test_error.txt)
            log_test "$description" "FAIL" "Command: '$command' did not produce expected output. Expected: '$expected_output', Got: '$output', Errors: '$errors'"
            return 1
        fi
    else
        log_test "$description" "PASS" "Command: '$command' executed without checking output."
    fi
    
    return 0
}

# Function to check log file
check_log_file() {
    if [ ! -f "exec_time.log" ]; then
        log_test "Log file creation" "FAIL" "exec_time.log file was not created"
        return 1
    fi
    
    # Check if log file has entries
    if [ ! -s "exec_time.log" ]; then
        log_test "Log file content" "FAIL" "exec_time.log exists but is empty"
        return 1
    fi
    
    # Check if log file has proper format
    if ! grep -q "Execution time:" exec_time.log; then
        log_test "Log file format" "FAIL" "exec_time.log does not contain execution time information"
        return 1
    fi
    
    log_test "Log file verification" "PASS" "exec_time.log exists and has proper content"
    return 0
}

# Main test function
run_tests() {
    echo -e "${BLUE}Starting tests for mini shell...${NC}"
    
    # Backup any existing dangerous_commands.txt
    if [ -f "dangerous_commands.txt" ]; then
        cp dangerous_commands.txt dangerous_commands.txt.backup
    fi
    
    # Create test dangerous command file
    create_test_dangerous_commands.txt
    
    # Test 1: Basic command execution
    run_shell_command "echo hello world" "hello world" "" "Basic command execution"
    
    # Test 2: Command with arguments
    run_shell_command "ls -la" "" "" "Command with arguments"
    
    # Test 3: Try running a dangerous command
    run_shell_command "rm -rf *" "dangerous command" "" "Dangerous command detection"
    
    # Test 4: Try running a custom dangerous command
    run_shell_command "test_dangerous_command" "dangerous command" "" "Custom dangerous command detection"
    
    # Test 5: Empty command
    run_shell_command "" "" "" "Empty command handling"
    
    # Test 6: Very long command (boundary condition)
    long_cmd=$(printf 'a%.0s' {1..1000})
    run_shell_command "echo $long_cmd" "" "" "Very long command (1000 chars)"
    
    # Test 7: Command with many arguments (boundary condition)
    many_args=$(for i in {1..100}; do echo -n "arg$i "; done)
    run_shell_command "echo $many_args" "" "" "Command with many arguments (100 args)"
    
    # Test 8: Non-existent command
    run_shell_command "thiscommanddoesnotexist" "failed" "" "Non-existent command"
    
    # Test 9: Command with special characters
    run_shell_command "echo 'Hello; World'" "Hello; World" "" "Command with special characters"
    
    # Test 10: Using cd command
    run_shell_command "cd /tmp && pwd" "/tmp" "" "Change directory command"
    
    # Test 11: Multiple commands on separate runs
    run_shell_command "echo test1" "test1" "" "First of multiple commands"
    run_shell_command "echo test2" "test2" "" "Second of multiple commands"
    
    # Test 12: Check log file
    check_log_file
    
    # Test 13: Running without dangerous_commands.txt
    mv dangerous_commands.txt dangerous_commands.txt.temp
    run_shell_command "echo test" "error" "" "Running without dangerous_commands.txt"
    mv dangerous_commands.txt.temp dangerous_commands.txt
    
    # Additional tests:
    
    # Test 14: Command with leading/trailing whitespace
    run_shell_command "   echo   spaced   out   " "spaced   out" "" "Command with excessive whitespace"
    
    # Test 15: Unicode characters in command
    run_shell_command "echo 'Hello 世界'" "Hello 世界" "" "Unicode characters in command"
    
    # Test 16: Multiple pipes (if supported)
    run_shell_command "ls | grep test" "" "" "Command with pipe (if supported)"
    
    # Test 17: Input/output redirection (if supported)
    run_shell_command "echo test > test_file.txt" "" "" "Output redirection (if supported)"
    
    # Test 18: Restart after error
    run_shell_command "thiscommanddoesnotexist" "" "" "Non-existent command"
    run_shell_command "echo recovery" "recovery" "" "Recovery after error"
    
    # Test 19: Memory stress test - create many processes
    stress_cmd="for i in {1..5}; do echo \$i; done"
    run_shell_command "$stress_cmd" "" "" "Multiple command executions (stress test)"
    
    # Test 20: Graceful termination
    run_shell_command "exit" "" "" "Graceful termination"
    
    # Restore original dangerous_commands.txt if it existed
    if [ -f "dangerous_commands.txt.backup" ]; then
        mv dangerous_commands.txt.backup dangerous_commands.txt
    fi
    
    # Clean up test files
    rm -f test_input.txt test_output.txt test_error.txt test_file.txt
}

# Main execution
echo -e "${BLUE}=== Mini Shell Test Suite ===${NC}"
check_required_files
compile_program
run_tests

# Print summary
echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "Total tests: ${TOTAL_TESTS}"
echo -e "${GREEN}Passed tests: ${PASSED_TESTS}${NC}"
echo -e "${RED}Failed tests: ${FAILED_TESTS}${NC}"

echo -e "\n${BLUE}Detailed log saved to: ${TEST_LOG}${NC}"

# Add summary to log
echo -e "\n===== TEST SUMMARY =====" >> "$TEST_LOG"
echo "Total tests: ${TOTAL_TESTS}" >> "$TEST_LOG"
echo "Passed tests: ${PASSED_TESTS}" >> "$TEST_LOG"
echo "Failed tests: ${FAILED_TESTS}" >> "$TEST_LOG"
echo "============================" >> "$TEST_LOG"

# Exit with error code if any tests failed
if [ ${FAILED_TESTS} -gt 0 ]; then
    exit 1
else
    exit 0
fi