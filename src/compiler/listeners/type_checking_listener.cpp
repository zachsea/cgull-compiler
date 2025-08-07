#include "type_checking_listener.h"
#include "cgullParser.h"
#include <memory>

TypeCheckingListener::TypeCheckingListener(
    ErrorReporter& errorReporter, const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
    std::shared_ptr<Scope> globalScope)
    : errorReporter(errorReporter), scopes(scopes), globalScope(globalScope), currentScope(globalScope) {}

std::shared_ptr<Type> TypeCheckingListener::getExpressionType(antlr4::ParserRuleContext* ctx) const {
  auto it = expressionTypes.find(ctx);
  if (it != expressionTypes.end()) {
    return it->second;
  }
  return nullptr;
}

std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> TypeCheckingListener::getExpressionTypes() const {
  return expressionTypes;
}

std::unordered_set<antlr4::ParserRuleContext*> TypeCheckingListener::getExpectingStringConversion() const {
  return expectingStringConversion;
}

void TypeCheckingListener::setExpressionType(antlr4::ParserRuleContext* ctx, std::shared_ptr<Type> type) {
  expressionTypes[ctx] = type;
}

void TypeCheckingListener::enterEveryRule(antlr4::ParserRuleContext* ctx) {
  auto scopeIt = scopes.find(ctx);
  if (scopeIt != scopes.end()) {
    currentScope = scopeIt->second;
  }
}

void TypeCheckingListener::enterFunction_definition(cgullParser::Function_definitionContext* ctx) {
  currentFunctionReturnTypes.clear();

  // find the function symbol and get its return types
  if (ctx->IDENTIFIER()) {
    std::string functionName = ctx->IDENTIFIER()->getSymbol()->getText();

    // extract parameter types from the parameter list to find the correct overload
    std::vector<std::shared_ptr<Type>> paramTypes;
    if (ctx->parameter_list()) {
      for (auto paramCtx : ctx->parameter_list()->parameter()) {
        auto paramType = resolveType(paramCtx->type());
        if (paramType) {
          paramTypes.push_back(paramType);
        } else {
          // if we can't resolve a parameter type, use void as placeholder
          paramTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
        }
      }
    }
    if (ctx->FN_SPECIAL()) {
      functionName = ctx->FN_SPECIAL()->getText() + functionName;
    }
    auto funcSymbol =
        std::dynamic_pointer_cast<FunctionSymbol>(currentScope->resolveFunctionCall(functionName, paramTypes));

    if (funcSymbol) {
      currentFunctionReturnTypes = funcSymbol->returnTypes;
    }
  }
}

void TypeCheckingListener::exitFunction_definition(cgullParser::Function_definitionContext* ctx) {
  currentFunctionReturnTypes.clear();
}

void TypeCheckingListener::exitReturn_statement(cgullParser::Return_statementContext* ctx) {
  if (!ctx->expression()) {
    // check if function expects no return values (void)
    if (currentFunctionReturnTypes.empty() ||
        (currentFunctionReturnTypes.size() == 1 &&
         std::dynamic_pointer_cast<PrimitiveType>(currentFunctionReturnTypes[0]) &&
         std::dynamic_pointer_cast<PrimitiveType>(currentFunctionReturnTypes[0])->getPrimitiveKind() ==
             PrimitiveType::PrimitiveKind::VOID)) {
      return;
    } else {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Function expects return value(s) but none provided");
      return;
    }
  }
  auto expression = ctx->expression();

  auto returnType = getExpressionType(expression);
  if (!returnType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, expression->getStart()->getLine(),
                              expression->getStart()->getCharPositionInLine(),
                              "Cannot determine type of return expression");
    return;
  }

  if (currentFunctionReturnTypes.empty()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, expression->getStart()->getLine(),
                              expression->getStart()->getCharPositionInLine(), "Function has no return type specified");
    return;
  }

  if (!areTypesCompatible(returnType, currentFunctionReturnTypes[0], expression, ctx)) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, expression->getStart()->getLine(),
                              expression->getStart()->getCharPositionInLine(),
                              "Return type mismatch: expected " + currentFunctionReturnTypes[0]->toString() +
                                  " but got " + returnType->toString());
  }
}

std::shared_ptr<Type> TypeCheckingListener::resolveType(cgullParser::TypeContext* typeCtx) {
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

  if (typeCtx->array_suffix()) {
    baseType = std::make_shared<PointerType>(baseType);
  }

  for (auto child : typeCtx->children) {
    if (child->getText() == "*") {
      baseType = std::make_shared<PointerType>(baseType);
    }
  }

  return baseType;
}

