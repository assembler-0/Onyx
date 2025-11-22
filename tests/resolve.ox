@include "stdio.h"
@include "stdbool.h"

struct Vector {
    x: i32
    y: i32
}

resolve Vector {
    fn print() -> void {
        printf("Vector(%d, %d)\n", self->x, self->y)
    }

    fn set(x: i32, y: i32) -> void {
        self->x = x
        self->y = y
    }
}

fn main() -> i32 {
    var v: Vector
    v.set(10, 20)
    v.print()

    # Also test the pipe syntax for resolve functions
    v |> Vector_print()
    return 0
}
