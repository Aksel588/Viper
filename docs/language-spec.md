# Viper Language Specification

## Overview

Viper is a statically typed language for AI/ML and data-science research. Programs use `.vp` files organized into modules. Cross-file functions are linked via explicit `open` dependencies. Programs compile to bytecode and run on the `viperrun` VM.

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
| `tensor[elem, d1, d2, ...]` | N-dimensional tensor; `elem` is `int` or `float` |

Example: `tensor[float, 2, 3]` is a 2×3 matrix of floats.

## Statements

### Variable Declaration

```viper
x: int = 42
name: string = "viper"
A: tensor[float, 2, 2] = [[1.0, 2.0], [3.0, 4.0]]
```

### Assignment

```viper
x = 10
```

### Print

```viper
print(x)
print(A)
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

### Conditionals

```viper
if x > 0 {
    y = 1
} else {
    y = 0
}
```

### For Loops

```viper
for i in 0..10 {
    x = x + i
}
```

Range `start..end` is inclusive on both bounds. Loop variable is `int`.

## Expressions

### Operators

| Precedence | Operators |
|------------|-----------|
| Unary | `-` |
| Postfix | `[` index `]` |
| Matrix multiply | `@` |
| Multiplicative | `*`, `/` |
| Additive | `+`, `-` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |

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

### Tensor Builtins

```viper
T: tensor[float, 3, 2] = transpose(A)
R: tensor[float, 4, 1] = reshape(A, 4, 1)
val: float = A[0, 1]
```

### Type Rules

- No implicit type conversion (use casts)
- Binary arithmetic requires identical operand types for scalars
- Tensor literal shape must match annotated type
- Function calls resolve via `open` (unqualified) or `module.name` (qualified)

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
./build/bin/viper -o out.vbc examples/hello.vp
./build/bin/viperrun out.vbc
```

### CLI options

| Flag | Description |
|------|-------------|
| `-p`, `--project ROOT` | Module search root (default: parent of entry file, or `.`) |
| `-r` | Recursively discover `.vp` files under project root |
| `--run` | Compile and execute |
| `-o FILE.vbc` | Write bytecode output |

Single-file mode resolves only the entry file plus modules opened (directly or transitively). Directory mode treats each `.vp` file as an entry point.

## Multi-File Compilation

The compiler builds a module index from the project root, follows `open` statements to compute a dependency closure, registers exported functions in Pass 1, and links function bodies from the closure during bytecode generation.