std::shared_ptr<Type> TypeCheckingListener::resolvePrimitiveType(const std::string& typeName) {
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

bool TypeCheckingListener::hasToStringMethod(const std::shared_ptr<Type>& type) {
  // handle primitive types and non-user-defined types
  if (!type || type->getKind() != Type::TypeKind::USER_DEFINED) {
    return false;
  }

  auto userType = std::dynamic_pointer_cast<UserDefinedType>(type);
  if (!userType || !userType->getTypeSymbol() || !userType->getTypeSymbol()->memberScope) {
    return false;
  }

  auto toStringSymbol = userType->getTypeSymbol()->memberScope->resolve("$toString");
  if (!toStringSymbol || toStringSymbol->type != SymbolType::FUNCTION) {
    return false;
  }

  auto funcSymbol = std::dynamic_pointer_cast<FunctionSymbol>(toStringSymbol);
  if (funcSymbol->returnTypes.size() != 1) {
    return false;
  }
  auto returnType = funcSymbol->returnTypes[0];
  auto primitiveReturnType = std::dynamic_pointer_cast<PrimitiveType>(returnType);
  return primitiveReturnType && primitiveReturnType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING;
}

bool TypeCheckingListener::canConvertToString(const std::shared_ptr<Type>& type) {
  if (!type) {
    return false;
  }
  // pointers can be converted to string, it'll just be the address
  auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
  if (pointerType) {
    return true;
  }

  // any primitive type can be converted to string
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
  if (primitiveType) {
    return true;
  }

  // type with $toString method can be converted to string
  return hasToStringMethod(type);
}

bool TypeCheckingListener::areTypesCompatible(const std::shared_ptr<Type>& sourceType,
                                              const std::shared_ptr<Type>& targetType,
                                              antlr4::ParserRuleContext* sourceCtx,
                                              antlr4::ParserRuleContext* targetCtx) {
  // same types are always compatible
  if (sourceType->equals(targetType)) {
    return true;
  }

  // check if target is string and source has a $toString method
  auto targetPrimitive = std::dynamic_pointer_cast<PrimitiveType>(targetType);
  if (targetPrimitive && targetPrimitive->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING &&
      canConvertToString(sourceType)) {
    expectingStringConversion.insert(static_cast<antlr4::ParserRuleContext*>(targetCtx));
    return true;
  }

  // check pointer types
  auto sourcePointer = std::dynamic_pointer_cast<PointerType>(sourceType);
  auto targetPointer = std::dynamic_pointer_cast<PointerType>(targetType);
  if (sourcePointer && targetPointer) {
    // allow null pointer (void*) assignment to any pointer
    auto sourcePointedType = sourcePointer->getPointedType();
    auto targetPointedType = targetPointer->getPointedType();

    auto sourceVoidType = std::dynamic_pointer_cast<PrimitiveType>(sourcePointedType);
    if (sourceVoidType && sourceVoidType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::VOID) {
      return true;
    }

    // pointers MUST point to the same type otherwise
    sourcePointedType->equals(targetPointedType);
  }

  // numerics can convert to each other
  auto sourcePrimitive = std::dynamic_pointer_cast<PrimitiveType>(sourceType);
  if (sourcePrimitive && targetPrimitive) {
    return sourcePrimitive->isNumeric() && targetPrimitive->isNumeric();
  }

  return false;
}

std::shared_ptr<Type> TypeCheckingListener::getFieldType(const std::shared_ptr<Type>& baseType,
                                                         const std::string& fieldName) {
  if (!baseType) {
    return nullptr;
  }

  // if it's a pointer type, we cannot access its fields directly
  if (auto pointerType = std::dynamic_pointer_cast<PointerType>(baseType)) {
    return nullptr;
  }

  if (auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(baseType)) {
    auto structSymbol = userDefinedType->getTypeSymbol();
    if (structSymbol && structSymbol->memberScope) {
      auto fieldSymbol = structSymbol->memberScope->resolve(fieldName);
      if (auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(fieldSymbol)) {
        return varSymbol->dataType;
      }
    }
  }

  if (auto pointerType = std::dynamic_pointer_cast<PointerType>(baseType)) {
    return getFieldType(pointerType->getPointedType(), fieldName);
  }

  if (auto tupleType = std::dynamic_pointer_cast<TupleType>(baseType)) {
    // check if fieldName is a numeric index
    int index = std::stoi(fieldName);
    if (index >= 0 && index < tupleType->getElementTypes().size()) {
      return tupleType->getElementTypes()[index];
    }
  }

  return nullptr;
}

std::shared_ptr<Type> TypeCheckingListener::getElementType(const std::shared_ptr<Type>& arrayType) {
  if (!arrayType) {
    return nullptr;
  }

  if (auto pointerType = std::dynamic_pointer_cast<PointerType>(arrayType)) {
    return pointerType->getPointedType();
  }
  return nullptr;
}

std::vector<std::shared_ptr<Type>>
TypeCheckingListener::collectArgumentTypes(cgullParser::Expression_listContext* exprList) {
  std::vector<std::shared_ptr<Type>> argumentTypes;
  if (exprList && exprList->expression().size() > 0) {
    for (auto expr : exprList->expression()) {
      auto exprType = getExpressionType(expr);
      if (exprType) {
        argumentTypes.push_back(exprType);
      } else {
        argumentTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      }
    }
  }
  return argumentTypes;
}

void TypeCheckingListener::checkArgumentCompatibility(const std::vector<std::shared_ptr<Type>>& argumentTypes,
                                                      const std::vector<std::shared_ptr<Type>>& parameterTypes,
                                                      cgullParser::Expression_listContext* exprList,
                                                      const std::string& functionName, int line, int pos) {

  if (argumentTypes.size() != parameterTypes.size()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, line, pos,
                              "Function call to '" + functionName + "' with incorrect number of arguments. Expected " +
                                  std::to_string(parameterTypes.size()) + ", got " +
                                  std::to_string(argumentTypes.size()));
    return;
  }

  for (size_t i = 0; i < argumentTypes.size(); i++) {
    auto paramType = parameterTypes[i];
    auto argType = argumentTypes[i];

    if (!areTypesCompatible(argType, paramType, exprList->expression()[i], exprList->expression()[i])) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, exprList->expression()[i]->getStart()->getLine(),
                                exprList->expression()[i]->getStart()->getCharPositionInLine(),
                                "Incompatible argument type for parameter " + std::to_string(i + 1) + " of function '" +
                                    functionName + "'. Expected " + paramType->toString() + ", got " +
                                    argType->toString());
    }
  }
}

