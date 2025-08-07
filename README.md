# cgull

This repository (will) contain the code for cgull, ~~a c++-inspired language~~, well before I pivoted to target JVM bytecode for time (class assignment).
See the [informal spec](informal_spec.md) for more details on the language (Language Definition - Examples and Semantics assignment).
See the [formal grammar](src/grammar/cgull.g4) for the formal definition of the language. (Language Definition - Formal Grammar assignment)

## Temporary HW5 completion status

- [ ] Example 1: [ex1_dynamic_array.cgl](examples/ex1_dynamic_array.cgl)
- [x] Example 2: [ex2_misc1.cgl](examples/ex2_misc1.cgl)
- [x] Example 3: [ex3_functions.cgl](examples/ex3_functions.cgl)
- [x] Example 4: [ex4_branching.cgl](examples/ex4_branching.cgl)
- [x] Example 5: [ex5_looping.cgl](examples/ex5_looping.cgl)
- [x] Example 6: [ex6_math_structs.cgl](examples/ex6_math_structs.cgl)
- [x] Example 7: [ex7_builtin.cgl](examples/ex7_builtin.cgl)
- [x] Example 8: [ex8_types_and_casting.cgl](examples/ex8_types_and_casting.cgl)
- [ ] Example 9: [ex9_misc2.cgl](examples/ex9_misc2.cgl)
- [x] Example 10: [ex10_misc3.cgl](examples/ex10_misc3.cgl)
- [x] Example 11: [ex11_operations.cgl](examples/ex11_operations.cgl)
- [x] Example 12: [ex12_misc4.cgl](examples/ex12_misc4.cgl)
- [x] Example 13: [ex13_misc5.cgl](examples/ex13_misc5.cgl)
- [x] Example 14: [ex14_bool_ops_nested.cgl](examples/ex14_bool_ops_nested.cgl)

## Temporary note for grader

Please see the below sections for building, compiling, and assembling the source files.
I've attempted to make sure there's a high chance the cmake file will also find the ANTLR library for you, but you may need to modify it **especially if you are on Windows.**
It should describe everything you need to know, but the TL;DR if you are struggling to use the run.sh script for system environment reasons:

- Even though this is a C++ project the compiler targets the JVM.
- The antlr4-cpp-runtime for 4.13.2 is required
- Have Java installed (I used 23.0.2), there are no need to fetch dependencies these are located in the `thirdparty` directory.
- Use cmake to build the compiler
- Compile using the produced executable with an example file as an argument
- Assemble the produced .jasm files with the jasm tool
- Run the java runtime with the Main class as the argument and the out directory as the classpath

**See [Manual Building/Assembling/Running](#manual-buildingassemblingrunning) for more details about specific commands.**

## Building

Java is required for the ANTLR generation and JASM for generating bytecode. Both jars are included for convenience. If you must source them yourself, 4.13.2 is the expected ANTLR version and [this specific PR of JASM](https://github.com/roscopeco/jasm/pull/60) is the one that works with it (needed for empty package names on classes).

The cmake project requires that the antlr4 cpp runtime be locatable. This has only been tested on macOS through homebrew (as seen by the hints) and should be similar for most Linux package managers, but I will see about making this work on Windows as well (for my own sanity too).

Assuming all dependencies are installed, the `src/run.sh` script will build and run the project. The argument for the file to parse is required. Please cd into the `src` directory before running the script.

TODO: windows script equivalent

Known working with Java 23.0.2

```bash
cd src
./run.sh <file> [--lexer | --parser | --semantic ]
```

## Manual Building/Assembling/Running

If you have issues with the bootstrap makefile or run.sh script in general, you can use the following commands to build and run the project manually.

```bash
# --- compiling cgull ---
# generate makefile w/ cmake
cd src
mkdir build
cd build
cmake ..
# build the project
make

# --- running the cgull compiler ---
cd ..
./build/cgull <source_file> [--lexer | --parser | --semantic]

# --- running the jasm assembler ---
# if on windows, use `jasm.bat` instead of `jasm`
# run the assembler for EVERY .jasm file in the out directory (could do with a loop or something)
# !!! note that the input files are relative to the input directory specified with the i argument
./thirdparty/jasm/bin/jasm -i out -o out Main.jasm
./thirdparty/jasm/bin/jasm -i out -o out IntReference.jasm
./thirdparty/jasm/bin/jasm -i out -o out StringReference.jasm

# --- running the assembled class files ---
# run the class files, with the classpath being the out folder we just assembled the files in
java -cp out Main
```
