# cgull Informal Specification

Not all cases are covered in this document, but it should give you a good idea of the language and its philosophy (yet another C inspired language with some modern influences).

## Full list of examples

Below is a full list of the files available in the `examples` directory. These will be referenced and re-linked in the following list of functionalities.

1. [Dynamic Array](examples/ex1_dynamic_array.cgl)
2. [First Class Functions](examples/ex2_first_class_functions.cgl)
3. [Functions](examples/ex3_functions.cgl)
4. [Branching](examples/ex4_branching.cgl)
5. [Looping](examples/ex5_looping.cgl)
6. [Structs and Tuples](examples/ex6_structs_tuples.cgl)
7. [Built-in Functions](examples/ex7_builtin.cgl)
8. [Types and Casting](examples/ex8_types_and_casting.cgl)
9. [Exceptions](examples/ex9_exceptions.cgl)
10. [Overloads](examples/ex10_overloads.cgl)
11. [Operations](examples/ex11_operations.cgl)
12. [Interfaces](examples/ex12_interfaces.cgl)

## Functionalities

### Entry Point

See any of the examples (e.g. [examples/ex1_dynamic_array.cgl](examples/ex1_dynamic_array.cgl)) to see a main function in action.

cgull requires a named `main` function of type `int` to be defined somewhere (once) in the program.
Just as with many other languages, the return value of `main` is the exit code of the program.

```cgull
fn main() -> int {
  println("Hello, World!");
  return 0;
}
```

### Functions

See [examples/ex3_functions.cgl](examples/ex3_functions.cgl) for functions in action.

```cgull
fn countup(int n) -> void {
  if (n > 0) {
    countdown(n - 1);
  }
  println(n);
}

fn addSub(int a, int b) -> (int, int) {
  return (a + b, a - b);
}

fn useless() -> int {
  return 0;
}

// using multiple return values

int a, int b = addSub(5, 3);
println(a); // prints 8
println(b); // prints 2

// alternatively, store in a tuple
tuple<int, int> t = addSub(5, 3);

```

### Branching

See [examples/ex4_branching.cgl](examples/ex4_branching.cgl) for branching in action.

cgull supports typical branching statements like `if`, `else if`, and `else`.

```cgull
if (condition) {
  // do something
} else if (condition) {
  // do something else
} else {
  // do something else
}
```

`when` acts like a switch statement with more flexibility. It can be used with ranges and negations (similar to Kotlin and Rust). Only works with numerics.

```cgull
when (c) {
  1 => println("one"),
  2 => println("two"),
  3..=5 => println("three to five"),
  6..10 => println("six to nine"),
  !15..20 => println("not 15 to 19"),
  _ => println("other")
}
```

`when` can also be used as an expression, similar to a match statement in Rust. It can return a value based on the case that matches.

```cgull
string x = 0;
x = when (x) {
  0 => "zero",
  1 => "one",
  2 => "two",
  _ => {
    // multi-statement support, requires return statement
    return "other";
  }, // trailing comma is optional
}
```

### Looping

See [examples/ex5_looping.cgl](examples/ex5_looping.cgl) for looping in action.

Very similar to Go, no while loops, only for loops, break and continue.

```cgull
for (int i = 0; i < 10; i++) {
  println(i);
}

// while loop

int i = 0;
for (i < 10) {
  println(i);
  if (i == 5) {
    i += 2;
    continue;
  }
  i++;
}

// infinite loop

for {
  println("Hello, World!");
  break;
}

// do while loop

for {
  // stmts
} until (condition);

```

### Composite/User-Defined Types

See [examples/ex6_structs_tuples.cgl](examples/ex6_structs_tuples.cgl) for composite types in action.

cgull supports composite types like structs and tuples.

```cgull
tuple<int, int> t = (1, 2);

// can be destructured
int a, int b = t;
a, b = t;
```

```cgull
struct Foo {
  int x;
  int y;
  int z;
}
```

### More on structs (adding methods to scoped data)

Previously linked example applies.

Structs are treated similarly to classes like in C++. They can also take interfaces as parameters. There is no inheritance.

```cgull
struct Foo {
  public int x;
  int y; // default is public
  private int z;

  private {
    int a;
    int b;
    fn foo() -> void {}
  }

  static fn bar() -> void {}
}
```

