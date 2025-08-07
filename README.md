# cgull

This repository (will) contain the code for cgull, ~~a c++-inspired language~~, well before I pivoted to target JVM bytecode for time (class assignment).
See the [informal spec](informal_spec.md) for more details on the language (Language Definition - Examples and Semantics assignment).
See the [formal grammar](src/grammar/cgull.g4) for the formal definition of the language. (Language Definition - Formal Grammar assignment)

## Building

Java is required for the antlr generation and JASM for generating bytecode. Both jars are included for convenience.

The cmake project requires that the antlr4 cpp runtime be locatable. This has only been tested on macOS through homebrew (as seen by the hints) and should be similar for most Linux package managers, but I will see about making this work on Windows as well (for my own sanity too).

Assuming all dependencies are installed, the `src/run.sh` script will build and run the project. The argument for the file to parse is required. Please cd into the `src` directory before running the script.

TODO: windows script equivalent

Known working with Java 23.0.2

```bash
cd src
./run.sh <file> [--lexer | --parser | --semantic ]
```

## Manual Building/Running

If you have issues with the bootstrap makefile or run.sh script in general, you can use the following commands to build and run the project manually with cmake.

```bash
# generate makefile
cd src
mkdir build
cd build
cmake ..
# build the project
make
# run the compiler
cd ..
./build/cgull <source_file> [--lexer | --parser | --semantic]
# run the assembler and java runtime, if applicable
# if on windows, use jasm.bat instead of jasm
./thirdparty/jasm/bin/jasm -i out -o out Main.jasm
java -cp out Main
```
