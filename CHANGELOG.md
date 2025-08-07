# Changelog

## [HW2]

### Added

- Formal definition of the cgull language in [cgull.g4](src/grammar/cgull.g4).
- Project to build antlr4 parser and lexer using antlr4-cpp-runtime.

### Changed

- Updated 2DVector struct's name in [example 10](examples/ex10_overloads.cgl) to `Vector2D` to match the naming convention used in the rest of the codebase (grammar does not allow for numeric prefixes).
- Changed destructuring declarations to require specifying the types of all variables in the list unless already declared in the same scope in [example 6](examples/ex6_structs_tuples.cgl).
- Moved stack array definition brackets to the type instaed of the variable name in [example 2](examples/ex2_first_class_functions.cgl).
- Modified if expressions to require parentheses if more than one expression makes up the result or condition in [example 1](examples/ex1_dynamic_array.cgl).

### Fixed

- Missing semicolon for when expr in [example 4](examples/ex4_branching.cgl).
- Missing closing multiline comment in [example 8](example/ex8_types_and_casting.cgl).
