// transpiled from attributes.ox
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
typedef struct PackedStruct {
    uint8_t a;
    int b;
} PackedStruct __attribute__((packed));
__attribute__((noreturn)) void stop_now() {
    printf("This should not return.\n");
     exit(1); 
}
int main() {
    // The test passes if this compiles.
    // The noreturn attribute will cause a warning if the compiler
    // thinks the function can return, but our native exit prevents that.
    PackedStruct s;
    printf("Size of packed struct: %zu\n", sizeof(s));
    
    // We don't call stop_now() because it would stop the test.
    // The test is that it transpiles and compiles correctly.
    return 0;
}
