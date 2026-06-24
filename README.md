# rain

A small dynamically typed programming language with a **single-pass bytecode compiler** and stack-based virtual machine, written in C.

Source code is scanned, parsed, and compiled to bytecode in one walk — no separate AST phase. The VM executes that bytecode with closures, classes, inheritance, and mark-and-sweep garbage collection.

## Quick start

### Prerequisites

- `clang` (or another C compiler)
- `make`

### Build

```bash
make
```

This produces the `rain` executable in the project root.

### Run

**REPL** — interactive prompt:

```bash
./rain
```

**Script file**:

```bash
./rain hello.rn
```

### Exit codes

| Code | Meaning |
|------|---------|
| `0`  | Success |
| `64` | Invalid usage |
| `65` | Compile error |
| `70` | Runtime error |
| `74` | Could not read file |

---

## Language overview

Rain is expression-oriented with statement syntax similar to languages like Lox. Programs use semicolons, curly braces for blocks, and parentheses around `if` / `while` conditions.

### Hello world

```rain
print "Hello, world!";
```

### Comments

Line comments start with `//`:

```rain
// this is a comment
var x = 42;
```

---

## Types

| Type | Literals / creation | Notes |
|------|---------------------|-------|
| `nil` | `nil` | Default value for uninitialized `var` |
| Boolean | `true`, `false` | |
| Number | `42`, `3.14` | IEEE doubles |
| String | `"hello"` | Immutable, supports `+` concatenation |
| Function | `fun name() { ... }` | First-class, supports closures |
| Class | `class Name { ... }` | Callable as constructor |
| Instance | `Name(args)` | Fields + methods |

### Truthiness

Only `nil` and `false` are falsey. Everything else is truthy (including `0`).

### Equality

`==` compares values. Numbers compare by value; strings compare by content.

---

## Variables

Declare with `var`. Globals live at the top level; locals are created inside blocks `{ }`.

```rain
var count = 10;
var name;        // defaults to nil

{
    var count = 1;   // shadows outer count inside this block
    print count;     // 1
}

print count;     // 10
```

Assignment uses `=`:

```rain
var x = 1;
x = x + 1;
```

---

## Expressions

### Arithmetic

```rain
print 1 + 2 * 3;    // 7
print 10 / 4;       // 2.5
print -5;
```

### Strings

```rain
print "foo" + "bar";   // foobar
```

### Comparison

```rain
print 3 < 5;           // true
print 3 == 3;          // true
print 3 != 4;          // true
```

### Logical operators

`and` and `or` short-circuit:

```rain
var ok = false or print "skipped";   // does not print
var no = true and print "runs";        // prints, then assigns
```

### Grouping

```rain
print (1 + 2) * 3;   // 9
```

---

## Control flow

### `if` / `else`

Condition must be wrapped in parentheses:

```rain
if (age >= 18) {
    print "adult";
} else {
    print "minor";
}
```

### `while`

```rain
var i = 0;
while (i < 3) {
    print i;
    i = i + 1;
}
```

### `for`

C-style three-part loop:

```rain
for (var i = 0; i < 5; i = i + 1) {
    print i;
}
```

The initializer, condition, and increment are all optional:

```rain
var n = 0;
for (; n < 3;) {
    print n;
    n = n + 1;
}
```

---

## Functions

Define with `fun`. Functions are closures — they capture variables from enclosing scopes.

```rain
fun makeGreeter(greeting) {
    fun greet(name) {
        print greeting + ", " + name + "!";
    }
    return greet;
}

var sayHi = makeGreeter("Hi");
sayHi("Rain");
```

### Parameters and return

- Up to **255** parameters per function.
- `return;` returns `nil`.
- `return expr;` returns a value.
- Top-level `return` is not allowed.

```rain
fun add(a, b) {
    return a + b;
}

print add(2, 3);   // 5
```

---

## Classes and objects

### Defining a class

```rain
class Person {
    init(name) {
        this.name = name;
    }

    greet() {
        print "Hi, I'm " + this.name;
    }
}

var p = Person("Alex");
p.greet();
```

- `init` is the initializer (constructor). It runs when you call `ClassName(args)`.
- `this` refers to the current instance inside methods.
- Instance fields are set with `this.field = value` and read with `this.field` or `instance.field`.

### Inheritance

Use `<` to inherit from a superclass:

```rain
class Animal {
    init(name) {
        this.name = name;
    }

    speak() {
        print this.name + " makes a sound.";
    }
}

class Dog < Animal {
    init(name) {
        super.init(name);
    }

    speak() {
        print this.name + " barks.";
    }
}

Dog("Rex").speak();
```

### `super`

Inside a subclass, `super` calls methods on the parent class:

```rain
class Child < Parent {
    method() {
        super.method();          // call parent method
        var fn = super.method;   // get parent method (bound)
    }
}
```

`super` is only valid inside a class that has a superclass.

### Method calls

```rain
instance.method(arg1, arg2);
```

Fields shadow methods — if an instance has a field with the same name as a method, the field is used when invoked.

---

## Built-in functions

| Name | Description |
|------|-------------|
| `clock()` | Returns seconds (as a number) since the process started |

```rain
var start = clock();
// ... do work ...
print clock() - start;
```

---

## Printing

`print` evaluates an expression and writes it to stdout, followed by a newline:

```rain
print 42;
print "hello";
print true;        // true
print nil;         // nil
print Dog;         // Dog (class name)
```

---

## Project structure

```
src/
  scanner.c    Lexer — source → tokens
  compiler.c   Single-pass parser + bytecode emitter
  chunk.c      Bytecode buffer and constant pool
  vm.c         Stack VM interpreter
  object.c     Strings, functions, classes, instances
  table.c      Hash tables for globals and fields
  memory.c     Allocator + mark-and-sweep GC
  value.c      Runtime value representation
  debug.c      Disassembler (optional)
  main.c       REPL and file runner
hello.rn       Example program (inheritance demo)
Makefile
```

### How it works

1. **Scan** — `scanner.c` tokenizes the source (keywords, literals, operators).
2. **Compile** — `compiler.c` uses a Pratt parser to emit opcodes directly into a `Chunk` (no AST).
3. **Execute** — `vm.c` runs the bytecode on a value stack with call frames for functions and methods.
4. **Collect** — `memory.c` reclaims unreachable objects via mark-and-sweep GC.

---

## Debugging

Uncomment flags in `src/common.h` and rebuild:

| Flag | Effect |
|------|--------|
| `DEBUG_PRINT_CODE` | Disassemble compiled bytecode after each compile |
| `DEBUG_TRACE_EXECUTION` | Trace VM instructions and stack during execution |
| `DEBUG_STRESS_GC` | Run GC on every allocation (stress test) |
| `DEBUG_LOG_GC` | Log GC activity |

```bash
make clean && make
```

---

## Example

See [`hello.rn`](hello.rn) for a full inheritance example with `Animal`, `Dog`, and `GuideDog`:

```bash
./rain hello.rn
```

```
Rex barks.
Buddy barks.
Buddy is also a guide dog.
```

---

## License

Add a license here if you plan to open-source the project.