void TypeCheckingListener::setFunctionCallReturnType(cgullParser::Function_callContext* ctx,
                                                     const std::vector<std::shared_ptr<Type>>& returnTypes) {

  if (returnTypes.size() == 1) {
    setExpressionType(ctx, returnTypes[0]);
  } else if (returnTypes.size() > 1) {
    setExpressionType(ctx, std::make_shared<TupleType>(returnTypes));
  } else {
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
  }
}

void TypeCheckingListener::exitFunction_call(cgullParser::Function_callContext* ctx) {
  if (!ctx->IDENTIFIER()) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Function call without identifier");
    return;
  }

  std::string functionName = ctx->IDENTIFIER()->getSymbol()->getText();
  auto argumentTypes = collectArgumentTypes(ctx->expression_list());

  // method call in a field access context
  auto fieldAccessCtx = dynamic_cast<cgullParser::Field_accessContext*>(ctx->parent->parent);
  if (fieldAccessCtx) {
    auto fieldAccessIt = fieldAccessContexts.find(fieldAccessCtx);
    if (fieldAccessIt == fieldAccessContexts.end() || fieldAccessIt->second.empty()) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }

    auto baseType = fieldAccessIt->second.top();
    if (!baseType) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Base type is null for method call");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }

    if (auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(baseType)) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Cannot call method '" + functionName + "' on primitive type " + baseType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }

    auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(baseType);
    if (userDefinedType && userDefinedType->getTypeSymbol() && userDefinedType->getTypeSymbol()->memberScope) {
      auto funcSymbol = userDefinedType->getTypeSymbol()->memberScope->resolveFunctionCall(functionName, argumentTypes);
      if (funcSymbol) {
        std::vector<std::shared_ptr<Type>> paramTypes;
        for (auto& param : funcSymbol->parameters) {
          paramTypes.push_back(param->dataType);
        }

        checkArgumentCompatibility(argumentTypes, paramTypes, ctx->expression_list(), functionName,
                                   ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine());

        setFunctionCallReturnType(ctx, funcSymbol->returnTypes);
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Method '" + functionName + "' not found in type " + baseType->toString());
        setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      }
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Type " + baseType->toString() + " does not support method calls");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
    return;
  }

  auto funcSymbol =
      std::dynamic_pointer_cast<FunctionSymbol>(currentScope->resolveFunctionCall(functionName, argumentTypes));

  if (!funcSymbol) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "No matching function found for call to '" + functionName + "'");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  std::vector<std::shared_ptr<Type>> paramTypes;
  for (auto& param : funcSymbol->parameters) {
    paramTypes.push_back(param->dataType);
  }

  checkArgumentCompatibility(argumentTypes, paramTypes, ctx->expression_list(), functionName,
                             ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine());

  setFunctionCallReturnType(ctx, funcSymbol->returnTypes);
}

void TypeCheckingListener::exitVariable(cgullParser::VariableContext* ctx) {
  if (ctx->IDENTIFIER()) {
    std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(identifier);
    if (auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol)) {
      setExpressionType(ctx, variableSymbol->dataType);
    } else {
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  } else if (ctx->field_access()) {
    auto fieldType = getExpressionType(ctx->field_access());
    setExpressionType(ctx, fieldType);
  }
}

void TypeCheckingListener::exitLiteral(cgullParser::LiteralContext* ctx) {
  std::shared_ptr<Type> literalType;

  // this needs to be changed to allow the hex/bin literals to be of any numeric type
  if (ctx->NUMBER_LITERAL() || ctx->HEX_LITERAL() || ctx->BINARY_LITERAL()) {
    literalType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT);
  } else if (ctx->FLOAT_POSINF_LITERAL() || ctx->FLOAT_NEGINF_LITERAL() || ctx->FLOAT_NAN_LITERAL() ||
             ctx->DECIMAL_LITERAL()) {
    literalType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::FLOAT);
  } else if (ctx->STRING_LITERAL()) {
    literalType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::STRING);
  } else if (ctx->BOOLEAN_TRUE() || ctx->BOOLEAN_FALSE()) {
    literalType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN);
  } else if (ctx->NULLPTR_LITERAL()) {
    auto voidType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);
    literalType = std::make_shared<PointerType>(voidType);
  }

  setExpressionType(ctx, literalType);
}

void TypeCheckingListener::enterField_access(cgullParser::Field_accessContext* ctx) {
  std::vector<bool> isDereference;
  for (auto expr : ctx->access_operator()) {
    if (expr->getText() == "->") {
      isDereference.push_back(true);
    } else {
      isDereference.push_back(false);
    }
  }
  for (int i = 0; i < isDereference.size(); i++) {
    isDereferenceContexts[ctx->field(i)] = isDereference[i];
  }
  fieldAccessContexts[ctx] = std::stack<std::shared_ptr<Type>>();
}

void TypeCheckingListener::exitField_access(cgullParser::Field_accessContext* ctx) {
  // the last element in the stack is the type of the field access
  if (fieldAccessContexts[ctx].empty()) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
    return;
  }
  auto fieldType = fieldAccessContexts[ctx].top();
  setExpressionType(ctx, fieldType);
}

