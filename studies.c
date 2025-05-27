#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX 4

// Structure to hold two matrices for addition
typedef struct {
    int *matrix1;
    int *matrix2;
    int size;  // Size of matrices (number of elements)
} AdditionArgs;

void* addition(void *arg) {
    AdditionArgs *args = (AdditionArgs*)arg;
    int *result = malloc(sizeof(int) * args->size);

    if (!result) {
        printf("Memory allocation failed\n");
        pthread_exit(NULL);
    }

    // Add corresponding elements
    for(int i = 0; i < args->size; i++) {
        result[i] = args->matrix1[i] + args->matrix2[i];
    }

    // Don't free args here as we need it in the parent thread
    return result;
}

void print_matrix(int *matrix, int size) {
    printf("[");
    for(int i = 0; i < size; i++) {
        printf("%d", matrix[i]);
        if (i < size-1) printf(", ");
    }
    printf("]\n");
}

int main() {
    int m1[] = {1, 2, 3, 4, 5, 6};
    int m2[] = {1, 2, 3, 4, 5, 6};
    int m3[] = {1, 1, 1, 1, 1, 1};
    int m4[] = {1, 1, 1, 1, 1, 1};
    int *matrices[] = {m1, m2, m3, m4};
    int mat_count = 4;
    int mat_size = 6;  // Each matrix has 6 elements

    pthread_t threads[3];  // Two for first level, one for final addition
    int *results[2];      // To store results from first level additions

    // First level: Calculate ma = m1+m2 and mb = m3+m4 in parallel
    for(int i = 0; i < 2; i++) {
        AdditionArgs *args = malloc(sizeof(AdditionArgs));
        args->matrix1 = matrices[i*2];     // First matrix
        args->matrix2 = matrices[i*2+1];   // Second matrix
        args->size = mat_size;

        pthread_create(&threads[i], NULL, addition, args);
    }

    // Join first level threads and get results
    for(int i = 0; i < 2; i++) {
        void *result;
        pthread_join(threads[i], &result);
        results[i] = (int*)result;

        printf("Intermediate result %d: ", i+1);
        print_matrix(results[i], mat_size);
    }

    // Second level: Calculate final result m = ma+mb
    AdditionArgs *final_args = malloc(sizeof(AdditionArgs));
    final_args->matrix1 = results[0];
    final_args->matrix2 = results[1];
    final_args->size = mat_size;

    pthread_create(&threads[2], NULL, addition, final_args);

    // Get final result
    void *final_result;
    pthread_join(threads[2], &final_result);
    int *m = (int*)final_result;

    printf("Final result: ");
    print_matrix(m, mat_size);

    // Clean up
    for(int i = 0; i < 2; i++) {
        free(results[i]);
    }
    free(m);
    free(final_args);

    return 0;
}