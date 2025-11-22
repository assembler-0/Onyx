// transpiled from resolve.ox
#include "stdio.h"
#include "stdbool.h"
typedef struct Vector {
    int x;
    int y;
} Vector;
// resolve Vector
void Vector_print(Vector* self) {
    printf("Vector(%d, %d)\n", self->x, self->y);
}
void Vector_set(Vector* self, int x, int y) {
    self->x = x;
    self->y = y;
}
// end resolve
int main() {
    Vector v;
    Vector_set(&v, 10, 20);
    Vector_print(&v);
    
    // Also test the pipe syntax for resolve functions
    Vector_print(&v);
    return 0;
}
