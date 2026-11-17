# Isolde Code Quality Enforcement

This document describes the strict C++ code-quality enforcement for the Isolde repository.

## Build System

The project uses **CMake** (`CMakeLists.txt` at the repository root). Configure with:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Enforced Limits

### clang-tidy (`.clang-tidy`)

| Check | Limit | Severity |
|-------|-------|----------|
| `readability-function-cognitive-complexity` | max 15 | **Error** (build-breaking) |
| `readability-function-size` (lines) | max 60 | **Error** (build-breaking) |
| `readability-function-size` (statements) | max 30 | Error |
| `readability-function-size` (branches) | max 10 | Error |
| `readability-function-size` (nesting depth) | max 3 | Error |
| `readability-function-size` (parameters) | max 5 | Error |
| `readability-function-size` (local variables) | max 8 | Error |
| All `bugprone-*` | enabled | Warning |
| All `performance-*` | enabled | Warning |
| All `modernize-*` | enabled | Warning |
| All other `readability-*` | enabled | Warning |

Two checks are configured as **WarningsAsErrors** so they fail the build:
- `readability-function-cognitive-complexity`
- `readability-function-size`

### Lizard (cyclomatic complexity)

| Metric | Limit |
|--------|-------|
| Cyclomatic complexity (CCN) | max 10 |
| Function length (NLOC) | max 60 lines |
| Function parameters | max 5 |

Run: `python tools/lizard-check.ps1` (Windows) or `python3 -m lizard src -l cpp -C 10 -L 60 -a 5` (Linux)

### Class-size checker (`tools/class-size-check.py`)

| Metric | Limit |
|--------|-------|
| Lines per class body | max 300 |
| Methods per class | max 20 |
| Data members per class | max 10 |
| Public methods per class | max 5 |

Run: `python tools/class-size-check.py src`

### Compiler warnings (`CMakeLists.txt`)

For GCC/Clang, the `isolde_core` target is compiled with:

```
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wnull-dereference
-Wdouble-promotion -Wformat=2 -Werror
```

`-Werror` is applied only to project code (`isolde_core`), not third-party or generated code. On MSVC, `/W4 /WX /permissive-` is used instead.

## Formatting

Formatting is **already handled separately** by Microsoft clang-format (`.clang-format` at repo root). No clang-format job is included in CI — formatting is enforced through the existing `.clang-format` configuration and editor integration (`.vscode/settings.json` with `editor.formatOnSave`).

## Doxygen documentation enforcement (`Doxyfile`, `tools/documentation-check.py`)

| Check | Requirement | Severity |
|-------|-------------|----------|
| Class doxygen comment | Every class/struct must have a `/** ... */` block before its declaration explaining its purpose | **Error** (CI-failing) |
| Function doxygen comment | Every function definition must have `/** ... */` with `@brief` (purpose), `@details` (how it works), and usage context | **Error** (CI-failing) |
| No inline doxygen at call sites | Doxygen blocks must not appear immediately before function call expressions | **Error** (CI-failing) |
| No useless comments | Filler comments (e.g. "TODO", "fixme", "hack") without substance are flagged | Warning |

### Required doxygen structure

**Class:**
```cpp
/**
 * @brief One-line purpose of the class.
 *
 * @details Multi-paragraph explanation of what the class owns, its lifetime
 *          responsibilities, and how it interacts with other components.
 */
class GameLibrary { ... };
```

**Function:**
```cpp
/**
 * @brief One-line purpose of the function.
 *
 * @details How the function works internally, including any preconditions,
 *          postconditions, side effects, and thread-safety guarantees.
 *
 * @param name Description of parameter `name`.
 * @return Description of the return value.
 */
bool loadGame(const QString& id);
```

Run: `python tools/documentation-check.py src`

### What is NOT allowed

- Doxygen blocks placed immediately before a function **call** (call site)
- Comments that only restate the obvious (e.g. `// increment i`)
- `/** */` blocks with no substantive content (empty `@brief`, no `@details`)

## Local Usage

### Prerequisites

