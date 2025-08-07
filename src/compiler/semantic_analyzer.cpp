#include "semantic_analyzer.h"
#include "listeners/default_constructor_listener.h"
#include "listeners/special_methods_listener.h"
#include "listeners/symbol_collection_listener.h"
#include "listeners/type_checking_listener.h"
#include "listeners/use_before_definition_listener.h"
#include <antlr4-runtime.h>
#include <iostream>

SemanticAnalyzer::SemanticAnalyzer() {
  globalScope = std::make_shared<Scope>(nullptr);
  scopeMap[nullptr] = globalScope;
  addBuiltinFunctions();
}

void SemanticAnalyzer::analyze(cgullParser::ProgramContext* programCtx) {
  // FIRST PASS: collect symbols, handles declarations errors
  SymbolCollectionListener symbolCollector(errorReporter, globalScope);
  antlr4::tree::ParseTreeWalker walker;
  walker.walk(&symbolCollector, programCtx);
  scopeMap = symbolCollector.getScopeMapping();

  // SECOND PASS: create default constructors for structs
  DefaultConstructorListener defaultConstructorListener(errorReporter, scopeMap);
  walker.walk(&defaultConstructorListener, programCtx);

  // THIRD PASS: ensure special methods are valid
  SpecialMethodsListener specialMethodsListener(errorReporter, scopeMap);
  walker.walk(&specialMethodsListener, programCtx);

  // FOURTH PASS: validate types and expressions
  TypeCheckingListener typeChecker(errorReporter, scopeMap, globalScope);
  walker.walk(&typeChecker, programCtx);

  // FIFTH PASS: check for use before definition errors
  UseBeforeDefinitionListener useBeforeDefListener(errorReporter, scopeMap);
  walker.walk(&useBeforeDefListener, programCtx);
}

void SemanticAnalyzer::addBuiltinFunctions() {
  auto addBuiltinFunction = [this](const std::string& name,
                                   const std::vector<std::pair<std::string, std::shared_ptr<Type>>>& params,
                                   const std::vector<std::shared_ptr<Type>>& returnTypes) {
    auto funcSymbol = std::make_shared<FunctionSymbol>(name, 0, 0, globalScope);
    funcSymbol->isDefined = true;
    funcSymbol->isBuiltin = true;

    for (const auto& [paramName, paramType] : params) {
      auto paramSymbol = std::make_shared<VariableSymbol>(paramName, 0, 0, globalScope);
      paramSymbol->type = SymbolType::PARAMETER;
      paramSymbol->dataType = paramType;
      paramSymbol->isDefined = true;
      funcSymbol->parameters.push_back(paramSymbol);
    }
    funcSymbol->returnTypes = returnTypes;
    globalScope->add(funcSymbol);
  };

  auto intType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT);
  auto shortType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::SHORT);
  auto longType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::LONG);
  auto floatType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::FLOAT);
  auto charType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::CHAR);
  auto boolType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN);
  auto stringType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::STRING);
  auto voidType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);
  auto unsignedIntType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::UNSIGNED_INT);
  auto unsignedShortType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::UNSIGNED_SHORT);
  auto unsignedLongType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::UNSIGNED_LONG);
  auto unsignedCharType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::UNSIGNED_CHAR);
  auto signedIntType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::SIGNED_INT);
  auto signedShortType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::SIGNED_SHORT);
  auto signedLongType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::SIGNED_LONG);
  auto signedCharType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::SIGNED_CHAR);

  addBuiltinFunction("println", {{"value", stringType}}, {voidType});
  addBuiltinFunction("print", {{"value", stringType}}, {voidType});
  addBuiltinFunction("print", {{"value", stringType}, {"end", charType}}, {voidType});
  addBuiltinFunction("readline", {}, {stringType});
  addBuiltinFunction("read", {}, {charType});
  addBuiltinFunction("read", {{"delimiter", charType}}, {stringType});
  addBuiltinFunction("read", {{"delimiter", charType}, {"maxChars", intType}}, {stringType});

  // math functions, eventually will be moved to a math library
  addBuiltinFunction("sqrt", {{"value", floatType}}, {floatType});
}

