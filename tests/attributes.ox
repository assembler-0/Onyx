@include "stdio.h"
@include "stdint.h"
@include "stdlib.h"

@[packed]
struct PackedStruct {
    a: u8
    b: i32
}

@[noreturn]
fn stop_now() -> void {
    printf("This should not return.\n")
    native { exit(1); }
}

fn main() -> i32 {
    # The test passes if this compiles.
    # The noreturn attribute will cause a warning if the compiler
    # thinks the function can return, but our native exit prevents that.
    var s: PackedStruct
    printf("Size of packed struct: %zu\n", sizeof(s))

    # We don't call stop_now() because it would stop the test.
    # The test is that it transpiles and compiles correctly.
    return 0
}
