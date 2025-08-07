#include "symbol_collection_listener.h"
#include "../errors/error_reporter.h"
#include <memory>

SymbolCollectionListener::SymbolCollectionListener(ErrorReporter& errorReporter, std::shared_ptr<Scope> existingScope)
    : errorReporter(errorReporter) {
  if (existingScope) {
    currentScope = existingScope;
  } else {
    currentScope = std::make_shared<Scope>(nullptr);
  }
  globalScope = currentScope;
  scopes[nullptr] = globalScope;
}

std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> SymbolCollectionListener::getScopeMapping() {
  return scopes;
}

std::shared_ptr<Scope> SymbolCollectionListener::getCurrentScope() { return currentScope; }

/* rules that define symbols */

std::shared_ptr<VariableSymbol> SymbolCollectionListener::createAndRegisterVariableSymbol(
    const std::string& identifier, cgullParser::TypeContext* typeCtx, bool isConst, int line, int column) {

  auto varSymbol = std::make_shared<VariableSymbol>(identifier, line, column, currentScope);

  std::shared_ptr<Type> resolvedType = resolveType(typeCtx);

  if (!resolvedType) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, line, column, "unresolved type " + typeCtx->getText());
    resolvedType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);
  }

  varSymbol->dataType = resolvedType;
  varSymbol->isConstant = isConst;
  varSymbol->isPrivate = inPrivateScope;

  if (!currentScope->add(varSymbol)) {
    auto conflictSymbol = currentScope->resolve(identifier);
    errorReporter.reportError(ErrorType::REDECLARATION, line, column,
                              "redeclaration of variable '" + identifier + "' + " + conflictSymbol->name + " at line " +
                                  std::to_string(conflictSymbol->definedAtLine) + " column " +
                                  std::to_string(conflictSymbol->definedAtColumn));
  }

  return varSymbol;
}

void SymbolCollectionListener::enterVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
  bool isConst = ctx->CONST() != nullptr;
  int line = ctx->IDENTIFIER()->getSymbol()->getLine();
  int column = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();

  createAndRegisterVariableSymbol(identifier, ctx->type(), isConst, line, column);
}
void SymbolCollectionListener::exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  // mark as defined if assigned an expression
  if (ctx->expression()) {
    auto varSymbol = currentScope->resolve(ctx->IDENTIFIER()->getText());
    if (varSymbol) {
      varSymbol->isDefined = true;
      varSymbol->definedAtLine = ctx->IDENTIFIER()->getSymbol()->getLine();
      varSymbol->definedAtColumn = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();
    }
  }
}

void SymbolCollectionListener::enterDestructuring_item(cgullParser::Destructuring_itemContext* ctx) {
  std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
  bool isConst = ctx->CONST() != nullptr;
  int line = ctx->IDENTIFIER()->getSymbol()->getLine();
  int column = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();

  createAndRegisterVariableSymbol(identifier, ctx->type(), isConst, line, column);
}
void SymbolCollectionListener::exitDestructuring_item(cgullParser::Destructuring_itemContext* ctx) {
  // mark as defined if assigned an expression
  auto varSymbol = currentScope->resolve(ctx->IDENTIFIER()->getText());
  if (varSymbol) {
    varSymbol->definedAtLine = ctx->IDENTIFIER()->getSymbol()->getLine();
    varSymbol->definedAtColumn = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();
  }
}

void SymbolCollectionListener::enterAccess_block(cgullParser::Access_blockContext* ctx) {
  inPrivateScope = ctx->PRIVATE() != nullptr;
}
void SymbolCollectionListener::exitAccess_block(cgullParser::Access_blockContext* ctx) {
  // we don't have nested access blocks, so this is ok to just assume we're no longer private no matter what
  inPrivateScope = false;
}

