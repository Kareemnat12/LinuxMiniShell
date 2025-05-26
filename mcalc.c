#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_MATRICES 10

typedef struct {
    int rows;
    int cols;
    int* data; // 1D array storing matrix elements row-wise
} Matrix;

int is_uppercase(const char* str) {
    for (; *str; str++) {
        if (!isupper(*str)) return 0;
    }
    return 1;
}

int parse_matrix(const char* token, Matrix* matrix) {
    // Format: (R,C:a1,a2,...,aR*C)
    int r, c;
    const char* ptr = token;

    if (*ptr != '(') return 0;
    ptr++;
    if (sscanf(ptr, "%d,%d", &r, &c) != 2) return 0;

    // Move ptr to after 'R,C'
    while (*ptr && *ptr != ':') ptr++;
    if (*ptr != ':') return 0;
    ptr++;

    // Count expected number of elements
    int expected = r * c;
    int* data = malloc(sizeof(int) * expected);
    if (!data) return 0;

    // Parse all elements separated by ','
    char* endptr;
    for (int i = 0; i < expected; i++) {
        data[i] = (int)strtol(ptr, &endptr, 10);
        if (ptr == endptr) {
            free(data);
            return 0; // parse error or missing number
        }
        ptr = endptr;
        if (i < expected - 1) {
            if (*ptr != ',') {
                free(data);
                return 0; // missing comma
            }
            ptr++;
        }
    }

    // After last element, expect ')'
    if (*ptr != ')') {
        free(data);
        return 0;
    }

    matrix->rows = r;
    matrix->cols = c;
    matrix->data = data;
    return 1;
}

void free_matrices(Matrix* matrices, int count) {
    for (int i = 0; i < count; i++) {
        free(matrices[i].data);
    }
}

int check_same_dimensions(Matrix* matrices, int count) {
    if (count < 1) return 1;

    int base_rows = matrices[0].rows;
    int base_cols = matrices[0].cols;

    for (int i = 1; i < count; i++) {
        if (matrices[i].rows != base_rows || matrices[i].cols != base_cols) {
            printf("Error: Matrix #%d dimensions (%d,%d) differ from Matrix #1 (%d,%d)\n",
                   i+1, matrices[i].rows, matrices[i].cols, base_rows, base_cols);
            return 0;
        }
    }
    return 1;
}

int parse_input(const char* input, Matrix* matrices, int* matrix_count, char* operation_out) {
    if (strncmp(input, "mcalc ", 6) != 0) {
        printf("Error: Input must start with 'mcalc'\n");
        return 0;
    }

    const char* ptr = input + 6; // skip "mcalc "
    char tokens[MAX_MATRICES + 1][512];
    int token_index = 0;

    while (*ptr) {
        while (*ptr == ' ') ptr++;
        if (*ptr != '"') {
            printf("Error: Expected '\"' at token #%d\n", token_index + 1);
            return 0;
        }
        ptr++; // skip opening quote

        const char* end_quote = strchr(ptr, '"');
        if (!end_quote) {
            printf("Error: Missing closing '\"' at token #%d\n", token_index + 1);
            return 0;
        }

        int len = end_quote - ptr;
        if (len <= 0) {
            printf("Error: Empty token at #%d\n", token_index + 1);
            return 0;
        }
        if (len >= sizeof(tokens[token_index])) {
            printf("Error: Token too long at #%d\n", token_index + 1);
            return 0;
        }

        strncpy(tokens[token_index], ptr, len);
        tokens[token_index][len] = '\0';

        token_index++;
        if (token_index > MAX_MATRICES + 1) {
            printf("Error: Too many tokens\n");
            return 0;
        }

        ptr = end_quote + 1;
    }

    if (token_index < 3) {
        printf("Error: Must provide at least two matrices and one operation\n");
        return 0;
    }

    char* operation = tokens[token_index - 1];
    if (!is_uppercase(operation) || (strcmp(operation, "ADD") != 0 && strcmp(operation, "SUB") != 0)) {
        printf("Error: Invalid operation '%s'\n", operation);
        return 0;
    }
    strcpy(operation_out, operation);

    int matrices_count = token_index - 1;

    for (int i = 0; i < matrices_count; i++) {
        if (!parse_matrix(tokens[i], &matrices[i])) {
            printf("Error: Invalid matrix format at #%d\n", i + 1);
            for (int j = 0; j < i; j++) free(matrices[j].data);
            return 0;
        }
    }

    if (!check_same_dimensions(matrices, matrices_count)) {
        for (int i = 0; i < matrices_count; i++) free(matrices[i].data);
        return 0;
    }

    *matrix_count = matrices_count;
    return 1;
}

void print_matrix(const Matrix* m) {
    printf("Matrix %dx%d:\n", m->rows, m->cols);
    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            printf("%d ", m->data[i * m->cols + j]);
        }
        printf("\n");
    }
}

int main() {
    char input[2048];
    while (1) {
        printf("Enter input: ");
        if (!fgets(input, sizeof(input), stdin)) {
            printf("Input error\n");
            return 1;
        }

        // remove newline
        input[strcspn(input, "\n")] = '\0';

        Matrix matrices[MAX_MATRICES];
        int matrix_count = 0;
        char operation[16];


        if (!parse_input(input, matrices, &matrix_count, operation)) {
            printf("❌ Parsing failed.\n");
            return 1;
        }

        printf("✅ Parsed %d matrices with operation %s\n", matrix_count, operation);
        for (int i = 0; i < matrix_count; i++) {
            print_matrix(&matrices[i]);
        }

        // Clean up
        free_matrices(matrices, matrix_count);
 //       return 0;
    }
}