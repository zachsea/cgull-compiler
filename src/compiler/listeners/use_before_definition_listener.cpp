#include "use_before_definition_listener.h"

UseBeforeDefinitionListener::UseBeforeDefinitionListener(
    ErrorReporter& errorReporter, const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes)
    : errorReporter(errorReporter), scopes(scopes) {
  // start at global scope (nullptr context)
  auto it = scopes.find(nullptr);
  if (it != scopes.end()) {
    currentScope = it->second;
  }
}

void UseBeforeDefinitionListener::enterEveryRule(antlr4::ParserRuleContext* ctx) {
  auto it = scopes.find(ctx);
  if (it != scopes.end()) {
    currentScope = it->second;
  }
}

void UseBeforeDefinitionListener::enterVariable(cgullParser::VariableContext* ctx) {
  auto parent = ctx->parent;
  bool isAssignmentTarget = false;

  if (dynamic_cast<cgullParser::Variable_declarationContext*>(parent)) {
    // the variable is being declared, so skip
    isAssignmentTarget = true;
  } else if (auto assign = dynamic_cast<cgullParser::Assignment_statementContext*>(parent)) {
    if (assign->variable() == ctx) {
      isAssignmentTarget = true;
    }
  }

  if (isAssignmentTarget)
    return;

  if (!ctx->IDENTIFIER())
    return;
  std::string name = ctx->IDENTIFIER()->getSymbol()->getText();
  auto symbol = currentScope ? currentScope->resolve(name) : nullptr;
  if (symbol && !symbol->isDefined) {
    errorReporter.reportError(ErrorType::USE_BEFORE_DEFINITION, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "use of '" + name + "' before its definition");
  }
}

void UseBeforeDefinitionListener::enterFunction_call(cgullParser::Function_callContext* ctx) {
  if (!ctx->IDENTIFIER())
    return;
  std::string name = ctx->IDENTIFIER()->getSymbol()->getText();
  auto symbol = currentScope ? currentScope->resolve(name) : nullptr;
  if (symbol && !symbol->isDefined) {
    errorReporter.reportError(ErrorType::USE_BEFORE_DEFINITION, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "call to function '" + name + "' before its definition");
  }
}

void UseBeforeDefinitionListener::enterStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  // mark struct as defined at this point
  if (!ctx->IDENTIFIER())
    return;
  std::string name = ctx->IDENTIFIER()->getSymbol()->getText();
  auto symbol = currentScope && currentScope->parent ? currentScope->parent->resolve(name) : nullptr;
  if (symbol)
    symbol->isDefined = true;
}

void UseBeforeDefinitionListener::enterFunction_definition(cgullParser::Function_definitionContext* ctx) {
  // mark function as defined at this point
  if (!ctx->IDENTIFIER())
    return;
  std::string name = ctx->IDENTIFIER()->getSymbol()->getText();
  if (ctx->FN_SPECIAL())
    name = ctx->FN_SPECIAL()->getText() + name;
  auto symbol = currentScope && currentScope->parent ? currentScope->parent->resolve(name) : nullptr;
  if (symbol)
    symbol->isDefined = true;
}

void UseBeforeDefinitionListener::enterCast_expression(cgullParser::Cast_expressionContext* ctx) {
  if (!ctx->IDENTIFIER())
    return;
  std::string name = ctx->IDENTIFIER()->getSymbol()->getText();
  auto symbol = currentScope ? currentScope->resolve(name) : nullptr;
  if (symbol && !symbol->isDefined) {
    errorReporter.reportError(ErrorType::USE_BEFORE_DEFINITION, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "use of '" + name + "' before its definition");
  }
  // the type is a primitive, so no need to check it
}

void UseBeforeDefinitionListener::exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) {
  // mark variable as defined if assigned (not for index or dereference)
  if (ctx->variable() && ctx->ASSIGN()) {
    if (ctx->variable()->IDENTIFIER()) {
      std::string identifier = ctx->variable()->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope ? currentScope->resolve(identifier) : nullptr;
      if (varSymbol) {
        varSymbol->isDefined = true;
        varSymbol->definedAtLine = ctx->variable()->IDENTIFIER()->getSymbol()->getLine();
        varSymbol->definedAtColumn = ctx->variable()->IDENTIFIER()->getSymbol()->getCharPositionInLine();
      }
    }
  }
}

std::pair<bool, std::shared_ptr<TypeSymbol>> UseBeforeDefinitionListener::isStructScope(std::shared_ptr<Scope> scope) {
  for (const auto& [ctx, mappedScope] : scopes) {
    if (mappedScope == scope && dynamic_cast<cgullParser::Struct_definitionContext*>(ctx)) {
      auto structCtx = dynamic_cast<cgullParser::Struct_definitionContext*>(ctx);
      std::string structName = structCtx->IDENTIFIER()->getSymbol()->getText();

      if (scope->parent) {
        auto structSymbol = scope->parent->resolve(structName);
        if (structSymbol && structSymbol->type == SymbolType::STRUCT) {
          return {true, std::dynamic_pointer_cast<TypeSymbol>(structSymbol)};
        }
      }
      return {true, nullptr};
    }
  }
  return {false, nullptr};
}

std::pair<bool, std::shared_ptr<TypeSymbol>>
UseBeforeDefinitionListener::isFunctionDefinitionScope(std::shared_ptr<Scope> scope) {
  for (const auto& [ctx, mappedScope] : scopes) {
    if (mappedScope == scope && dynamic_cast<cgullParser::Function_definitionContext*>(ctx)) {
      auto functionCtx = dynamic_cast<cgullParser::Function_definitionContext*>(ctx);
      std::string functionName = functionCtx->IDENTIFIER()->getSymbol()->getText();

      if (scope->parent) {
        auto functionSymbol = scope->parent->resolve(functionName);
        if (functionSymbol && functionSymbol->type == SymbolType::FUNCTION) {
          return {true, std::dynamic_pointer_cast<TypeSymbol>(functionSymbol)};
        }
      }
      return {true, nullptr};
    }
  }
  return {false, nullptr};
}