void SymbolCollectionListener::enterTop_level_struct_statement(cgullParser::Top_level_struct_statementContext* ctx) {
  inPrivateScope = ctx->PRIVATE() != nullptr;
}
void SymbolCollectionListener::exitTop_level_struct_statement(cgullParser::Top_level_struct_statementContext* ctx) {
  // similarly, public/private keywords can't be used within the access blocks, so we definitely aren't in one
  inPrivateScope = false;
}

/* rules that involve entering a new scope (and potentially a symbol) */

void SymbolCollectionListener::enterProgram(cgullParser::ProgramContext* ctx) {
  auto programScope = std::make_shared<Scope>(currentScope);
  programScope->parent = currentScope;
  currentScope = programScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitProgram(cgullParser::ProgramContext* ctx) { currentScope = currentScope->parent; }

void SymbolCollectionListener::enterStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  auto structScope = std::make_shared<Scope>(currentScope);
  structScope->parent = currentScope;
  currentScope = structScope;
  scopes[ctx] = currentScope;

  std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
  int line = ctx->IDENTIFIER()->getSymbol()->getLine();
  int column = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();
  auto structSymbol = std::make_shared<TypeSymbol>(identifier, line, column, currentScope);
  structSymbol->memberScope = currentScope;
  structSymbol->type = SymbolType::STRUCT;

  auto structType = std::make_shared<UserDefinedType>(structSymbol);
  structSymbol->typeRepresentation = structType;

  if (!currentScope->parent->add(structSymbol)) {
    // for now there's no forward declaration, so this can only be a redefinition (technically its both)
    auto conflictSymbol = currentScope->parent->resolve(identifier);
    errorReporter.reportError(ErrorType::REDEFINITION, line, column,
                              "redefinition of struct '" + identifier + "' + " + conflictSymbol->name + " at line " +
                                  std::to_string(conflictSymbol->definedAtLine) + " column " +
                                  std::to_string(conflictSymbol->definedAtColumn));
  }
}
void SymbolCollectionListener::exitStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterFunction_definition(cgullParser::Function_definitionContext* ctx) {
  auto functionScope = std::make_shared<Scope>(currentScope);
  functionScope->parent = currentScope;
  currentScope = functionScope;
  scopes[ctx] = currentScope;

  std::string identifierName = ctx->IDENTIFIER()->getSymbol()->getText();
  std::string specialToken = ctx->FN_SPECIAL() ? ctx->FN_SPECIAL()->getText() : "";
  std::string identifier = specialToken + identifierName;
  int line = ctx->IDENTIFIER()->getSymbol()->getLine();
  int column = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();
  auto functionSymbol = std::make_shared<FunctionSymbol>(identifier, line, column, currentScope);
  functionSymbol->type = SymbolType::FUNCTION;
  functionSymbol->isPrivate = inPrivateScope;
  functionSymbol->isDefined = true; // for recursion
  auto [isStructMethod, structSymbol] = isStructScope(currentScope->parent);

  if (ctx->parameter_list()) {
    for (auto paramCtx : ctx->parameter_list()->parameter()) {
      std::string paramName = paramCtx->IDENTIFIER()->getSymbol()->getText();
      auto paramType = resolveType(paramCtx->type());
      if (paramType) {
        auto paramSymbol = createAndRegisterVariableSymbol(paramName, paramCtx->type(), false, line, column);
        paramSymbol->isDefined = true;
        paramSymbol->definedAtLine = line;
        paramSymbol->definedAtColumn = column;
        functionSymbol->parameters.push_back(paramSymbol);
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, line, column,
                                  "unresolved type " + paramCtx->type()->getText());
      }
    }
  }

  // add "this" as a local variable if it's a struct method, not as a parameter
  if (isStructMethod && structSymbol) {
    // create a pointer type to the struct
    auto structType = structSymbol->typeRepresentation;
    auto structPointerType = std::make_shared<PointerType>(structType);

    // create the "this" variable (but not as a parameter)
    auto thisVar = std::make_shared<VariableSymbol>("this", line, column, currentScope);
    thisVar->dataType = structPointerType;
    thisVar->definedAtLine = line;
    thisVar->definedAtColumn = column;

    currentScope->add(thisVar);
    functionSymbol->isStructMethod = true; // Mark this as a struct method
  }

  if (ctx->type_list()) {
    // one type that's a tuple of types, in fuuture maybe allow multiple return types
    std::vector<std::shared_ptr<Type>> returnTypes;
    for (auto typeCtx : ctx->type_list()->type()) {
      auto resolvedType = resolveType(typeCtx);
      if (resolvedType) {
        returnTypes.push_back(resolvedType);
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, line, column,
                                  "unresolved type " + typeCtx->getText());
        returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      }
    }
    functionSymbol->returnTypes.push_back(std::make_shared<TupleType>(returnTypes));
  } else if (ctx->type()) {
    auto resolvedType = resolveType(ctx->type());
    if (resolvedType) {
      functionSymbol->returnTypes.push_back(resolvedType);
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, line, column,
                                "unresolved type " + ctx->type()->getText());
      functionSymbol->returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  } else {
    functionSymbol->returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
  }

  // Use addFunction instead of add to properly register for overload resolution
  if (!currentScope->parent->addFunction(functionSymbol)) {
    auto conflictSymbol = currentScope->parent->resolve(functionSymbol->getMangledName());
    errorReporter.reportError(ErrorType::REDEFINITION, line, column,
                              "redefinition of function '" + identifier + "' + " + conflictSymbol->name + " at line " +
                                  std::to_string(conflictSymbol->definedAtLine) + " column " +
                                  std::to_string(conflictSymbol->definedAtColumn));
  }
}
void SymbolCollectionListener::exitFunction_definition(cgullParser::Function_definitionContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterWhile_statement(cgullParser::While_statementContext* ctx) {
  auto whileScope = std::make_shared<Scope>(currentScope);
  whileScope->parent = currentScope;
  currentScope = whileScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitWhile_statement(cgullParser::While_statementContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterUntil_statement(cgullParser::Until_statementContext* ctx) {
  auto untilScope = std::make_shared<Scope>(currentScope);
  untilScope->parent = currentScope;
  currentScope = untilScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitUntil_statement(cgullParser::Until_statementContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterFor_statement(cgullParser::For_statementContext* ctx) {
  auto forScope = std::make_shared<Scope>(currentScope);
  forScope->parent = currentScope;
  currentScope = forScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitFor_statement(cgullParser::For_statementContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterInfinite_loop_statement(cgullParser::Infinite_loop_statementContext* ctx) {
  auto infiniteLoopScope = std::make_shared<Scope>(currentScope);
  infiniteLoopScope->parent = currentScope;
  currentScope = infiniteLoopScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitInfinite_loop_statement(cgullParser::Infinite_loop_statementContext* ctx) {
  currentScope = currentScope->parent;
}

void SymbolCollectionListener::enterBranch_block(cgullParser::Branch_blockContext* ctx) {
  auto branchScope = std::make_shared<Scope>(currentScope);
  branchScope->parent = currentScope;
  currentScope = branchScope;
  scopes[ctx] = currentScope;
}
void SymbolCollectionListener::exitBranch_block(cgullParser::Branch_blockContext* ctx) {
  currentScope = currentScope->parent;
}

// /* recursively checking that all identifiers used in an expression are defined */

void SymbolCollectionListener::enterIndexable(cgullParser::IndexableContext* ctx) {
  // check that any identifiers inside the indexable are defined
  for (auto child : ctx->children) {
    if (auto varCtx = dynamic_cast<cgullParser::VariableContext*>(child)) {
      std::string identifier = varCtx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      if (!varSymbol) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, varCtx->getStart()->getLine(),
                                  varCtx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    }
  }
}

void SymbolCollectionListener::enterDereferenceable(cgullParser::DereferenceableContext* ctx) {
  // check that any identifiers inside the dereferenceable are defined
  for (auto child : ctx->children) {
    if (auto varCtx = dynamic_cast<cgullParser::VariableContext*>(child)) {
      std::string identifier = varCtx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      if (!varSymbol) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, varCtx->getStart()->getLine(),
                                  varCtx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    }
  }
}

void SymbolCollectionListener::enterFunction_call(cgullParser::Function_callContext* ctx) {
  // check that any identifiers inside the function call are defined
  for (auto child : ctx->children) {
    if (auto varCtx = dynamic_cast<cgullParser::VariableContext*>(child)) {
      std::string identifier = varCtx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      if (!varSymbol) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, varCtx->getStart()->getLine(),
                                  varCtx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    }
  }
}

void SymbolCollectionListener::enterCast_expression(cgullParser::Cast_expressionContext* ctx) {
  // check that any identifiers inside the cast expression are defined
  for (auto child : ctx->children) {
    if (auto varCtx = dynamic_cast<cgullParser::VariableContext*>(child)) {
      std::string identifier = varCtx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      if (!varSymbol) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, varCtx->getStart()->getLine(),
                                  varCtx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    }
  }
}

void SymbolCollectionListener::enterPostfix_expression(cgullParser::Postfix_expressionContext* ctx) {
  // check that any identifiers inside the postfix expression are defined
  for (auto child : ctx->children) {
    if (auto varCtx = dynamic_cast<cgullParser::VariableContext*>(child)) {
      std::string identifier = varCtx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      if (!varSymbol) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, varCtx->getStart()->getLine(),
                                  varCtx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    }
  }
}

void SymbolCollectionListener::enterVariable(cgullParser::VariableContext* ctx) {
  // check that any identifiers inside the variable are resolved
  if (ctx->IDENTIFIER()) {
    std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(identifier);
    if (!varSymbol) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
    }
  }
}

/* helpers */

std::shared_ptr<Type> SymbolCollectionListener::resolveType(cgullParser::TypeContext* typeCtx) {
  std::shared_ptr<Type> baseType = nullptr;
  if (typeCtx->primitive_type()) {
    std::string primitiveTypeName = typeCtx->primitive_type()->getText();
    baseType = resolvePrimitiveType(primitiveTypeName);
  } else if (typeCtx->user_defined_type()) {
    std::string typeName = typeCtx->user_defined_type()->getText();
    auto resolvedTypeSymbol = currentScope->resolve(typeName);
    if (auto typeSymbol = std::dynamic_pointer_cast<TypeSymbol>(resolvedTypeSymbol)) {
      baseType = typeSymbol->typeRepresentation;
    }
  } else if (typeCtx->tuple_type()) {
    std::vector<std::shared_ptr<Type>> elementTypes;
    if (typeCtx->tuple_type()->type_list()) {
      for (auto typectx : typeCtx->tuple_type()->type_list()->type()) {
        auto resolvedType = resolveType(typectx);
        if (resolvedType) {
          elementTypes.push_back(resolvedType);
        } else {
          return nullptr;
        }
      }
    }
    baseType = std::make_shared<TupleType>(elementTypes);
  }

  if (!baseType) {
    return nullptr;
  }

  // handle pointers
  for (auto child : typeCtx->children) {
    if (child->getText() == "*") {
      baseType = std::make_shared<PointerType>(baseType);
    }
  }

  if (typeCtx->array_suffix().size() > 0) {
    for (auto suffix : typeCtx->array_suffix()) {
      baseType = std::make_shared<ArrayType>(baseType);
    }
  }
  return baseType;
}

std::shared_ptr<Type> SymbolCollectionListener::resolvePrimitiveType(const std::string& typeName) {
  if (typeName == "int")
    return std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT);
  if (typeName == "float")
    return std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::FLOAT);
  if (typeName == "bool")
    return std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN);
  if (typeName == "string")
    return std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::STRING);
  if (typeName == "void")
    return std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);

  return nullptr;
}

std::pair<bool, std::shared_ptr<TypeSymbol>> SymbolCollectionListener::isStructScope(std::shared_ptr<Scope> scope) {
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