void TypeCheckingListener::exitField(cgullParser::FieldContext* ctx) {
  // get the parent context's access stack
  std::shared_ptr<Type> fieldType;
  auto parentCtx = dynamic_cast<cgullParser::Field_accessContext*>(ctx->parent);

  if (!parentCtx) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Field not part of field access");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  auto fieldAccessIt = fieldAccessContexts.find(parentCtx);
  if (fieldAccessIt == fieldAccessContexts.end()) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Field access context not found");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  auto& parentAccessStack = fieldAccessIt->second;

  // if there are no elements in the stack, we are at the base of the field access and we can just push the result type
  // without caring about a struct scope
  if (parentAccessStack.empty()) {
    antlr4::ParserRuleContext* baseCtx;
    std::shared_ptr<Type> baseType;
    if (ctx->function_call()) {
      baseCtx = ctx->function_call();
      baseType = getExpressionType(ctx->function_call());
    } else if (ctx->IDENTIFIER()) {
      baseCtx = ctx;
      auto fieldName = ctx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(fieldName);
      auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol);
      if (variableSymbol) {
        baseType = variableSymbol->dataType;
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
        return;
      }
    } else if (ctx->index_expression()) {
      baseCtx = ctx->index_expression();
      baseType = getExpressionType(ctx->index_expression());
    } else if (ctx->expression()) {
      baseCtx = ctx->expression();
      baseType = getExpressionType(ctx->expression());
    } else {
      return;
    }
    if (!baseType) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
      return;
    }
    fieldType = baseType;
  } else {
    if (ctx->expression()) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
      return;
    }
    if (ctx->function_call()) {
      // already verified in the function_call exit method
      fieldType = getExpressionType(ctx->function_call());
    } else if (ctx->index_expression()) {
      fieldType = getExpressionType(ctx->index_expression());
    } else if (ctx->IDENTIFIER()) {
      auto fieldName = ctx->IDENTIFIER()->getSymbol()->getText();
      fieldType = getFieldType(parentAccessStack.top(), fieldName);
      if (!fieldType) {
        errorReporter.reportError(
            ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine(),
            "Cannot resolve field '" + fieldName + "' in type " + parentAccessStack.top()->toString());
        return;
      }
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for field access");
      return;
    }
  }
  // check if we're meant to dereference the field access
  auto isDereferenceIt = isDereferenceContexts.find(ctx);
  if (isDereferenceIt != isDereferenceContexts.end()) {
    auto isDereference = isDereferenceIt->second;
    if (isDereference) {
      auto pointerType = std::dynamic_pointer_cast<PointerType>(fieldType);
      if (!pointerType) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Cannot dereference non-pointer type " + fieldType->toString());
        return;
      }
      fieldType = pointerType->getPointedType();
    }
  }
  parentAccessStack.push(fieldType);
  setExpressionType(ctx, fieldType);
}

void TypeCheckingListener::exitIndex_expression(cgullParser::Index_expressionContext* ctx) {
  std::shared_ptr<Type> indexType;
  if (ctx->expression()) {
    indexType = getExpressionType(ctx->expression());
    if (!indexType) {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Cannot resolve type for index expression");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }
    // check if the index is a valid integer type
    if (!std::dynamic_pointer_cast<PrimitiveType>(indexType) ||
        std::dynamic_pointer_cast<PrimitiveType>(indexType)->getPrimitiveKind() != PrimitiveType::PrimitiveKind::INT) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Index type mismatch: expected int but got " + indexType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }
  }

  auto baseType = getExpressionType(ctx->indexable());
  if (!baseType) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot resolve base type for index expression");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  // check if its a tuple type
  auto tupleType = std::dynamic_pointer_cast<TupleType>(baseType);
  if (tupleType) {
    // check if the index is a valid integer type
    if (!std::dynamic_pointer_cast<PrimitiveType>(indexType) ||
        std::dynamic_pointer_cast<PrimitiveType>(indexType)->getPrimitiveKind() != PrimitiveType::PrimitiveKind::INT) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Index type mismatch: expected int but got " + indexType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }
    // check if the index is in bounds
    // TODO: maybe allow other expressions here
    try {
      std::stoi(ctx->expression()->getText());
    } catch (const std::invalid_argument& e) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Index type mismatch: expected int but got " + indexType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }
    int index = std::stoi(ctx->expression()->getText());
    if (index < 0 || index >= tupleType->getElementTypes().size()) {
      errorReporter.reportError(ErrorType::OUT_OF_BOUNDS, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Index out of bounds for tuple type: " + std::to_string(index));
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }
    setExpressionType(ctx, tupleType->getElementTypes()[index]);
    return;
  }

  auto elementType = getElementType(baseType);
  if (!elementType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot index type " + baseType->toString() + " (not an array/pointer type)");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  setExpressionType(ctx, elementType);
}

void TypeCheckingListener::exitIndexable(cgullParser::IndexableContext* ctx) {
  std::shared_ptr<Type> indexableType;

  // check if the indexable is a field access (great grandparent)
  auto fieldAccessCtx = dynamic_cast<cgullParser::Field_accessContext*>(ctx->parent->parent->parent);
  auto parentAccessIt = fieldAccessContexts.find(fieldAccessCtx);
  if (fieldAccessCtx && parentAccessIt != fieldAccessContexts.end() && !parentAccessIt->second.empty()) {
    // if nothing in the stack, treat as a normal indexable
    auto& parentAccessStack = fieldAccessContexts[fieldAccessCtx];

    auto baseType = parentAccessStack.top();
    if (ctx->expression()) {
      auto indexType = getExpressionType(ctx->expression());
      if (!indexType) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(), "1Cannot resolve type for indexable");
        return;
      }
      // check if the index is a valid integer type
      if (!std::dynamic_pointer_cast<PrimitiveType>(indexType) ||
          std::dynamic_pointer_cast<PrimitiveType>(indexType)->getPrimitiveKind() !=
              PrimitiveType::PrimitiveKind::INT) {
        errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Index type mismatch: expected int but got " + indexType->toString());
        return;
      }
    }
    if (ctx->IDENTIFIER()) {
      auto fieldName = ctx->IDENTIFIER()->getSymbol()->getText();
      indexableType = getFieldType(baseType, fieldName);
      if (!indexableType) {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Cannot resolve field '" + fieldName + "' in type " + baseType->toString());
        return;
      }
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "2Cannot resolve type for indexable");
      return;
    }
  } else {
    // otherwise, resolve the type normally
    if (ctx->IDENTIFIER()) {
      std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
      auto varSymbol = currentScope->resolve(identifier);
      auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol);
      if (variableSymbol) {
        indexableType = variableSymbol->dataType;
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(), "3Cannot resolve type for indexable");
        return;
      }
    } else if (ctx->expression()) {
      indexableType = getExpressionType(ctx->expression());
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "4Cannot resolve type for indexable");
      return;
    }
  }
  if (!indexableType) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "5Cannot resolve type for indexable");
    return;
  }
  setExpressionType(ctx, indexableType);
}

