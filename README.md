# Viper

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![CI](https://github.com/Aksel588/Viper/actions/workflows/ci.yml/badge.svg)](https://github.com/Aksel588/Viper/actions/workflows/ci.yml)

Viper is a statically typed language and compiler for numeric and tensor workloads. Write `.vp` source files, compile with `viper`, and run programs directly or via bytecode with `viperrun`.

**Version:** 0.2.0  
**Author:** [Aksel Aghajanyan](https://github.com/Aksel588)

## Features

- Static typing for `int`, `float`, `string`, `bool`, `list[T]`, `struct`, and `tensor[...]`
- Logical operators: `!`, `&&`, `||` with short-circuit evaluation
- Loop control: `break` and `continue`
- Struct declarations, literals, and field access (`p.x`)
- List literals, indexing, and builtins `len` / `append`
- Native builtins: `abs`, `sqrt`, `floor`, `ceil`, `read_file`, `write_file`, `input`
- OCaml-style **modules** with `export fn`, `open`, and qualified calls (`math.add`)
- Dependency-aware linking — only modules in the `open` closure are compiled
- Bytecode compilation and stack VM execution
- Interactive REPL: `viper --repl`
- Source-linked diagnostics with caret snippets
- Standard library installed under `VIPER_PATH` (`math`, `string`, `io`)
- CLI flags: `--run`, `--repl`, `-p/--project`, `-r`, `-o`, `--verbose`, `--version`, `--help`

## Requirements

- **macOS or Linux**
- **GCC or Clang** (any C11-capable compiler)
- **GNU Make**
- **zsh** recommended (install script configures shell integration)

## Install

### One-line install

```bash
git clone https://github.com/Aksel588/Viper.git
cd Viper
./install.sh
source ~/.zshrc    # required once after install
viper --version    # should print: viper 0.2.0
```

By default, `./install.sh` installs to `~/.local` (no `sudo`). For a system-wide install:

```bash
sudo ./install.sh --system   # installs to /usr/local
source ~/.zshrc
```

### What gets installed

| Path | Contents |
|------|----------|
| `~/.local/bin/viper` | Compiler (default install) |
| `~/.local/bin/viperrun` | Bytecode runner |
| `~/.local/lib/viper/` | Standard library (`.vp` modules) |
| `/usr/local/bin/viper` | Compiler (`--system` install) |
| `/usr/local/lib/viper/` | Standard library (`--system` install) |

### Manual install

```bash
make all
make install PREFIX=$HOME/.local    # or: sudo make install
source ~/.zshrc
```

Equivalent shortcut:

```bash
make install-user
```

Test an install without writing to your home directory:

```bash
make install DESTDIR=/tmp/viper-install PREFIX=/usr/local
/tmp/viper-install/usr/local/bin/viper --version
```

### Uninstall

```bash
make uninstall PREFIX=$HOME/.local
# remove the marked block from ~/.zshrc (between "# >>> viper compiler >>>" and "# <<< viper compiler <<<")
```

## Python name conflict

A **Python package** also installs a command named `viper`. If you see:

```
ModuleNotFoundError: No module named 'main'
```

your shell is invoking the Python tool, not the Viper compiler.

`./install.sh` detects this and adds **zsh shell functions** to `~/.zshrc` that always call the C compiler:

```bash
# >>> viper compiler >>>
viper() {
  '$HOME/.local/bin/viper' "$@"
}
viperrun() {
  '$HOME/.local/bin/viperrun' "$@"
}
export PATH="$HOME/.local/bin:$PATH"
export VIPER_PATH="$HOME/.local/lib/viper"
# <<< viper compiler <<<
```

**Fix:** reload your shell after install:

```bash
source ~/.zshrc
# or open a new terminal tab, or:
exec zsh
```

Verify resolution:

```bash
command -v viper          # should show ~/.local/bin/viper or "viper is a shell function"
viper --version           # viper 0.1.0
whence -w viper           # viper: function  (in zsh)
```

If you cannot reload the shell, call the compiler directly:

```bash
~/.local/bin/viper --version
```

Run the bundled diagnostic from the repo:

```bash
./scripts/viper-path-debug.sh
```

## Quick start

Create `hello.vp`:

```viper
x: int = 42
print(x)
```

Compile and run:

```bash
viper --run hello.vp
```

Use the standard library:

```viper
open math
print(add(2, 3))
print(math.sub(10, 4))
```

### Multi-file project

`examples/main.vp`:

```viper
module main
open math

x: int = add(1, 1)
print(x)
```

```bash
viper -p examples -r --run examples/main.vp
```

The `-p` flag sets the project root for module discovery. `-r` recursively indexes all `.vp` files under that root.

## Language overview

Viper programs live in `.vp` files. Each file belongs to a **module** (defaults to the filename stem):

```viper
module math

export fn add(a: int, b: int) -> int {
    return a + b
}

fn helper() -> int {
    return 1    # private to this module
}
```

Open modules to use their exported symbols:

```viper
open math
result: int = add(1, 2)       # unqualified
alt: int = math.add(3, 4)     # qualified
```

Supported types:

| Type | Description |
|------|-------------|
| `int` | 64-bit integer |
| `float` | Floating point |
| `string` | UTF-8 string |
| `bool` | Boolean |
| `tensor[elem, d1, d2, ...]` | N-dimensional tensor |

Control flow includes `if`/`else`, `while`, and `for i in 0..10` loops.

Full reference: [docs/language-spec.md](docs/language-spec.md)

## Standard library

Built-in modules ship in `lib/viper/` and are installed to `VIPER_PATH`.

Current modules:

| Module | Exports |
|--------|---------|
| `math` | `add`, `sub` |

Project modules override stdlib modules with the same name when both are on the search path.

## Environment

| Variable | Default | Purpose |
|----------|---------|---------|
| `VIPER_PATH` | `/usr/local/lib/viper` | Directory containing standard library modules |

After `./install.sh`, `VIPER_PATH` is set in `~/.zshrc` to match your install prefix (typically `~/.local/lib/viper`).

For local development from the repo:

```bash
export VIPER_PATH="$PWD/lib/viper"
./build/bin/viper --run myprogram.vp
```

## CLI reference

```
viper [options] [path]
```

| Option | Description |
|--------|-------------|
| `path` | `.vp` file or directory to compile (default: `.`) |
| `-p`, `--project ROOT` | Module search root for your project |
| `-r` | Recursively discover `.vp` files under project root |
| `--run` | Compile and execute |
| `-o FILE.vbc` | Write bytecode output |
| `--verbose` | Print discovery and compilation steps to stderr |
| `--version` | Show version and exit |
| `-h`, `--help` | Show usage |

### Examples

```bash
# Run a single file
viper --run examples/hello.vp

# Project with explicit root
viper -p examples --run examples/globals.vp

# Recursive module discovery
viper -p examples -r --run examples/main.vp

# Compile to bytecode, then run with viperrun
viper -o /tmp/out.vbc hello.vp
viperrun /tmp/out.vbc

# Verbose discovery and compile log
viper --verbose -p examples -r --run examples/main.vp
```

### viperrun

```
viperrun FILE.vbc
```

Executes bytecode produced by `viper -o`.

## Building from source

```bash
make clean && make all && make test && make verify
```

| Target | Description |
|--------|-------------|
| `make all` | Build `viper`, `viperrun`, and unit tests |
| `make test` | Run unit tests |
| `make verify` | End-to-end checks on `examples/` |
| `make install` | Install binaries and stdlib (`PREFIX` configurable) |
| `make install-user` | Install to `~/.local` |
| `make uninstall` | Remove installed binaries and stdlib |
| `make viper` | Symlink `build/bin/viper` at repo root |
| `make viperrun` | Symlink `build/bin/viperrun` at repo root |

Binaries are written to `build/bin/`.

## Project layout

```
include/          Public headers
src/
  driver/         viper, viperrun entry points
  frontend/       lexer, parser, AST
  analysis/       types, symbol table, semantic analysis
  codegen/        bytecode compiler
  vm/             runtime and stack VM
  support/        arena, diagnostics, discovery, modules, paths
lib/viper/        Standard library (installed to VIPER_PATH)
examples/         Sample programs
tests/            Unit tests
docs/             Language specification
scripts/          Shell helpers (e.g. path diagnostic)
install.sh        Build and install script
```

## Examples in this repo

| File | Demonstrates |
|------|--------------|
| `examples/hello.vp` | Basic print |
| `examples/globals.vp` | Global variables |
| `examples/while.vp` | `while` loops |
| `examples/math.vp` | Module with `export fn` |
| `examples/main.vp` | Multi-module `open` |
| `examples/matmul.vp` | Tensor operations |
| `examples/tensor_ops.vp` | Tensor indexing |
| `examples/bad.vp` | Type error (used by `make verify`) |

Run all verified examples:

```bash
make verify
```

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions and pull request guidelines.

## License

Copyright 2026 [Aksel Aghajanyan](https://github.com/Aksel588)

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for the full license text.