- Python 3 (for lizard, class-size checker, documentation checker)
- clang-tidy (from LLVM, e.g. `C:\Program Files\LLVM\bin\clang-tidy.exe`)
- CMake and Qt6 (for build + clang-tidy compilation database)
- doxygen (for documentation generation / CI only)

### Run all checks

**Windows (PowerShell):**

```powershell
# Lizard complexity
python tools/lizard-check.ps1

# Class-size
python tools/class-size-check.py src

# Documentation
python tools/documentation-check.py src

# clang-tidy (requires build directory with compile_commands.json)
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build --config-file .clang-tidy src
```

**Linux:**

```bash
# Lizard complexity
python3 -m lizard src -l cpp -C 10 -L 60 -a 5

# Class-size
python3 tools/class-size-check.py src

# Documentation
python3 tools/documentation-check.py src

# clang-tidy (requires build directory)
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j$(nproc)
clang-tidy -p build --config-file .clang-tidy src
```


## CI Enforcement

GitHub Actions (`.github/workflows/code-quality.yml`) runs on every push and pull request:

1. Configures CMake with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
2. Builds the project
3. Runs clang-tidy
4. Runs Lizard
5. Runs the class-size checker
6. Runs the class-size checker
7. Runs the documentation checker
8. Fails the pull request if any check fails

No clang-format job is included — formatting is handled separately.

## Baseline Handling

A baseline file (`tools/code-quality-baseline.json`) documents pre-existing violations found before enforcement was enabled:

- **Lizard:** 343 warnings across the codebase
- **Class-size:** 9 violations across 8 files
- **clang-tidy:** not run locally (CMake/Qt6 unavailable on this host)

The baseline:
- Is committed to the repository
- Prevents new violations from being introduced
- Does **not** hide violations in files modified by a pull request — changed files are checked in full
- Does **not** weaken limits; existing code is expected to be refactored over time

## Refactoring Guidance

### Function exceeds complexity or size limits

1. **Extract helper functions:** Move blocks of logic into private/static helper functions with descriptive names.
2. **Reduce nesting:** Replace deeply nested `if`/`for`/`while` with early returns or guard clauses.
3. **Reduce parameters:** Group related parameters into a struct or pass a context object.
4. **Reduce variables:** Limit local variable scope; reuse variables where safe.
5. **Split switch/if chains:** Use dispatch tables or strategy objects.

### Class exceeds size limits

1. **Extract collaborator classes:** Move related methods and data members into a new class that the original class composes.
2. **Extract implementation details:** Move private helpers into a pImpl or detail namespace.
3. **Split responsibilities:** Separate data storage from business logic (e.g., a `GameLibrary` split into `GameLibraryImporter` and `GameLibraryCollections`).
4. **Use free functions:** Convert stateless methods to free functions in an anonymous namespace.

### Example: GameLibrary refactoring

`GameLibrary` (3,871 lines) was split into:
- `GameLibrary.cpp` — core catalog queries (~1,485 lines)
- `GameLibrary_importer.cpp` — import/install flows (~1,300 lines)
- `GameLibrary_collections.cpp` — collection/playlist management (~900 lines)

All files remain under 1,500 lines; each class declaration is unchanged.

## Strict vs Informational

| Check | Strictness |
|-------|------------|
| clang-tidy cognitive complexity | **Error** (build-breaking) |
| clang-tidy function size | **Error** (build-breaking) |
| clang-tidy other checks | Warning |
| Lizard complexity/length/parameters | **Error** (CI-failing) |
| Class-size checker | **Error** (CI-failing) |
| Compiler warnings (`-Werror`) | **Error** (build-breaking) |
| clang-format | Separate (Microsoft style, `.clang-format`) |

## Limitations

- The class-size checker uses a conservative regex-based parser (libclang Python bindings are not available). It may miss some edge cases with templates, macros, or complex constructor syntax. It avoids counting comments and string literals where practical.
- clang-tidy was not run locally because CMake and Qt6 are not installed on this Windows host. The baseline for clang-tidy will be established on the first CI run.
- `-Werror` compiler warnings are applied only to the `isolde_core` target; test executables and third-party code are not affected.
