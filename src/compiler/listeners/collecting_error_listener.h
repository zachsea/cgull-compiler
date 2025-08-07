#pragma once

#include <antlr4-runtime.h>
#include <string>
#include <vector>

class CollectingErrorListener : public antlr4::BaseErrorListener {
public:
  std::vector<std::string> errors;
  void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                   size_t charPositionInLine, const std::string& msg, std::exception_ptr e) override;
};
