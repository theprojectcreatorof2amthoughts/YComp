# YComp

YComp is a small, make-like build system configured with **`CompilFileY.compy`**.

## Build

YComp requires a C++17 compiler and CMake.

```sh
cmake -S . -B build
cmake --build build
```

The executable is named `ycomp` (or `ycomp.exe` on Windows).

## Quick start

Create a file named `CompilFileY.compy`:

```text
SHY -> gshai
SHYFLAG -> --shaywall --shaywextra --shayo2
SRC -> main.shay

!$all > test

test > !#(SRC)
	!#(SHY) !#(SHYFLAG) -o test !#(SRC)

clr >
	rm -f test
```

Run `ycomp` to build the first target, or `ycomp clr` to run the clean target.

## Syntax

- `NAME -> value` defines a variable.
- `!#(NAME)` expands a variable.
- `target > dependency1 dependency2` defines a target and its dependencies.
- `!$target > dependencies` defines a target that is always considered out of date. This is useful for aggregate targets such as `all`.
- Indented lines below a target are shell commands (recipes).
- The first target in the file is the default target.
- A target is rebuilt when its output does not exist or one of its dependencies is newer.
- A dependency without a rule must exist as a file.

Use `ycomp -f path/to/file.compy [target]` to select a different build file.

## Current scope

This is the initial implementation: variables, target dependencies, recipes, default-target selection, cycle detection, and timestamp-based incremental builds. Recipes are passed to the system shell. More advanced features can be added as the format evolves.