void TypeCheckingListener::exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) {
  if (!ctx->expression())
    return;

  std::shared_ptr<Type> targetType = nullptr;

  // track what kind of assignment target we have for error messages
  std::string targetDescription;
  antlr4::ParserRuleContext* targetCtx = nullptr;

  if (ctx->variable()) {
    targetType = getExpressionType(ctx->variable());
    targetDescription = "variable";
    targetCtx = ctx->variable();
  } else if (ctx->index_expression()) {
    targetType = getExpressionType(ctx->index_expression());
    targetDescription = "indexed element";
    targetCtx = ctx->index_expression();
  } else if (ctx->dereference_expression()) {
    targetType = getExpressionType(ctx->dereference_expression());
    targetDescription = "dereferenced pointer";
    targetCtx = ctx->dereference_expression();
  }

  if (!targetType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine type of assignment target");
    return;
  }
  // right side
  auto valueType = getExpressionType(ctx->expression());
  if (!valueType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->expression()->getStart()->getLine(),
                              ctx->expression()->getStart()->getCharPositionInLine(),
                              "Cannot determine type of expression");
    return;
  }
  if (!areTypesCompatible(valueType, targetType, ctx->expression(), ctx)) {
    std::string errorMessage = "Cannot assign value of type " + valueType->toString() + " to " + targetDescription;

    // For index expressions, include the variable name in the error message
    if (ctx->index_expression() && ctx->index_expression()->indexable()) {
      errorMessage += " " + ctx->index_expression()->indexable()->getText() + "[...]";
    }

    errorMessage += " of type " + targetType->toString();

    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->expression()->getStart()->getLine(),
                              ctx->expression()->getStart()->getCharPositionInLine(), errorMessage);
  }

  // can't assign to const
  if (ctx->variable() && ctx->variable()->IDENTIFIER()) {
    auto varName = ctx->variable()->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(varName);
    if (auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol)) {
      if (variableSymbol->isConstant) {
        errorReporter.reportError(ErrorType::ASSIGNMENT_TO_CONST, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Cannot assign to const variable '" + varName + "'");
      }
    }
  }
}

void TypeCheckingListener::exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  if (!ctx->expression())
    return;

  auto declaredType = resolveType(ctx->type());
  if (!declaredType) {
    return;
  }

  auto initType = getExpressionType(ctx->expression());
  if (!initType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->expression()->getStart()->getLine(),
                              ctx->expression()->getStart()->getCharPositionInLine(),
                              "Cannot determine type of initialization expression");
    return;
  }

  if (!areTypesCompatible(initType, declaredType, ctx->expression(), ctx)) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->expression()->getStart()->getLine(),
                              ctx->expression()->getStart()->getCharPositionInLine(),
                              "Cannot initialize variable of type " + declaredType->toString() +
                                  " with value of type " + initType->toString());
  }
}

void TypeCheckingListener::exitCast_expression(cgullParser::Cast_expressionContext* ctx) {
  std::shared_ptr<Type> targetType;

  if (ctx->primitive_type()) {
    std::string typeName = ctx->primitive_type()->getText();
    targetType = resolvePrimitiveType(typeName);
  } else if (ctx->IDENTIFIER()) {
    std::string typeName = ctx->IDENTIFIER()->getSymbol()->getText();
    auto resolvedTypeSymbol = currentScope->resolve(typeName);
    if (auto typeSymbol = std::dynamic_pointer_cast<TypeSymbol>(resolvedTypeSymbol)) {
      targetType = typeSymbol->typeRepresentation;
    }
  }

  if (!targetType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Invalid target type for cast");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  std::shared_ptr<Type> sourceType;
  if (ctx->expression()) {
    sourceType = getExpressionType(ctx->expression());
  } else if (ctx->IDENTIFIER()) {
    std::string varName = ctx->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(varName);
    if (auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol)) {
      sourceType = variableSymbol->dataType;
    }
  }

  if (!sourceType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine source type for cast");
    setExpressionType(ctx, targetType);
    return;
  }

  if (ctx->BITS_AS_CAST()) {
    auto sourcePrim = std::dynamic_pointer_cast<PrimitiveType>(sourceType);
    auto targetPrim = std::dynamic_pointer_cast<PrimitiveType>(targetType);

    if (!sourcePrim || !targetPrim) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "bits_as_cast can only be used between primitive types");
    }
  }

  setExpressionType(ctx, targetType);
}