void SemanticAnalyzer::printSymbolsAsJson(std::ostream& out) const {
  out << "{\n";
  printScopeAsJson(globalScope, out, 2);
  out << "\n}\n";
}

void SemanticAnalyzer::printScopeAsJson(std::shared_ptr<Scope> scope, std::ostream& out, int indentLevel) const {
  std::string indent(indentLevel, ' ');
  std::string childIndent(indentLevel + 2, ' ');
  std::string symbolIndent(indentLevel + 4, ' ');

  // basic scope information
  std::string scopeName = getScopeName(scope);
  out << indent << "\"scopeName\": \"" << scopeName << "\",\n";
  out << indent << "\"scopeId\": \"" << scope.get() << "\"";

  // add parent reference if exists
  if (scope->parent) {
    out << ",\n" << indent << "\"parentId\": \"" << scope->parent.get() << "\"";
  }

  // write scope symbols
  out << ",\n" << indent << "\"symbols\": {";

  bool firstSymbol = true;
  for (const auto& [name, symbol] : scope->symbols) {
    if (!firstSymbol) {
      out << ",";
    }
    out << "\n";
    firstSymbol = false;

    out << childIndent << "\"" << name << "\": {\n";

    // common attributes for all symbols
    out << symbolIndent << "\"name\": \"" << symbol->name << "\",\n";
    out << symbolIndent << "\"type\": \"" << symbolTypeToString(symbol->type) << "\",\n";
    out << symbolIndent << "\"defined\": " << (symbol->isDefined ? "true" : "false") << ",\n";
    out << symbolIndent << "\"private\": " << (symbol->isPrivate ? "true" : "false") << ",\n";
    out << symbolIndent << "\"line\": " << symbol->definedAtLine << ",\n";
    out << symbolIndent << "\"column\": " << symbol->definedAtColumn;

    // type-specific attributes
    if (symbol->type == SymbolType::VARIABLE) {
      auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(symbol);
      out << ",\n" << symbolIndent << "\"isConst\": " << (varSymbol->isConstant ? "true" : "false") << ",\n";
      out << symbolIndent << "\"dataType\": \"" << (varSymbol->dataType ? varSymbol->dataType->toString() : "unknown")
          << "\"";
    } else if (symbol->type == SymbolType::FUNCTION) {
      auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(symbol);

      // return types
      out << ",\n" << symbolIndent << "\"returnTypes\": [";
      for (size_t i = 0; i < funcSymbol->returnTypes.size(); ++i) {
        if (i > 0)
          out << ", ";
        out << "\"" << (funcSymbol->returnTypes[i] ? funcSymbol->returnTypes[i]->toString() : "unknown") << "\"";
      }
      out << "],\n";

      // parameters
      out << symbolIndent << "\"parameters\": [";
      for (size_t i = 0; i < funcSymbol->parameters.size(); ++i) {
        if (i > 0)
          out << ", ";
        out << "\"" << funcSymbol->parameters[i]->name << " ("
            << (funcSymbol->parameters[i]->dataType ? funcSymbol->parameters[i]->dataType->toString() : "unknown")
            << ")\"";
      }
      out << "]";
    } else if (symbol->type == SymbolType::STRUCT) {
      auto typeSymbol = std::dynamic_pointer_cast<TypeSymbol>(symbol);
      out << ",\n" << symbolIndent << "\"memberScopeId\": \"" << typeSymbol->memberScope.get() << "\"";
    } else if (symbol->type == SymbolType::PARAMETER) {
      auto paramSymbol = std::dynamic_pointer_cast<VariableSymbol>(symbol);
      out << ",\n" << symbolIndent << "\"isConst\": " << (paramSymbol->isConstant ? "true" : "false") << ",\n";
      out << symbolIndent << "\"dataType\": \""
          << (paramSymbol->dataType ? paramSymbol->dataType->toString() : "unknown") << "\"";
    } else if (symbol->type == SymbolType::TYPE) {
      auto typeSymbol = std::dynamic_pointer_cast<TypeSymbol>(symbol);
      out << ",\n"
          << symbolIndent << "\"typeRepresentation\": \""
          << (typeSymbol->typeRepresentation ? typeSymbol->typeRepresentation->toString() : "unknown") << "\"";
    }

    out << "\n" << childIndent << "}";
  }

  out << (scope->symbols.empty() ? "}" : "\n" + indent + "}");

  // child scopes
  std::vector<std::shared_ptr<Scope>> childScopes = findChildScopes(scope);
  if (!childScopes.empty()) {
    out << ",\n" << indent << "\"childScopes\": [";

    for (size_t i = 0; i < childScopes.size(); ++i) {
      if (i > 0)
        out << ",";
      out << "\n" << childIndent << "{\n";
      printScopeAsJson(childScopes[i], out, indentLevel + 4);
      out << "\n" << childIndent << "}";
    }

    out << "\n" << indent << "]";
  }
}

