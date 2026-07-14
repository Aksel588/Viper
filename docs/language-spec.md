# Viper Language Specification

## Overview

Viper is a statically typed language for AI/ML and data-science research. Programs use `.vp` files organized into modules. Cross-file functions are linked via explicit `open` dependencies. Programs compile to bytecode and run on the `viperrun` VM.

**Version:** 0.2.0

## Modules

Each file belongs to a module. Declare the module name at the top of the file (optional — defaults to the filename stem):

```viper
module math
export fn add(a: int, b: int) -> int {
    return a + b
}
fn helper() -> int { return 1 }   # file-private
```

- `export fn` — visible to other modules that `open` this module
- plain `fn` — private to the file/module

### Opening modules

```viper
open math
result: int = add(1, 2)       # unqualified via open
alt: int = math.add(3, 4)     # qualified call
```

The compiler resolves `open` statements transitively and only links modules in the dependency closure.

## Types

| Type | Description |
|------|-------------|
| `int` | 64-bit integer |
| `float` | Floating point |
| `string` | UTF-8 string |
| `bool` | Boolean |
| `list[elem]` | Homogeneous list; `elem` is `int`, `float`, `string`, or `bool` |
| `struct Name { ... }` | User-defined struct type |
| `tensor[elem, d1, d2, ...]` | N-dimensional tensor; `elem` is `int` or `float` |

Example: `tensor[float, 2, 3]` is a 2×3 matrix of floats. Example: `list[int]` is a list of integers.

## Structs

Declare a struct at top level:

```viper
struct Point {
    x: int
    y: int
}
```

Create values with struct literals and access fields:

```viper
p: Point = Point { x: 1, y: 2 }
print(p.x)
```

Field access uses dot syntax on a struct value (not to be confused with `module.fn` qualified calls).

## Lists

List type syntax: `list[int]`, `list[string]`, etc.

```viper
nums: list[int] = [1, 2, 3]
print(len(nums))
print(nums[1])
append(nums, 4)
```

List literals use single brackets with comma-separated expressions. Tensor literals use nested brackets: `[[1, 2], [3, 4]]`.

## Statements

### Variable Declaration

```viper
x: int = 42
name: string = "viper"
A: tensor[float, 2, 2] = [[1.0, 2.0], [3.0, 4.0]]
items: list[int] = [1, 2, 3]
```

### Assignment

```viper
x = 10
```

### Print

```viper
print(x)
print(A)
print(items)
```

### Functions

```viper
export fn add(a: int, b: int) -> int {
    return a + b
}
```

### While Loops

```viper
while x > 0 {
    x = x - 1
}
```

Condition must be `bool` (same rule as `if`).

### For Loops

```viper
for i in 0..10 {
    x = x + i
}
```

Range `start..end` is inclusive on both bounds. Loop variable is `int`.

### Break and Continue

```viper
for i in 0..10 {
    if i == 5 {
        break
    }
    if i == 2 {
        continue
    }
    print(i)
}
```

`break` exits the innermost loop; `continue` jumps to the next iteration (for loops: the increment step).

### Conditionals

```viper
if x > 0 {
    y = 1
} else {
    y = 0
}
```

## Expressions

### Operators

| Precedence | Operators |
|------------|-----------|
| Unary | `-`, `!` |
| Postfix | `[` index `]`, `.` field |
| Matrix multiply | `@` |
| Multiplicative | `*`, `/` |
| Additive | `+`, `-` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical AND | `&&` (short-circuit) |
| Logical OR | `\|\|` (short-circuit) |

Logical operands must be `bool`. Result type is `bool`.

### Explicit Casts

```viper
x: float = float(42)
y: int = int(3.14)
z: bool = bool(1)
```

### Matrix Multiply

For 2D tensors `(m,n) @ (n,p)`, the result shape is `(m,p)`.

### Element-wise Tensor Ops

For tensors with identical shape: `+`, `-`, `*`, `/` apply element-wise.

### Builtins

Global builtins (no module prefix):

| Builtin | Description |
|---------|-------------|
| `transpose(tensor)` | Transpose a 2D tensor |
| `reshape(tensor, d1, d2, ...)` | Reshape tensor |
| `len(string)` / `len(list)` | Length as `int` |
| `append(list, elem)` | Append element; returns list |
| `abs(int\|float)` | Absolute value |
| `sqrt(float)` | Square root |
| `floor(float)` / `ceil(float)` | Rounding |
| `read_file(path)` | Read file contents as string |
| `write_file(path, content)` | Write file; returns `1` on success, `0` on failure |
| `input()` | Read one line from stdin |

```viper
T: tensor[float, 3, 2] = transpose(A)
R: tensor[float, 4, 1] = reshape(A, 4, 1)
val: float = A[0, 1]
n: int = len("hello")
```

### Type Rules

- No implicit type conversion (use casts)
- Binary arithmetic requires identical operand types for scalars
- Tensor literal shape must match annotated type
- Function calls resolve via `open` (unqualified) or `module.name` (qualified)
- Struct types are compared by name
- List element types must be uniform within a literal

## Standard Library

Installed under `VIPER_PATH` (default `/usr/local/lib/viper`):

- `math` — `add`, `sub`, `mul`, `div`, `min`, `max`
- `string` — `is_empty`, `is_not_empty`
- `io` — `read`, `write` wrappers around file builtins

```viper
open math
open io
x: int = math.max(3, 7)
text: string = io.read("data.txt")
```

## Comments

```viper
# line comment
```

## Toolchain

```bash
make
./build/bin/viper -p examples --run examples/globals.vp
./build/bin/viper -p examples -r --run examples/main.vp
./build/bin/viper --run examples/while.vp
./build/bin/viper --run examples/full_lang.vp
./build/bin/viper --repl
./build/bin/viper -o out.vbc examples/hello.vp
./build/bin/viperrun out.vbc
```

### CLI options

| Flag | Description |
|------|-------------|
| `-p`, `--project ROOT` | Module search root (default: parent of entry file, or `.`) |
| `-r` | Recursively discover `.vp` files under project root |
| `--run` | Compile and execute |
| `--repl` | Interactive REPL with accumulating source |
| `-o FILE.vbc` | Write bytecode output |

Single-file mode resolves only the entry file plus modules opened (directly or transitively). Directory mode treats each `.vp` file as an entry point.

## REPL

```bash
viper --repl
```

The REPL accumulates successful input across lines so declarations persist. Each non-empty line is compiled and run as part of the growing program. Empty lines are skipped. Exit with Ctrl-D.

## Multi-File Compilation

The compiler builds a module index from the project root, follows `open` statements to compute a dependency closure, registers exported functions and struct types in Pass 1, and links function bodies from the closure during bytecode generation.