void TypeCheckingListener::exitDereference_expression(cgullParser::Dereference_expressionContext* ctx) {
  if (!ctx->dereferenceable()) {
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  // when dereferencing, we need to properly get the base identifier type
  std::shared_ptr<Type> baseType = nullptr;

  if (ctx->dereferenceable()->IDENTIFIER()) {
    std::string varName = ctx->dereferenceable()->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(varName);
    if (auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol)) {
      baseType = variableSymbol->dataType;
    }
  } else {
    baseType = getExpressionType(ctx->dereferenceable());
  }

  if (!baseType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine base type for dereference");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  auto pointerType = std::dynamic_pointer_cast<PointerType>(baseType);
  if (!pointerType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot dereference non-pointer type " + baseType->toString());
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  setExpressionType(ctx, pointerType->getPointedType());
}

void TypeCheckingListener::exitReference_expression(cgullParser::Reference_expressionContext* ctx) {
  if (!ctx->expression()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine base type for reference");
  }
  auto baseType = getExpressionType(ctx->expression());
  if (!baseType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine base type for reference");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }
  auto pointerType = std::dynamic_pointer_cast<PointerType>(baseType);
  if (pointerType) {
    setExpressionType(ctx, pointerType);
  } else {
    setExpressionType(ctx, std::make_shared<PointerType>(baseType));
  }
}

void TypeCheckingListener::exitTuple_expression(cgullParser::Tuple_expressionContext* ctx) {
  if (!ctx->expression_list()) {
    setExpressionType(ctx, std::make_shared<TupleType>(std::vector<std::shared_ptr<Type>>{}));
    return;
  }

  std::vector<std::shared_ptr<Type>> elementTypes;
  for (auto expr : ctx->expression_list()->expression()) {
    auto elemType = getExpressionType(expr);
    if (elemType) {
      elementTypes.push_back(elemType);
    } else {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, expr->getStart()->getLine(),
                                expr->getStart()->getCharPositionInLine(), "Cannot determine type for tuple element");
      elementTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  }

  setExpressionType(ctx, std::make_shared<TupleType>(elementTypes));
}

void TypeCheckingListener::exitBase_expression(cgullParser::Base_expressionContext* ctx) {
  if (ctx->expression()) {
    setExpressionType(ctx, getExpressionType(ctx->expression()));
  } else if (ctx->literal()) {
    setExpressionType(ctx, getExpressionType(ctx->literal()));
  } else if (ctx->function_call()) {
    setExpressionType(ctx, getExpressionType(ctx->function_call()));
  } else if (ctx->variable()) {
    setExpressionType(ctx, getExpressionType(ctx->variable()));
  } else if (ctx->index_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->index_expression()));
  } else if (ctx->dereference_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->dereference_expression()));
  } else if (ctx->reference_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->reference_expression()));
  } else if (ctx->cast_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->cast_expression()));
  } else if (ctx->tuple_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->tuple_expression()));
  } else if (ctx->unary_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->unary_expression()));
  } else if (ctx->allocate_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->allocate_expression()));
  } else if (ctx->children.size() >= 3) {
    auto left = getExpressionType(ctx->base_expression(0));
    auto right = getExpressionType(ctx->base_expression(1));

    if (!left || !right) {
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
      return;
    }

    std::string op = ctx->children[1]->getText();

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "&" || op == "|" || op == "^" ||
        op == "<<" || op == ">>") {

      // handle string concatenation with the + operator
      if (op == "+") {
        // check if either operand is a string
        bool leftIsString = false;
        bool rightIsString = false;

        auto leftPrimitive = std::dynamic_pointer_cast<PrimitiveType>(left);
        if (leftPrimitive && leftPrimitive->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
          leftIsString = true;
        }

        auto rightPrimitive = std::dynamic_pointer_cast<PrimitiveType>(right);
        if (rightPrimitive && rightPrimitive->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
          rightIsString = true;
        }

        if (leftIsString || rightIsString) {
          // if either operand is a string, check if the other can be converted
          if (leftIsString && !rightIsString) {
            rightIsString = canConvertToString(right);
          } else if (rightIsString && !leftIsString) {
            leftIsString = canConvertToString(left);
          }

          // only if at least one is a string and the other can be converted
          if (leftIsString && rightIsString) {
            setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::STRING));
            return;
          }
        }
      }

      // handle numeric operations
      bool leftIsNumeric = std::dynamic_pointer_cast<PrimitiveType>(left) != nullptr &&
                           std::dynamic_pointer_cast<PrimitiveType>(left)->isNumeric();
      bool rightIsNumeric = std::dynamic_pointer_cast<PrimitiveType>(right) != nullptr &&
                            std::dynamic_pointer_cast<PrimitiveType>(right)->isNumeric();

      if (!leftIsNumeric || !rightIsNumeric) {
        errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Operator '" + op + "' requires numeric operands");
        setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT));
        return;
      }

      setExpressionType(ctx, left);
    } else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN));
    } else if (op == "&&" || op == "||") {
      auto leftBool = std::dynamic_pointer_cast<PrimitiveType>(left);
      auto leftPointer = std::dynamic_pointer_cast<PointerType>(left);
      auto rightBool = std::dynamic_pointer_cast<PrimitiveType>(right);
      auto rightPointer = std::dynamic_pointer_cast<PointerType>(right);

      // allow pointers in logical operations (treating them as boolean)
      bool leftIsValid =
          (leftBool && leftBool->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) || leftPointer;
      bool rightIsValid =
          (rightBool && rightBool->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) || rightPointer;

      if (!leftIsValid || !rightIsValid) {
        errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Logical operator '" + op + "' requires boolean operands or pointers");
      }

      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN));
    }
  } else if (ctx->if_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->if_expression()));
  } else {
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
  }
}