std::vector<std::shared_ptr<Scope>> SemanticAnalyzer::findChildScopes(std::shared_ptr<Scope> parent) const {
  std::vector<std::shared_ptr<Scope>> children;
  for (const auto& [ctx, scope] : scopeMap) {
    if (scope->parent == parent && scope != parent) {
      children.push_back(scope);
    }
  }
  return children;
}

std::string SemanticAnalyzer::symbolTypeToString(SymbolType type) const {
  switch (type) {
  case SymbolType::VARIABLE:
    return "VARIABLE";
  case SymbolType::FUNCTION:
    return "FUNCTION";
  case SymbolType::STRUCT:
    return "STRUCT";
  case SymbolType::PARAMETER:
    return "PARAMETER";
  case SymbolType::TYPE:
    return "TYPE";
  default:
    return "UNKNOWN";
  }
}

std::string SemanticAnalyzer::getScopeName(std::shared_ptr<Scope> scope) const {
  if (!scope->parent) {
    return "Global Scope";
  }

  antlr4::ParserRuleContext* ctx = nullptr;
  for (const auto& [context, mappedScope] : scopeMap) {
    if (mappedScope == scope) {
      ctx = context;
      break;
    }
  }

  if (!ctx) {
    return "Unknown Scope";
  }
  // create a friendly-ish name for the scope
  if (dynamic_cast<cgullParser::Function_definitionContext*>(ctx)) {
    auto funcCtx = dynamic_cast<cgullParser::Function_definitionContext*>(ctx);
    std::string specialFunc = funcCtx->FN_SPECIAL() ? funcCtx->FN_SPECIAL()->getText() : "";
    if (funcCtx->IDENTIFIER()) {
      return "Function " + specialFunc + funcCtx->IDENTIFIER()->getText();
    }
    return "Funcion " + specialFunc;
  } else if (dynamic_cast<cgullParser::Struct_definitionContext*>(ctx)) {
    auto structCtx = dynamic_cast<cgullParser::Struct_definitionContext*>(ctx);
    if (structCtx->IDENTIFIER()) {
      return "Struct " + structCtx->IDENTIFIER()->getText();
    }
    return "Anonymous Struct";
  } else if (dynamic_cast<cgullParser::If_statementContext*>(ctx)) {
    return "If Block (Line " + std::to_string(ctx->getStart()->getLine()) + ")";
  } else if (dynamic_cast<cgullParser::Loop_statementContext*>(ctx)) {
    return "Loop Block (Line " + std::to_string(ctx->getStart()->getLine()) + ")";
  } else if (dynamic_cast<cgullParser::ProgramContext*>(ctx)) {
    return "Program";
  }

  return "Block at Line " + std::to_string(ctx->getStart()->getLine());
}