See [examples/ex10_overloads.cgl](examples/ex10_overloads.cgl) for operator overloading in action. This includes the $toString method discussed [later](#tostring).

```cgull
// operator overloading
struct Foo {
  // ...
  fn $operator+(Foo* other) -> Foo {
    return Foo(this->x + other.x, this->y + other.y, this->z + other.z);
  }
}
```

Structs with no methods can be initialized with the following syntax:

```cgull
Foo f = Foo(1, 2, 3);
```

They can also be initialized with tuples.

```cgull
tuple<int, int, int> t = (1, 2, 3);
Foo f = Foo(t); // f.x = 1, f.y = 2, f.z = 3
```

### Strongly Typed

```cgull
int x = 0;
// x = 1.0; // error: cannot convert from float to int
int y = 1.0; // error: cannot convert from float to int
int z = 1; // ok: implicit conversion from int to float, etc.
```

### Field Access

One example showing field access is [examples/ex6_structs_tuples.cgl](examples/ex6_structs_tuples.cgl).

Structs are accessed using the dot operator.

```cgull
Foo f;
f.x = 1;
println(f.x); // prints 1

f.someFunction(); // calls someFunction() on f
```

An array is a pointer to the first element, the square brackets operator is equivalent to pointer arithmetic: `arr[i]` is equivalent to `*(arr + i)`

This can be seen in [examples/ex1_dynamic_array.cgl](examples/ex1_dynamic_array.cgl).

```cgull
int arr[10];
arr[0] = 1;
println(arr[0]); // prints 1
```

Tuples are accessed via the square brackets operator, similar to arrays.

```cgull
tuple<int, int> t = (1, 2);
println(t[0]); // prints 1
println(t[1]); // prints 2
```

#### Interfaces

See [examples/ex12_interfaces.cgl](examples/ex12_interfaces.cgl) for interfaces in action.

```cgull
interface IFoo<T> {
  fn foo(T x) -> T;
  fn bar() -> void;
}
```

```cgull
struct Foo : IFoo<int> {
  fn foo(int x) -> int {
    return x + 1;
  }

  fn bar() -> void {
    println("bar");
  }
}
```

Interfaces do not need to be generic.

```cgull
interface IFoo {
  fn foo() -> void;
  fn bar() -> void;
}
```

### Commenting

```cgull
// This is a single line comment
/* This is a multi-line comment
   that spans multiple lines */
```

### Built-in

See [examples/ex7_builtin.cgl](examples/ex7_builtin.cgl) for built-in functions in action.

#### I/O

```cgull
println("Hello, World!"); // prints to stdout
print("Hello, World!"); // prints to stdout without newline
print("Hello, World!", '\t'); // prints to stdout with tab at end

readline(); // reads a line from stdin
read(); // reads up to and discards whitespace from stdin
read('a'); // reads until 'a' is found from stdin and discards it
read('a', 10); // reads until 'a' is found from stdin and discards it, up to 10 characters
```

#### Casting

See [examples/ex8_types_and_casting.cgl](examples/ex8_types_and_casting.cgl) for casting in action.

Use "as" for explicit casting

```cgull
// implicit
char c = 'a';
int i = c; // ok: implicit conversion from char to int

// explicit
int i = 1;
char c = i as char; // ok: explicit conversion from int to char
```

#### Misc

```cgull
// get the size of a type
int size = sizeof(int); // size is 4
// get the size of a variable
int size = sizeof(x); // size is 4
// get the size of a struct
int size = sizeof(Foo); // size is 12 (3 ints)
```

### Primitives

See [examples/ex8_types_and_casting.cgl](examples/ex8_types_and_casting.cgl) for primitives in action.

#### Scalar Types

- `bool` - 8-bit (conceptually 1-bit) boolean (true/false)
- `char` - 8-bit signed integer (ASCII character)
- `unsigned char` - 8-bit unsigned integer (ASCII character)
- `short` - 16-bit signed integer
- `unsigned short` - 16-bit unsigned integer
- `int` - 32-bit signed integer
- `unsigned int` - 32-bit unsigned integer
- `long` - 64-bit signed integer
- `unsigned long` - 64-bit unsigned integer
- `float` - 32-bit floating point number
- `double` - 64-bit floating point number

No current support for single unicode characters, may be added in the future.

#### Compound Types

```cgull
int arr[10]; // array of 10 integers
int arr[10] = {1, 2, 3, 4, 5}; // array of 10 integers with initial values (rest are 0)
```

##### Tuples

See [Composite/User-Defined Types](#compositeuser-defined-types) for tuples.

### Literals

- Signed integers: `0`, `1`, `-1`
- Unsigned integers: `u0`, `u5400`
  - Note: There is no implicit conversion between signed and unsigned integers, call casting functions to convert between them.
- Floating point numbers: `0.0`, `1.42`, `-1.42`, `+inf`, `-inf`, `nan`
- Characters: `'a'`, `'\n'`, `'\t'`, `'\0'`, `'\xFF'`
  - An escaped single quote is represented as `'\''`
- Binary: `0b0`, `0b1`, `0b101010`, `0b1111_1111`
- Octal: `0o0`, `0o1`, `0o10`, `0o777` (note: no leading zeroes)
- Hexadecimal: `0x0`, `0x1`, `0x10`, `0xFF` (note: no leading zeroes)
- Strings: `"Hello, World!"`, `"Hello, \"World!\""`
  - An escaped double quote is represented as `\"`
- Booleans: `true`, `false`

### First class functions

See [examples/ex2_first_class_functions.cgl](examples/ex2_first_class_functions.cgl) for first class functions in action.

cgull supports first class functions, meaning that functions can be passed as arguments to other functions, returned from functions, and assigned to variables. The signature must be specified for parameters and return types, but you can use just `fn` for syntactic sugar.

```cgull
// Function with int param and void return type
fn<int> foo = (int a) {
  println("Hello, World! " + a);
}

// Function with int param and int return type
fn<int -> int> bar = (int x) {
  return x + 1;
}
int result = bar(5); // result is 6

// Multiple parameters and return types
fn<int, int -> int, int> baz = (int x, int y) {
  return x + 1, y + 1;
}

// Syntactic sugar for function signature
fn sugar = (int a) -> void {
  println("Hello, World! " + a);
}
sugar(5); // prints "Hello, World! 5"

// Function that takes a function as a parameter
fn<int -> int> apply(fn<int -> int> f, int x) -> int {
  return f(x);
}
```

### bits as

For reinterpretation of bits, the `bits as` keyword combo is used. If there are more bits than the target type, the most significant bits are discarded. If there are fewer bits than the target type, the most significant bits are set to 0.

```cgull

char x = 0b00000001; // x is 1
float y = x bits as float; // y is 0b0000_0000_0000_0000_0000_0000_0000_0001, or 1e-45
println(x bits as float); // prints 1e-45
```

### $toString

All types have a `$toString` method that returns a string representation of the type. This can be overridden in structs. It is not always necessary to call `$toString` explicitly, as it is called automatically when a string is expected.

### Exceptions

See [examples/ex9_exceptions.cgl](examples/ex9_exceptions.cgl) for exceptions in action.

cgull supports exceptions for error handling. The `try` block is used to catch exceptions, and the `handle` block is used to handle them. The `else` block is executed if no exception is thrown, and the `finally` block is always executed.

```cgull
try {
  // stmts
} handle (exception e) {
  // "exception" is a catch-all type, specifying a type is more granular
  // exception<logic_error> e;
  println(e.$toString());
} else {
  // this block is executed if no exception is thrown
  // it can be placed below finally, but not before handle
  println("else");
} else if (condition) {
  // this block is executed if no exception is thrown and the condition is true
  // it can also be placed below finally, but not before handle
} finally {
  println("finally");
}
```

```cgull
throw exception("Error message");
// type can be any string which can be specified on the interface for the user to be aware of
throw exception<logic_error>("Error message");
```

### References/Heap

There are no syntatic sugar references like C++, it takes the C approach of using only pointers. The `&` operator is used to get the address of a variable, and the `*` operator is used to dereference a pointer.

```cgull
int x = 0;
int* p = &x; // p is a pointer to x
*p = 1; // x is now 1
```

Use "allocate" to allocate memory on the heap. The `deallocate` function is used to free memory.

```cgull
int* p = allocate int[10]; // allocate an array of 10 integers on the heap
deallocate[] p; // free the memory
```
