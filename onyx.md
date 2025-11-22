Onyx Language Specification

Target: Source-to-Source (Transpiles to C99/GNU C)

Core Philosophy

Onyx is designed to look modern (Rust/Swift style) but map 1:1 to C constructs. It relies on strict keyword placement to allow simple Regex engines to parse and convert it.

Identity: Onyx is Composition-Oriented. It avoids inheritance in favor of compile-time mixins and strict data/logic separation.

1. Basic Syntax & Types

Primitive Types

Onyx types map directly to standard C types (stdint.h).

Onyx Type

C Equivalent

i32

int

u8

unsigned char

f64

double

bool

int (0 or 1)

void

void

str

char*

ptr

void*

Comments

Comments use the hash symbol #.

# This is a comment
var x: i32 = 10  # Inline comment


2. Variables & Pointers

Declaration

Syntax: var [modifiers] name: type = value

Regex Friendliness:
Regex captures groups: var\s+(?:(volatile|const)\s+)?(\w+):\s+([\w\*]+)\s*=\s*(.*)

Example:

var count: i32 = 0
var volatile status: u8 = 1        # Maps to: volatile unsigned char status = 1;
var reference: i32* = &count       # Maps to: int* reference = &count;
var null_ptr: void* = null         # Maps 'null' to 'NULL'


3. Functions

Definition

Syntax: [modifiers] fn Name(args) -> ReturnType {

Regex Friendliness:

Regex: (?:(inline|extern|static)\s+)?fn\s+(\w+)\((.*)\)\s*->\s*([\w\*]+)\s*\{

Replace: $1 $4 $2($3) {

Example:

fn add(a: i32, b: i32) -> i32 {
    return a + b
}

# Inline function
inline fn fast_math(x: i32) -> i32 {
    return x * 2
}


4. Control Flow

If / Else / Loops

if x > 10 {
    # do something
}

while x < 100 {
    x = x + 1
}

loop {
    # Infinite loop (while(1))
    break
}


5. Low-Level & System Features

Attributes (The @[...] syntax)

To keep the Rust-like aesthetic while supporting GCC/Clang attributes, we use @[...]. This wraps the content inside C's __attribute__((...)).

Onyx:

@[packed]
struct DataPacket {
    id: u8
    val: i32
}

@[noreturn]
fn abort_system() -> void {
    native { exit(1); }
}


Transpiles to:

struct DataPacket {
    unsigned char id;
    int val;
} __attribute__((packed));

__attribute__((noreturn)) void abort_system() { ... }


Inline Assembly (asm)

For single-line assembly instructions.

asm("nop")
asm("mov r0, r0")


Static Assertions

To check sizes or constants at compile time.

assert_static(sizeof(i32) == 4, "Integers must be 32-bit")


Transpiles to: _Static_assert(sizeof(int) == 4, "Integers must be 32-bit");

Modifiers

volatile: Ensures variable access isn't optimized away.

restrict: Optimization hint for pointers.

register: Suggests storing variable in register.

var volatile flag: u8* = 0x4000
var register counter: i32 = 0


6. The Onyx Identity: Composition & Impls

Onyx avoids C++ classes (inheritance/vtables). Instead, it uses Traits (mixins) for data reuse and Impls for behavior.

shared (traits) (Data Mixins)

A shared (trait) is a template of fields. use copies those fields into a struct at compile/transpile time.

shared Coord {
    x: f32
    y: f32
}

struct Player {
    use Coord   # Transpiler literally injects x and y here
    hp: i32
}

struct Enemy {
    use Coord
    type: u8
}


resolve Blocks (Behavior)

Instead of methods inside classes, we define resolve blocks.
The transpiler takes resolve Type { fn Name... } and converts it to Type_Name.
It automatically adds self: Type* as the first argument.

Onyx:

resolve Player {
    fn move(dx: f32, dy: f32) -> void {
        self.x = self.x + dx
        self.y = self.y + dy
    }
}


Transpiles to C:

void Player_move(Player* self, float dx, float dy) {
    self->x = self->x + dx;
    self->y = self->y + dy;
}


Dot-Syntax Sugar

Since Player_move expects a pointer to Player as the first arg, Onyx allows dot notation which the transpiler rewrites.

my_player.move(10, 10) -> Player_move(&my_player, 10, 10)

7. Unique Features (The "Spice")

The Pipe Operator |>

Passes the left-hand value into a function call on the right-hand side. Its behavior depends on the presence of a special `_` placeholder.

1.  **Piping with a Placeholder (`_`)**

    If the function call on the right contains a `_` placeholder, the value from the left-hand side will replace every occurrence of `_`. This allows you to specify the exact position of the piped value.

    `expression |> func(arg1, _, arg3)` becomes `func(arg1, expression, arg3)`

    **Examples:**

    ```onyx
    # Pipe to the last argument
    100 |> printf("Value: %d\n", _) 
    # Transpiles to: printf("Value: %d\n", 100);

    # Pipe to a middle argument
    v.normalize() |> vector_add(v_base, _, v_offset)
    # Transpiles to: vector_add(v_base, v_normalize(), v_offset);
    ```

2.  **Piping without a Placeholder (Default Behavior)**

    If no `_` placeholder is found, the value from the left-hand side is passed as the **first** argument to the function call on the right.

    `x |> do_something(y)` becomes `do_something(x, y)`

    **Example:**

    ```onyx
    # Classic pipe, same as do_something(x)
    x |> do_something()
    ```


Native C Injection

native {
    #define MAX_BUFFER 1024
    int global_c_var = 0;
}


8. Full Systems Example (driver.ox)

@include "stdint.h"

shared Entity {
    id: u32
    active: bool
}

@[packed]
struct Packet {
    use Entity  # Injects id and active
    payload: u8
}

resolve Packet {
    fn init(id: u32) -> void {
        self.id = id
        self.active = 1
        self.payload = 0
    }
    
    fn send() -> void {
        native { printf("Sending packet %d\n", self->id); }
    }
}

fn main() -> i32 {
    var p: Packet = {0}
    
    # "Method" style (sugar for Packet_init(&p, 100))
    p.init(100)
    
    # Pipeline style (sugar for Packet_send(&p))
    p |> Packet_send()
    
    return 0
}

