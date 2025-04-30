#include <stdio.h>
#include <string.h>

int pipe_split(char *input, char *left_cmd, char *right_cmd) {
    char *token = strtok(input, "|");

    if (token) {
        strcpy(left_cmd, token);

        token = strtok(NULL, "|");
        if (token) {
            strcpy(right_cmd, token);
            return 1; // pipe exists
        } else {
            right_cmd[0] = '\0';
            return 0; // no second command after pipe
        }
    } else {
        strcpy(left_cmd, input);
        right_cmd[0] = '\0';
        return 0; // no pipe at all
    }
}
int main() {
    char input[] = "ls -l  grep txt";
    char left[100], right[100];

    int has_pipe = pipe_split(input, left, right);

    printf("Pipe? %s\n", has_pipe ? "YES" : "NO");
    printf("Left:  '%s'\n", left);
    printf("Right: '%s'\n", right);

    return 0;
}