void TypeCheckingListener::exitExpression(cgullParser::ExpressionContext* ctx) {
  if (ctx->base_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->base_expression()));
  } else {
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
  }
}

void TypeCheckingListener::exitAllocate_expression(cgullParser::Allocate_expressionContext* ctx) {
  if (ctx->allocate_primitive()) {
    setExpressionType(ctx, getExpressionType(ctx->allocate_primitive()));
  } else if (ctx->allocate_array()) {
    setExpressionType(ctx, getExpressionType(ctx->allocate_array()));
  } else if (ctx->allocate_struct()) {
    setExpressionType(ctx, getExpressionType(ctx->allocate_struct()));
  }
}

void TypeCheckingListener::exitAllocate_primitive(cgullParser::Allocate_primitiveContext* ctx) {
  if (ctx->primitive_type()) {
    std::string typeName = ctx->primitive_type()->getText();
    auto baseType = resolvePrimitiveType(typeName);
    if (baseType) {
      setExpressionType(ctx, std::make_shared<PointerType>(baseType));
    } else {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Invalid primitive type in allocation");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  }
}

void TypeCheckingListener::exitAllocate_array(cgullParser::Allocate_arrayContext* ctx) {
  if (ctx->type()) {
    auto baseType = resolveType(ctx->type());
    if (baseType) {
      setExpressionType(ctx, std::make_shared<PointerType>(baseType));
    } else {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Invalid type in array allocation");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  }
}

void TypeCheckingListener::exitAllocate_struct(cgullParser::Allocate_structContext* ctx) {
  if (ctx->IDENTIFIER()) {
    std::string structName = ctx->IDENTIFIER()->getSymbol()->getText();
    auto structSymbol = currentScope->resolve(structName);
    if (auto typeSymbol = std::dynamic_pointer_cast<TypeSymbol>(structSymbol)) {
      // check if parameters match the struct definition (check if it resolves to the constructor)
      std::vector<std::shared_ptr<Type>> parameters;
      if (ctx->expression_list()) {
        for (auto expr : ctx->expression_list()->expression()) {
          auto paramType = getExpressionType(expr);
          if (!paramType) {
            errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                      ctx->getStart()->getCharPositionInLine(),
                                      "Cannot determine type of parameter in struct allocation");
            setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
            return;
          }
          parameters.push_back(paramType);
        }
      }
      auto constructor = currentScope->resolveFunctionCall(structName, parameters);
      // check that the parameters match the constructor
      if (!constructor) {
        errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(),
                                  "Cannot find constructor for struct '" + structName + "' with given parameters");
        setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
        return;
      }
      for (size_t i = 0; i < parameters.size(); i++) {
        auto paramType = parameters[i];
        auto expectedType = constructor->parameters[i]->dataType;
        if (!areTypesCompatible(paramType, expectedType, ctx->expression_list()->expression()[i], ctx)) {
          errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                    ctx->getStart()->getCharPositionInLine(),
                                    "Cannot pass parameter of type " + paramType->toString() +
                                        " to constructor of type " + expectedType->toString());
          setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
          return;
        }
      }
      setExpressionType(ctx, std::make_shared<PointerType>(typeSymbol->typeRepresentation));
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "Invalid struct type in allocation");
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    }
  }
}

void TypeCheckingListener::exitUnary_expression(cgullParser::Unary_expressionContext* ctx) {
  if (ctx->postfix_expression()) {
    setExpressionType(ctx, getExpressionType(ctx->postfix_expression()));
    return;
  }

  // handle +, -, !, ++, --, and ~ operator cases
  if (!ctx->expression()) {
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  auto operandType = getExpressionType(ctx->expression());
  if (!operandType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot determine type of operand in unary expression");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  // handle unary +, -
  if (ctx->PLUS_OP() || ctx->MINUS_OP()) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(operandType);
    if (!primitiveType || !primitiveType->isNumeric() ||
        primitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Unary operator " + std::string(1, ctx->getText()[0]) +
                                    " requires numeric non-boolean operand, got " + operandType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT));
    } else {
      setExpressionType(ctx, operandType);
    }
  }
  // handle logical NOT (!)
  else if (ctx->NOT_OP()) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(operandType);
    auto pointerType = std::dynamic_pointer_cast<PointerType>(operandType);

    // allow logical NOT on pointers (for null checks)
    if (pointerType) {
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN));
      return;
    }

    if (!primitiveType || primitiveType->getPrimitiveKind() != PrimitiveType::PrimitiveKind::BOOLEAN) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Logical NOT operator requires boolean operand, got " + operandType->toString());
    }
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN));
  }
  // handle bitwise NOT (~)
  else if (ctx->BITWISE_NOT_OP()) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(operandType);
    if (!primitiveType || !primitiveType->isInteger()) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Bitwise NOT operator requires integer operand, got " + operandType->toString());
      setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT));
    } else {
      setExpressionType(ctx, operandType);
    }
  }
  // handle increment/decrement operators (++, --)
  else if (ctx->INCREMENT_OP() || ctx->DECREMENT_OP()) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(operandType);
    if (!primitiveType || !primitiveType->isNumeric() ||
        primitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) {
      errorReporter.reportError(
          ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine(),
          "Increment/decrement operator requires numeric operand, got " + operandType->toString());
    }
    setExpressionType(ctx, operandType);
  }
}

