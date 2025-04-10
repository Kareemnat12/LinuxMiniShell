#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
const int MAX_INPUT_LENGTH = 1024; // Maximum input length
const int MAX_ARGC = 6; // Maximum number of arguments
char* get_string();
char **split_to_args(const char *string, const char *delimiter, int *count); // Function prototype
int  checkMultipleSpaces(const char* str);
void free_args(char **args);
int count_lines(const char* filename);
char** read_file_lines(const char* filename, int* num_lines);
int is_dangerous_command(char **user_args, int user_args_len);
char **Danger_CMD; // this is the dangerous command from the argv file
const char delim[] = " ";
int args_len; // NOW THIS USED FOR length of  splitted string form the user inoput

char **args;

int main(int argc, char* argv[]) {
    Danger_CMD= read_file_lines(argv[1], &args_len);
    // proccess starting while true inorder to keep continuously writing
    while (1) {
        printf("kareem@kareemTest~$ ");// temp untill i get the specific time and other stuff for prompt
        char *userInput = get_string();  // to store the user input, the function get_strind is defined below and gets user input dynamically and return string

        //<><> HANDLING MORE  1024 CHAR INPUT <><>
        if (userInput == NULL) { // skip if user input was more that 10124  the funciton will return null as false
            free(userInput);
            continue;
        }
        //<><> HANDLING SPACE CHECK <><>

        int spaceCheck = checkMultipleSpaces(userInput); // check for multiple spaces
        if (spaceCheck==1) { // skip if the user input is null
            free(userInput);
            continue;
        }
        // printf("<<<<THE USER INPUT IT [[[%s]]]}>>>>\n",userInput);// get input monitoring test purposes
        args = split_to_args(userInput, delim,&args_len);// we should reconstruct this to get the array splited

        //<><> HANDLING 6 ARG MAX <><>
        if (args == NULL) {//skip if the args is null
            free_args(args);
            continue; // we should handel the stuff of free storage for every cell
        }
        //<><> HANDLING DANGEROUS  INPUT <><>
        if (is_dangerous_command(args, args_len)) {
            continue; // Skip execution of dangerous command
        }
            //<><> HANDLING DONE COMMAND <><>
            if (strcmp(args[0], "done") == 0) {
                exit(0);
            }


            const pid_t pid = fork(); // starting the fork process
            // Command and arguments
            if (pid < 0)
            { /* error occurred */
                fprintf(stderr, "Fork Failed");
                free_args(args);
                // free(userInput);
                return 1;
            }

             if(pid == 0) {
                /* child process */
                // printf("THIS IS THE GIVIEN USER INPUT %s", userInput);
                // free(userInput);
                execvp(args[0], args);
                perror("execvp failed"); // ithink this for handling the args and stuff but im not sure untill now
                // free(userInput);// idont know if it will work or not but i will try to free the user input
                free_args(args);
                return 1; /**
                    * If execvp returns, an error occurred.
                    * i dont know if there is warnig about free user inpt
                    */

            }

                wait (NULL);
                free_args(args);

                printf("<<<args freed successfully>>>\n");
                printf ("<<<Child Complete>>>\n");

        }
        return 0;
    }


    ////////////////////////GET USER INPUT FUNCTION///////////////////////
    ///
    ///
    char* get_string(){
        size_t buffer_size = MAX_INPUT_LENGTH;  // Start with an initial buffer size
        size_t length = 0;  // Keep track of the current input length
        char *buffer = malloc(buffer_size);


        if (!buffer) {
            perror("Failed to allocate memory");
            return NULL;
        }

        int c;  // Variable to read each character
        while ((c = getchar()) != EOF && c != '\n') {
            if (length + 1 >= buffer_size) {
                buffer_size *= 2;  // Double the buffer size as needed
                char *new_buffer = realloc(buffer, buffer_size);
                if (!new_buffer) {
                    free(buffer);
                    perror("Failed to reallocate memory");
                    return NULL;
                }
                buffer = new_buffer;
            }

            buffer[length++] = c;  // Append the character to the buffer
        }

        // Null-terminate the string
        buffer[length] = '\0';

        // Check if input length exceeds a maximum allowed size
        if (length > MAX_INPUT_LENGTH) {
            printf("Input exceeds the maximum allowed length of %d characters\n", MAX_INPUT_LENGTH);
            free(buffer);
            return NULL;
        }

        return buffer;  // Return the dynamically allocated string

}
    //////////////////SPLIT STRING FUNCTION///////////////////////
    // Split string like a badass, and return argv-style array
    char **split_to_args(const char *string, const char *delimiter, int *count) {
        char *input_copy = strdup(string);  // Copy input to avoid trashing original
        if (!input_copy) {
            perror("Failed to allocate memory");
            exit(1);
        }

        char **argv = NULL;
        *count = 0;

        char *token = strtok(input_copy, delimiter);
        while (token != NULL) {
            // Expand the argv array to hold another string + null
            char **temp = realloc(argv, (*count + 2) * sizeof(char *));
            if (!temp) {
                perror("Failed to reallocate memory");
                free(input_copy);
                exit(1);
            }
            argv = temp;

            // Save a copy of the token
            argv[*count] = strdup(token);
            if (!argv[*count]) {
                perror("Failed to allocate memory for token");
                free(input_copy);
                exit(1);
            }

            (*count)++;
            token = strtok(NULL, delimiter);
        }

        // Null-terminate like a proper argv
        argv[*count] = NULL;
        //handling the maximum number of arguments error
        if (*count -1 > MAX_ARGC) {
            printf("ERR_ARGS\n");
            for (int i = 0; i < *count; i++) {
                free(argv[i]);
            }
            free(argv);
            free(input_copy);
            return NULL;
        }
        free(input_copy);
        return argv;
    }
    ///////SPACE CHECK FUNCTION////////


    int  checkMultipleSpaces(const char* str) {
        int spaceCount = 0;

        while (*str != '\0') {
            if (*str == ' ') {
                spaceCount++;
                if (spaceCount > 1) {
                    printf("ERR_SPC\n");
                    return 1;
                }
            } else {
                spaceCount = 0; // Reset on non-space
            }
            str++;
        }
        return 0; // No multiple spaces found
    }


    //////FREE METIHOD ////////
    void free_args(char **args) {
        if (args == NULL) {
            return;
        }

        for (int i = 0; args[i] != NULL; i++) {
            free(args[i]);
        }
        free(args);
    }




    /////////OPen FILE FUNCTION//////////
    // Step 1: Count lines
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

    // Step 2–4: Allocate and read lines
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
            buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
            lines[i++] = strdup(buffer); // Store line
        }

        fclose(file);
        return lines;
    }

////////Check For Dangerous coommand ma frined ///
///
///
int is_dangerous_command(char **user_args, int user_args_len) {
    for (int i = 0; i < args_len; i++) {
        char **dangerous_args = split_to_args(Danger_CMD[i], delim, &user_args_len);

        if (dangerous_args == NULL) {
            continue; // Skip if dangerous command couldn't be split properly
        }

        // Compare the command name (first argument)
        if (strcmp(user_args[0], dangerous_args[0]) == 0) {
            // Exact match check (same command and arguments)
            if (user_args_len == user_args_len) {
                int exact_match = 1;
                for (int j = 0; j < user_args_len; j++) {
                    if (strcmp(user_args[j], dangerous_args[j]) != 0) {
                        exact_match = 0;
                        break;
                    }
                }

                if (exact_match) {
                    // Exact match found
                    printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n", user_args[0]);
                    free_args(dangerous_args);
                    return 1; // Block execution
                }
            }

            // Similar command found (same name but different arguments)
            printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n", dangerous_args[0]);
            free_args(dangerous_args);
            return 0; // Allow execution with warning
        }

        free_args(dangerous_args);
    }

    return 0; // No dangerous command found
}
