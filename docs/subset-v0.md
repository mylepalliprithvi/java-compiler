# v0 language subset

Scope for the first end-to-end pipeline (lexer → parser → sema → codegen → real
`.class` file runnable by `java`). Deliberately small: the goal of v0 is a
working pipeline, not language coverage. Everything not listed here is out of
scope until a later milestone (see "Deferred" below).

## Compilation unit

- One top-level `public` class per file, filename must match the class name
  (`Foo.java` → `class Foo`), matching real `javac` behavior.
- No `package` declaration, no `import` — everything resolves against the
  single class in the file plus `java.lang.System` / `java.io.PrintStream`
  (needed for `System.out.println`).

## Types

- Primitives: `int`, `boolean`, `double`, `void` (return type only).
- Class types: the declared class itself and, if present, its `extends`
  superclass — both are user classes compiled together or the implicit
  `java.lang.Object` root.
- No arrays, no `String` as a value type yet (only as a literal argument to
  `println`, handled as a special case in codegen).

## Class members

- Fields (instance only — no `static` fields in v0).
- Methods: `public`/private not distinguished yet (everything compiles as
  `public`); no overloading — each method name is unique per class for v0.
- Either an implicit no-arg constructor (calls `super()`) or a single
  user-written constructor.
- Single inheritance via `extends` (no `interface`, no `implements`).

## Statements

- Local variable declarations (`int x = 1;`)
- `if` / `else`
- `while`
- `for` (init; cond; update)
- Blocks `{ ... }`
- `return` (with or without a value)
- Expression statements (assignment, method calls)

## Expressions

- Literals: integer, floating point, boolean (`true`/`false`)
- Arithmetic: `+ - * / %`
- Relational: `< <= > >= == !=`
- Logical: `&& || !`
- Assignment: `=` (compound assignment `+=` etc. deferred)
- Field access (`this.x`, `obj.x`)
- Method calls (`obj.m(...)`, unqualified calls to methods on `this`)
- `new ClassName(...)`
- `this`
- `System.out.println(...)` — special-cased: accepts a single `int`,
  `double`, `boolean`, or string-literal argument, emitted as a real
  `invokevirtual` against `java.io.PrintStream`.

## Target class file

- Major version **49** (Java SE 5). This predates the JVM verifier's
  `StackMapTable` requirement (introduced at major version 50), so v0 can
  skip stack-map-frame computation entirely and still produce a class file
  that passes verification on a real JVM.

## Deferred (explicitly out of scope for v0 — future milestones, roughly in this order)

1. Arrays
2. `String` as a first-class type + `+` concatenation (desugars to
   `StringBuilder`)
3. `interface` / `implements`
4. Exceptions: `try` / `catch` / `finally`, `throw`
5. `static` fields and static initializer blocks
6. Method overloading resolution
7. Remaining operators: bitwise (`& | ^ ~`), shifts (`<< >> >>>`), ternary
   (`?:`), compound assignment
8. `switch`
9. `StackMapTable` generation (needed once targeting classfile 50+)
10. Generics (type erasure model)
11. Lambdas / functional interfaces (`invokedynamic`)
