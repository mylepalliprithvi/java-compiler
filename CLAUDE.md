# CLAUDE.md

Context for working on this project. It's a Java compiler written in C++20
that emits real JVM `.class` files, built incrementally as a learning
project (hand-written stages, no parser generators).

## Goal

Compile a small subset of Java source directly to JVM bytecode (`.class`
files) runnable by a real `java` binary — not an interpreter, not a
transpiler to another language. The full feature roadmap is deliberately
staged: get a minimal subset working end-to-end first, then grow the
language surface milestone by milestone.

See `docs/subset-v0.md` for the exact language subset targeted right now,
and the plan file `~/.claude/plans/stateful-crafting-lynx.md` for the full
milestone breakdown (M0–M7).

## Architecture (pipeline stages, one dir per stage under `src/`)

```
src/
  core/     shared utilities (currently just a version string)
  lexer/    source text -> token stream               [not yet implemented]
  ast/      AST node definitions                       [not yet implemented]
  parser/   tokens -> AST (recursive descent + Pratt)   [not yet implemented]
  sema/     symbol tables, type checking                [not yet implemented]
  codegen/  AST -> constant pool + bytecode -> .class    [not yet implemented]
  driver/   CLI entry point (src/driver/main.cpp)
```

`compiler_core` (CMake static library) holds every stage except the CLI
entry point, so `compiler_tests` can link against internals directly.

Key early design decision: target class file **major version 49** (Java SE
5). Versions 50+ require `StackMapTable` attributes for verification, which
means a full control-flow/type-frame inference pass. Version 49 predates
that requirement, so v0 can skip stack-map-frame computation and still
produce valid, verifiable class files. Revisit once the language grows past
what the old inference verifier can check.

## Build & test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
cd build && ctest --output-on-failure
```

Tests use [doctest](https://github.com/doctest/doctest), pulled in via
`FetchContent` (see `CMakeLists.txt`). Note: that FetchContent step needed
`set(CMAKE_POLICY_VERSION_MINIMUM 3.5)` because doctest v2.4.11's own
`cmake_minimum_required` predates CMake 4's policy floor.

`cmake` was not preinstalled on this machine (the `.idea/` folder is a
leftover from IntelliJ IDEA CE, not CLion) — it was installed via
`brew install cmake`.

End-to-end verification (once the pipeline exists) runs generated `.class`
files against the real JVM: `java`/`javac`/`javap` (OpenJDK 21) are
confirmed available locally, so there's no need to implement a Java runtime
— we lean on the real `java.lang`/`java.io` classes at runtime, and can use
`javap -v` to cross-check our output against `javac`'s for debugging.

## Status

- **M0 — Scaffolding: done.** Git repo initialized; CMake split into
  `compiler_core` (lib) + `compiler` (CLI driver) + `compiler_tests`
  (doctest); one smoke test passes; driver builds and runs (prints a usage
  message — the actual pipeline isn't wired up yet).
- **M1 — v0 language subset: done.** Documented in `docs/subset-v0.md`:
  single top-level class, `int`/`boolean`/`double`/`void` + class types,
  fields/methods/single constructor, single inheritance, core statements
  and expressions, `System.out.println` special-cased. Arrays, `String`,
  interfaces, exceptions, generics, lambdas, etc. explicitly deferred (full
  ordered list in that doc).
- **M2 — Lexer: not started.**
- **M3 — Parser/AST: not started.**
- **M4 — Semantic analysis: not started.**
- **M5 — Bytecode generation: not started.**
- **M6 — End-to-end verification harness + CLI: not started** (the CLI
  currently just prints a usage/error message).

## Conventions established so far

- One `.cpp`/`.hpp` pair per unit, colocated in its stage's directory under
  `src/` (e.g. `src/core/Version.{hpp,cpp}`), no separate top-level
  `include/`.
- Everything lives in the `jc` namespace.
- Unit tests live in `tests/unit/`, one binary (`compiler_tests`) built from
  all of them plus `tests/unit/test_main.cpp` (the doctest main).
  `tests/programs/` is reserved for end-to-end `.java` sample programs with
  expected stdout, added starting at M6.
