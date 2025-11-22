// transpiled from pipes.ox
#include "stdio.h"
int add(int a, int b);
int main() {
    // Placeholder pipe (append)
    printf("Hello, %s!\n", "world");
    
    // Placeholder pipe (append)
    printf("Value is: %d\n", 123);
    
    // Chained expression
    printf("20 + 5 = %d\n", add(5, 20));
    
    return 0;
}
int add(int a, int b) {
    return a + b;
}
