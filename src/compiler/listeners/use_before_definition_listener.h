#ifndef USE_BEFORE_DEFINITION_LISTENER_H
#define USE_BEFORE_DEFINITION_LISTENER_H

#include "../errors/error_reporter.h"
#include "../symbols/symbol.h"
#include <cgullBaseListener.h>
#include <unordered_map>

class UseBeforeDefinitionListener : public cgullBaseListener {
public:
  UseBeforeDefinitionListener(ErrorReporter& errorReporter,
                              const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes);

private:
  ErrorReporter& errorReporter;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;
  std::shared_ptr<Scope> currentScope;

  void enterEveryRule(antlr4::ParserRuleContext* ctx) override;

  void enterVariable(cgullParser::VariableContext* ctx) override;
  void enterFunction_call(cgullParser::Function_callContext* ctx) override;
  void enterStruct_definition(cgullParser::Struct_definitionContext* ctx) override;
  void enterFunction_definition(cgullParser::Function_definitionContext* ctx) override;
  void enterCast_expression(cgullParser::Cast_expressionContext* ctx) override;
  void exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) override;

  std::pair<bool, std::shared_ptr<TypeSymbol>> isStructScope(std::shared_ptr<Scope> scope);
  std::pair<bool, std::shared_ptr<TypeSymbol>> isFunctionDefinitionScope(std::shared_ptr<Scope> scope);
};

#endif // USE_BEFORE_DEFINITION_LISTENER_H
