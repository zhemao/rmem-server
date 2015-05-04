#include <stdio.h>

#define SIZE 1000000

int main() {
    int i = 0;
    for (; i < SIZE; ++i) {
        if (i % 10000 == 0)
            printf("i: %d\n", i);
    }
    return 0;
}