void TypeCheckingListener::exitPostfix_expression(cgullParser::Postfix_expressionContext* ctx) {
  std::shared_ptr<Type> baseType = nullptr;

  // determine the base type of the expression
  if (ctx->IDENTIFIER()) {
    std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(identifier);
    if (auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol)) {
      baseType = variableSymbol->dataType;
    }
  } else if (ctx->function_call()) {
    baseType = getExpressionType(ctx->function_call());
  } else if (ctx->field_access()) {
    baseType = getExpressionType(ctx->field_access());
  } else if (ctx->expression()) {
    baseType = getExpressionType(ctx->expression());
  }

  if (!baseType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot determine base type for postfix operation");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  // check if the type is numeric for increment/decrement operations
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(baseType);
  if (!primitiveType || !primitiveType->isNumeric()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Postfix increment/decrement requires numeric type, got " + baseType->toString());
  }

  setExpressionType(ctx, baseType);
}

void TypeCheckingListener::exitIf_expression(cgullParser::If_expressionContext* ctx) {
  auto conditionType = getExpressionType(ctx->base_expression(0));
  if (!conditionType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->base_expression(0)->getStart()->getLine(),
                              ctx->base_expression(0)->getStart()->getCharPositionInLine(),
                              "Cannot determine type of condition in if expression");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  // verify condition is boolean or a pointer (which can be used in boolean contexts)
  auto primCondition = std::dynamic_pointer_cast<PrimitiveType>(conditionType);
  auto pointerType = std::dynamic_pointer_cast<PointerType>(conditionType);

  // allow pointers in if expressions (non-null is true, null is false)
  if (!pointerType && (!primCondition || primCondition->getPrimitiveKind() != PrimitiveType::PrimitiveKind::BOOLEAN)) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->base_expression(0)->getStart()->getLine(),
                              ctx->base_expression(0)->getStart()->getCharPositionInLine(),
                              "If expression condition must be a boolean or pointer, got " + conditionType->toString());
  }

  auto trueType = getExpressionType(ctx->base_expression(1));
  auto falseType = getExpressionType(ctx->base_expression(2));

  if (!trueType || !falseType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Cannot determine types in branches of if expression");
    setExpressionType(ctx, std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
    return;
  }

  if (trueType->equals(falseType)) {
    // if same type, use that type
    setExpressionType(ctx, trueType);
  } else if (areTypesCompatible(trueType, falseType, ctx->base_expression(1), ctx->base_expression(2))) {
    // if true type can be converted to false type
    setExpressionType(ctx, falseType);
  } else if (areTypesCompatible(falseType, trueType, ctx->base_expression(2), ctx->base_expression(1))) {
    // if false type can be converted to true type
    setExpressionType(ctx, trueType);
  } else {
    // incompatible types
    errorReporter.reportError(
        ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(), ctx->getStart()->getCharPositionInLine(),
        "Branches of if expression have incompatible types: " + trueType->toString() + " and " + falseType->toString());
    setExpressionType(ctx, trueType);
  }
}

void TypeCheckingListener::exitDestructuring_item(cgullParser::Destructuring_itemContext* ctx) {
  if (ctx->IDENTIFIER()) {
    std::string identifier = ctx->IDENTIFIER()->getSymbol()->getText();
    auto varSymbol = currentScope->resolve(identifier);
    if (varSymbol) {
      auto variableSymbol = std::dynamic_pointer_cast<VariableSymbol>(varSymbol);
      if (variableSymbol) {
        setExpressionType(ctx, variableSymbol->dataType);
      } else {
        errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                  ctx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
      }
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "unresolved variable " + identifier);
    }
  } else {
    auto type = getExpressionType(ctx->variable());
    if (type) {
      setExpressionType(ctx, type);
    } else {
      errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(), "unresolved variable");
    }
  }
}

void TypeCheckingListener::exitDestructuring_statement(cgullParser::Destructuring_statementContext* ctx) {
  if (!ctx->expression()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine type of destructuring");
    return;
  }

  auto rightSideType = getExpressionType(ctx->expression());
  if (!rightSideType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "Cannot determine type of destructuring");
    return;
  }
  auto tupleType = std::dynamic_pointer_cast<TupleType>(rightSideType);
  if (!tupleType) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Destructuring assignment requires a tuple type, got " + rightSideType->toString());
    return;
  }

  // check if destructuring items in destructuring list are compatible with the tuple type
  if (ctx->destructuring_list()->destructuring_item().size() != tupleType->elementTypes.size()) {
    errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "Destructuring assignment has incompatible number of elements");
    return;
  }
  for (int i = 0; i < ctx->destructuring_list()->destructuring_item().size(); i++) {
    auto itemType = getExpressionType(ctx->destructuring_list()->destructuring_item(i));
    if (!itemType) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Cannot determine type of destructuring item " + std::to_string(i));
      continue;
    }
    if (!areTypesCompatible(itemType, tupleType->elementTypes[i], ctx->destructuring_list()->destructuring_item(i),
                            ctx)) {
      errorReporter.reportError(ErrorType::TYPE_MISMATCH, ctx->getStart()->getLine(),
                                ctx->getStart()->getCharPositionInLine(),
                                "Destructuring item " + std::to_string(i) + " has incompatible type");
    }
  }
}
