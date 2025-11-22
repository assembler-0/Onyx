@include "stdio.h"

fn add(a: i32, b: i32) -> i32

fn main() -> i32 {
    # Placeholder pipe (append)
    "world" |> printf("Hello, %s!\n", _)

    # Placeholder pipe (append)
    123 |> printf("Value is: %d\n", _)

    # Chained expression
    20 |> add(5, _) |> printf("20 + 5 = %d\n", _)

    return 0
}

fn add(a: i32, b: i32) -> i32 {
    return a + b
}
