#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "errors/error_reporter.h"
#include "symbols/symbol.h"
#include <cgullParser.h>
#include <memory>

class SemanticAnalyzer {
public:
  SemanticAnalyzer();

  void analyze(cgullParser::ProgramContext* programCtx);
  ErrorReporter& getErrorReporter() { return errorReporter; }

  void printSymbolsAsJson(std::ostream& out = std::cout) const;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& getScopes();
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& getExpressionTypes();

private:
  ErrorReporter errorReporter;
  std::shared_ptr<Scope> globalScope;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;

  void addBuiltinFunctions();

  // JSON generation
  void printScopeAsJson(std::shared_ptr<Scope> scope, std::ostream& out, int indentLevel) const;
  std::vector<std::shared_ptr<Scope>> findChildScopes(std::shared_ptr<Scope> parent) const;
  std::string symbolTypeToString(SymbolType type) const;
  std::string getScopeName(std::shared_ptr<Scope> scope) const;
};

#endif // SEMANTIC_ANALYZER_H
