# cgull Informal Specification

Not all cases are covered in this document, but it should give you a good idea of the language and its philosophy (yet another C inspired language with some modern influences).

## Full list of examples

Below is a full list of the files available in the `examples` directory. These will be referenced and re-linked in the following list of functionalities.

1. [Dynamic Array](examples/ex1_dynamic_array.cgl)
2. ~~[First Class Functions]~~
3. [Functions](examples/ex3_functions.cgl)
4. [Branching](examples/ex4_branching.cgl)
5. [Looping](examples/ex5_looping.cgl)
6. [Structs and Tuples](examples/ex6_structs_tuples.cgl)
7. [Built-in Functions](examples/ex7_builtin.cgl)
8. [Types and Casting](examples/ex8_types_and_casting.cgl)
9. ~~[Exceptions]~~
10. ~~[Overloads]~~
11. [Operations](examples/ex11_operations.cgl)
12. ~~[Interfaces]~~

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
    i = i + 2;
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

Structs are treated similarly to classes like in C++. There is no inheritance. Interfaces are planned for the future.

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

  fn bar() -> void {}
}
```

Structs with no methods can be initialized with the following syntax:

```cgull
Foo f = Foo(1, 2, 3);
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

readline(); // reads a line from stdin
read(); // reads up to and discards whitespace from stdin
read("a"); // reads until 'a' is found from stdin and discards it
read("a", 10); // reads until 'a' is found from stdin and discards it, up to 10 characters
```

#### Casting

See [examples/ex8_types_and_casting.cgl](examples/ex8_types_and_casting.cgl) for casting in action.

Use "as" for explicit casting

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
- `int` - 32-bit signed integer
- `long` - 64-bit signed integer
- `float` - 64-bit floating point number

No current support for single unicode characters, may be added in the future.

#### Compound Types

```cgull
int* arr = allocate int[2]; // allocate an array of 2 integers on the heap
arr[0] = 1;
arr[1] = 2;
println(arr[0]); // prints 1
println(arr[1]); // prints 2
deallocate[] arr; // free the memory
```

##### Tuples

See [Composite/User-Defined Types](#compositeuser-defined-types) for tuples.

### Literals

- Signed integers: `0`, `1`, `-1`
- Floating point numbers: `0.0`, `1.42`, `-1.42`, `+inf`, `-inf`, `nan`
- Binary: `0b0`, `0b1`, `0b101010`, `0b1111_1111`
- Octal: `0o0`, `0o1`, `0o10`, `0o777` (note: no leading zeroes)
- Hexadecimal: `0x0`, `0x1`, `0x10`, `0xFF` (note: no leading zeroes)
- Strings: `"Hello, World!"`, `"Hello, \"World!\""`
  - An escaped double quote is represented as `\"`
- Booleans: `true`, `false`

### bits as

// probably scraping due to JVM target

For reinterpretation of bits, the `bits as` keyword combo is used. If there are more bits than the target type, the most significant bits are discarded. If there are fewer bits than the target type, the most significant bits are set to 0.

### $toString

All types have a `$toString` method that returns a string representation of the type. This can be overridden in structs. It is not always necessary to call `$toString` explicitly, as it is called automatically when a string is expected.

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
