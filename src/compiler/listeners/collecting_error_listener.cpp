#include "collecting_error_listener.h"
#include <sstream>

void CollectingErrorListener::syntaxError(antlr4::Recognizer* /*recognizer*/, antlr4::Token* /*offendingSymbol*/,
                                          size_t line, size_t charPositionInLine, const std::string& msg,
                                          std::exception_ptr /*e*/) {
  std::ostringstream oss;
  oss << "line " << line << ":" << charPositionInLine << " " << msg;
  errors.push_back(oss.str());
}
