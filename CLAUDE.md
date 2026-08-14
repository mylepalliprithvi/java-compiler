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
- **M2 — Lexer: done.** `src/lexer/{Token,Lexer}.{hpp,cpp}` — hand-written
  scanner covering the v0 token set (keywords, identifiers, int/double
  literals, string literals with `\n \t \" \\` escapes, all v0 operators,
  punctuation, `//` and `/* */` comments). Never throws: unrecognized input
  produces an `Error` token plus a `LexError{line,col,message}` recorded via
  `Lexer::errors()`, so multiple diagnostics can be collected per file.
  14 unit tests in `tests/unit/lexer_test.cpp`, all passing.
- **M3 — Parser/AST: done.** AST types in `src/ast/` (`Type`, `Expr`, `Stmt`,
  `Decl`) using a small inheritance hierarchy with a `Kind` enum tag on each
  node (downcast via `static_cast` after a `switch` on `kind` — no visitor
  yet). `src/parser/Parser.{hpp,cpp}` is a recursive-descent parser with a
  precedence-climbing expression parser (assignment → `||` → `&&` →
  equality → relational → additive → multiplicative → unary → postfix →
  primary). Never throws out of `parseCompilationUnit()`: syntax errors
  throw an internal `ParseException` that unwinds to the nearest
  statement/member boundary, gets recorded as a `ParseError` via
  `Parser::errors()`, then parsing resumes (panic-mode recovery), so one
  file can report multiple syntax errors. `src/ast/AstPrinter.{hpp,cpp}`
  renders a tree as a deterministic S-expression string, used by parser
  tests for snapshot-style assertions instead of hand-building expected
  trees. 14 new tests in `tests/unit/parser_test.cpp` (28/28 total passing).

  Two decisions worth knowing about:
  - `Type` supports `ArrayOfClassRef` (e.g. `String[]`) purely so a real
    `main(String[] args)` signature can be *parsed* — array values
    (indexing, `new T[]`, etc.) are still unimplemented; semantic analysis
    (M4) should reject arrays anywhere except that one parameter position.
  - Unary `-` and `!` are supported even though `docs/subset-v0.md`'s
    expression list only mentions them in binary/logical form — treated as
    a necessary reading of the existing "arithmetic"/"logical" operator
    entries, not a scope expansion, since `x = -1;` needs to work.
- **M4 — Semantic analysis: done.** `src/sema/ClassTable.{hpp,cpp}` builds a
  symbol table for the single class in the compilation unit (fields,
  methods — no overloading, so a name is one symbol — and the constructor's
  parameter types); `src/sema/SemanticAnalyzer.{hpp,cpp}` does name
  resolution and type checking, attributing the AST in place rather than
  building a parallel typed tree: `Expr::resolvedType`/`typeResolved`,
  `NameExpr::refKind`/`slot`, `MethodCallExpr::callKind`,
  `LocalVarDeclStmt::slot`, `Param::slot`, and
  `MethodDecl`/`ConstructorDecl::maxLocals` are all filled in for codegen
  (M5) to read directly. 19 new tests in `tests/unit/sema_test.cpp` (47/47
  total passing).

  Notable decisions:
  - `System.out.println(...)` is matched as a fixed AST *shape*
    (`MethodCallExpr` named `println` whose target is `FieldAccessExpr`
    named `out` off a `NameExpr` named `System`) rather than resolved
    through the symbol table — `System`/`out` have no symbol-table entries
    in v0, so matching happens before the target would otherwise fail to
    resolve. See `SemanticAnalyzer::isPrintlnPattern`.
  - Local variable slots are assigned during sema (not deferred to codegen)
    since sema already walks scopes in order; `double` locals/params
    correctly consume 2 JVM local-variable slots. Slots are never reused
    across sibling blocks (simpler, slightly wasteful — fine for v0).
  - `extends` is recorded on `ClassTable` for codegen's benefit (the
    `super_class` constant pool entry, `invokespecial <init>` in
    constructors) but member lookup never walks into the superclass: v0
    has no multi-file compilation, so nothing is actually known about a
    superclass beyond its name. Calling an inherited method/field that
    isn't redeclared on the subclass will incorrectly report "unknown
    method/field" — a known v0 limitation, not a bug to fix later without
    also adding multi-file compilation.
  - `String` values type-check as an opaque `ClassRef("String")` — assignable
    to `String`-typed locals/fields/params and comparable with `==`/`!=`,
    but `+` concatenation is explicitly rejected with a "not supported yet"
    diagnostic (real string behavior is deferred, see subset-v0.md).
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
