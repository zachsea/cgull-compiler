#include "special_methods_listener.h"
#include "../symbols/symbol.h"
#include "../symbols/type.h"

SpecialMethodsListener::SpecialMethodsListener(
    ErrorReporter& errorReporter, const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes)
    : errorReporter(errorReporter), scopes(scopes) {}

void SpecialMethodsListener::enterStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  // update scope
  auto scopeIt = scopes.find(ctx);
  if (scopeIt == scopes.end()) {
    return;
  }

  std::shared_ptr<Scope> structScope = scopeIt->second;
  std::string structName = ctx->IDENTIFIER()->getSymbol()->getText();
  int line = ctx->IDENTIFIER()->getSymbol()->getLine();
  int column = ctx->IDENTIFIER()->getSymbol()->getCharPositionInLine();

  validateNoUnsupportedSpecialMethods(structScope, structName, line, column);

  validateToStringMethod(structScope, structName, line, column);
  validateDestructMethod(structScope, structName, line, column);
}

void SpecialMethodsListener::validateToStringMethod(const std::shared_ptr<Scope>& structScope,
                                                    const std::string& structName, int line, int column) {
  auto toStringSymbol = structScope->resolve("$toString_");

  if (!toStringSymbol) {
    // $toString doesn't exist, add it
    addDefaultToStringMethod(structScope, structName);
    return;
  }

  if (toStringSymbol->type != SymbolType::FUNCTION) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$toString in struct " + structName + " must be a method");
    return;
  }

  auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(toStringSymbol);
  if (funcSymbol->parameters.size() != 0) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$toString in struct " + structName + " must take no parameters");
  }

  if (funcSymbol->returnTypes.size() != 1) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$toString in struct " + structName + " must return a single value");
    return;
  }

  auto returnType = funcSymbol->returnTypes[0];
  auto primitiveReturnType = std::dynamic_pointer_cast<PrimitiveType>(returnType);

  if (!primitiveReturnType || primitiveReturnType->getPrimitiveKind() != PrimitiveType::PrimitiveKind::STRING) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$toString in struct " + structName + " must return string");
  }
}

void SpecialMethodsListener::validateDestructMethod(const std::shared_ptr<Scope>& structScope,
                                                    const std::string& structName, int line, int column) {
  auto destructSymbol = structScope->resolve("$destruct_");

  if (!destructSymbol) {
    // $destruct is optional, so it's fine if it doesn't exist
    return;
  }

  if (destructSymbol->type != SymbolType::FUNCTION) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$destruct in struct " + structName + " must be a method");
    return;
  }

  auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(destructSymbol);
  if (funcSymbol->parameters.size() != 0) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$destruct in struct " + structName + " must take no parameters");
  }

  if (funcSymbol->returnTypes.size() != 1) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$destruct in struct " + structName + " must return void");
    return;
  }

  auto returnType = funcSymbol->returnTypes[0];
  auto primitiveReturnType = std::dynamic_pointer_cast<PrimitiveType>(returnType);

  if (!primitiveReturnType || primitiveReturnType->getPrimitiveKind() != PrimitiveType::PrimitiveKind::VOID) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, column,
                              "$destruct in struct " + structName + " must return void");
  }
}

void SpecialMethodsListener::validateNoUnsupportedSpecialMethods(const std::shared_ptr<Scope>& structScope,
                                                                 const std::string& structName, int line, int column) {
  // check all symbols in the struct scope
  for (const auto& [name, symbol] : structScope->symbols) {
    // if it starts with $ and is not one of our supported special methods
    if (name.size() > 0 && name[0] == '$' && name != "$toString_" && name != "$destruct_") {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, line, column,
                                "unsupported special method '" + name + "' in struct " + structName);
      return;
    }
  }
}

void SpecialMethodsListener::addDefaultToStringMethod(const std::shared_ptr<Scope>& structScope,
                                                      const std::string& structName) {
  // Create function symbol for $toString
  auto toStringSymbol = std::make_shared<FunctionSymbol>("$toString", 0, 0, structScope);
  toStringSymbol->type = SymbolType::FUNCTION;
  toStringSymbol->isDefined = true;

  // set return type to string
  auto stringType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::STRING);
  toStringSymbol->returnTypes.push_back(stringType);

  // add to scope
  structScope->add(toStringSymbol);
}
