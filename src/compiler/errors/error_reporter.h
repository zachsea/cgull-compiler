#ifndef ERROR_REPORTER_H
#define ERROR_REPORTER_H

#include <iostream>
#include <string>
#include <vector>

enum class ErrorType {
  LEXICAL_ERROR, // to be used, not currently implemented w/ ANTLR
  SYNTAX_ERROR,  // neither is this
  REDECLARATION,
  REDEFINITION,
  UNRESOLVED_REFERENCE,
  USE_BEFORE_DEFINITION,
  UNDEFINED_VARIABLE,
  UNDEFINED_FIELD,
  TYPE_MISMATCH,
  ACCESS_VIOLATION,
  OUT_OF_BOUNDS,
  ASSIGNMENT_TO_CONST,
};

struct CompilerError {
  ErrorType type;
  int line;
  int column;
  std::string message;
};

class ErrorReporter {
public:
  ErrorReporter() = default;

  void reportError(ErrorType type, int line, int column, const std::string& message);
  void displayErrors(std::ostream& out = std::cerr) const;
  bool hasErrors() const;

private:
  std::vector<CompilerError> errors;
};

#endif // ERROR_REPORTER_H
