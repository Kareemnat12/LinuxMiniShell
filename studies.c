#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX 6
int addition(int a, int b) {
    return a + b;
}
int main(){
    int  nums [] = {1,2,3,4};
    int result[3];
    int thread[3];

    for (int i = 0; i < MAX; i++) {
        pthread_create(&thread[i], &result[i], (void *(*)(void *)) nums[i], (void *) addition(nums[i], nums[i + 1]));
    }







}

