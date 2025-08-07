#include "error_reporter.h"
#include <algorithm>

void ErrorReporter::reportError(ErrorType type, int line, int column, const std::string& message) {
  errors.push_back({type, line, column, message});
}

void ErrorReporter::displayErrors(std::ostream& out) const {
  std::vector<CompilerError> sortedErrors = errors;
  std::sort(sortedErrors.begin(), sortedErrors.end(), [](const CompilerError& a, const CompilerError& b) {
    if (a.line != b.line)
      return a.line < b.line;
    return a.column < b.column;
  });
  for (const auto& error : sortedErrors) {
    out << "Line " << error.line << ":" << error.column << " - ";

    switch (error.type) {
    case ErrorType::LEXICAL_ERROR:
      out << "Lexical error: ";
      break;
    case ErrorType::SYNTAX_ERROR:
      out << "Syntax error: ";
      break;
    case ErrorType::REDEFINITION:
      out << "Redefinition: ";
      break;
    case ErrorType::REDECLARATION:
      out << "Duplicate definition: ";
      break;
    case ErrorType::UNRESOLVED_REFERENCE:
      out << "Unresolved reference: ";
      break;
    case ErrorType::USE_BEFORE_DEFINITION:
      out << "Usage before definition: ";
      break;
    case ErrorType::UNDEFINED_VARIABLE:
      out << "Undefined variable: ";
      break;
    case ErrorType::UNDEFINED_FIELD:
      out << "Undefined field: ";
      break;
    case ErrorType::TYPE_MISMATCH:
      out << "Type mismatch: ";
      break;
    case ErrorType::ACCESS_VIOLATION:
      out << "Access violation: ";
      break;
    case ErrorType::OUT_OF_BOUNDS:
      out << "Out of bounds: ";
      break;
    case ErrorType::ASSIGNMENT_TO_CONST:
      out << "Assignment to const: ";
      break;
    }

    out << error.message << std::endl;
  }
}

bool ErrorReporter::hasErrors() const { return !errors.empty(); }
