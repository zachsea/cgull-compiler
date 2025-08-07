#include "bytecode_ir_generator_listener.h"
#include "../bytecode_compiler.h"
#include "../primitive_wrapper_generator.h"
#include "type_checking_listener.h"

BytecodeIRGeneratorListener::BytecodeIRGeneratorListener(
    ErrorReporter& errorReporter, std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<FunctionSymbol>>& resolvedMethodSymbols,
    std::unordered_set<antlr4::ParserRuleContext*>& expectingStringConversion,
    std::unordered_map<PrimitiveType::PrimitiveKind, std::shared_ptr<IRClass>>& primitiveWrappers,
    std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>>& constructorMap)
    : errorReporter(errorReporter), scopes(scopes), expressionTypes(expressionTypes),
      resolvedMethodSymbols(resolvedMethodSymbols), expectingStringConversion(expectingStringConversion),
      primitiveWrappers(primitiveWrappers), constructorMap(constructorMap) {}

std::shared_ptr<Scope> BytecodeIRGeneratorListener::getCurrentScope(antlr4::ParserRuleContext* ctx) const {
  auto it = scopes.find(ctx);
  if (it != scopes.end()) {
    return it->second;
  }
  // try to find the scope in the parent context
  if (ctx->parent) {
    return getCurrentScope(dynamic_cast<antlr4::ParserRuleContext*>(ctx->parent));
  }
  return nullptr;
}

std::string BytecodeIRGeneratorListener::generateLabel() { return "L" + std::to_string(labelCounter++); }

const std::vector<std::shared_ptr<IRClass>>& BytecodeIRGeneratorListener::getClasses() const { return classes; }

int BytecodeIRGeneratorListener::assignLocalIndex(const std::shared_ptr<VariableSymbol>& variable) {
  if (variable->localIndex == -1) {
    variable->localIndex = currentLocalIndex++;
  }
  return variable->localIndex;
}

int BytecodeIRGeneratorListener::getLocalIndex(const std::string& variableName, std::shared_ptr<Scope> scope) {
  auto symbol = scope->resolve(variableName);
  if (symbol && symbol->type == SymbolType::VARIABLE) {
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(symbol);
    return assignLocalIndex(varSymbol);
  }
  // should never reach here if semantic analysis is working properly
  throw std::runtime_error("Variable not found: " + variableName);
}

void BytecodeIRGeneratorListener::generateStringConversion(antlr4::ParserRuleContext* ctx) {
  if (expectingStringConversion.count(ctx)) {
    // get the type of the expression
    auto type = expressionTypes[ctx];
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
    auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
    auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(type);

    if (pointerType) {
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokevirtual java/lang/Object.toString ()java/lang/String");
      currentFunction->instructions.push_back(rawInstruction);
    } else if (primitiveType) {
      switch (primitiveType->getPrimitiveKind()) {
      case PrimitiveType::PrimitiveKind::INT: {
        // convert int to string
        auto rawInstruction =
            std::make_shared<IRRawInstruction>("invokestatic java/lang/Integer.toString (I)java/lang/String");
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      case PrimitiveType::PrimitiveKind::FLOAT: {
        // convert float to string
        auto rawInstruction =
            std::make_shared<IRRawInstruction>("invokestatic java/lang/Float.toString (F)java/lang/String");
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      case PrimitiveType::PrimitiveKind::BOOLEAN: {
        // convert boolean to string
        auto rawInstruction =
            std::make_shared<IRRawInstruction>("invokestatic java/lang/Boolean.toString (Z)java/lang/String");
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      default:
        throw std::runtime_error("Unsupported primitive type for string conversion: " + primitiveType->toString());
      }
    } else if (userDefinedType) {
      // call the $toString method on the user-defined type
      auto rawInstruction = std::make_shared<IRRawInstruction>(
          "invokevirtual " + userDefinedType->getTypeSymbol()->name + ".$toString_() java/lang/String");
      currentFunction->instructions.push_back(rawInstruction);
    } else {
      throw std::runtime_error("Unsupported type for string conversion: " + type->toString());
    }
  }
}

void BytecodeIRGeneratorListener::enterProgram(cgullParser::ProgramContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    auto mainClass = std::make_shared<IRClass>();
    mainClass->name = "Main";
    classes.push_back(mainClass);
    currentClassStack.push(mainClass);
  } else {
    throw std::runtime_error("No scope found for program context");
  }
}

void BytecodeIRGeneratorListener::exitProgram(cgullParser::ProgramContext* ctx) {
  if (!currentClassStack.empty()) {
    currentClassStack.pop();
  }
}

void BytecodeIRGeneratorListener::enterFunction_definition(cgullParser::Function_definitionContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    currentLocalIndex = 0;

    std::string identifierName = ctx->IDENTIFIER()->getText();
    std::string specialToken = ctx->FN_SPECIAL() ? ctx->FN_SPECIAL()->getText() : "";
    std::string identifier = specialToken + identifierName;
    auto functionSymbol = scope->resolve(identifier);
    currentFunction = std::dynamic_pointer_cast<FunctionSymbol>(functionSymbol);
    auto currentClass = currentClassStack.top();
    currentClass->methods.push_back(currentFunction);

    // if this is a struct method, start local indices at 1 since 'this' is at 0
    if (currentFunction->isStructMethod) {
      currentLocalIndex = 1;
    }

    // initialize fields with default values
    if (currentFunction->name == currentClass->name) {
      for (const auto& field : currentClass->variables) {
        if (field->hasDefaultValue) {
          auto loadThis = std::make_shared<IRRawInstruction>("aload 0");
          currentFunction->instructions.push_back(loadThis);

          auto defaultValue = currentClass->defaultValues[field];
          auto loadDefault = std::make_shared<IRRawInstruction>(defaultValue);
          currentFunction->instructions.push_back(loadDefault);

          auto storeField = std::make_shared<IRRawInstruction>("putfield " + currentClass->name + "." + field->name +
                                                               " " + BytecodeCompiler::typeToJVMType(field->dataType));
          currentFunction->instructions.push_back(storeField);
        }
      }
    }
  } else {
    throw std::runtime_error("No scope found for function definition context");
  }
}

void BytecodeIRGeneratorListener::exitFunction_definition(cgullParser::Function_definitionContext* ctx) {
  if (currentFunction) {
    currentFunction = nullptr;
  }
}

void BytecodeIRGeneratorListener::enterParameter(cgullParser::ParameterContext* ctx) {
  // assign a local index to the parameter
  auto scope = getCurrentScope(ctx);
  if (scope) {
    auto parameterSymbol = scope->resolve(ctx->IDENTIFIER()->getText());
    auto parameterVarSymbol = std::dynamic_pointer_cast<VariableSymbol>(parameterSymbol);
    assignLocalIndex(parameterVarSymbol);
  } else {
    throw std::runtime_error("No scope found for parameter context");
  }
}

void BytecodeIRGeneratorListener::enterFunction_call(cgullParser::Function_callContext* ctx) {
  // for cases where methods are called on objects, to be filled later HW5
  auto scope = getCurrentScope(ctx);
  if (scope) {
    std::string identifierName = ctx->IDENTIFIER()->getText();
    std::string specialToken = ctx->FN_SPECIAL() ? ctx->FN_SPECIAL()->getText() : "";
    std::string identifier = specialToken + identifierName;
    // try to see if its a constructor first
    auto constructor = constructorMap.find(identifier);
    std::shared_ptr<FunctionSymbol> functionSymbol;
    if (constructor != constructorMap.end()) {
      functionSymbol = constructor->second;
      // we need a new instruction with dup to create a new object, will be called later in exitFunction_call
      auto newInstruction = std::make_shared<IRRawInstruction>("new " + functionSymbol->returnTypes[0]->toString());
      currentFunction->instructions.push_back(newInstruction);
      auto dupInstruction = std::make_shared<IRRawInstruction>("dup");
      currentFunction->instructions.push_back(dupInstruction);
    } else if (lastFieldType) {
      // is part of a field access, check the struct scope instead
      auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(lastFieldType);
      functionSymbol =
          std::dynamic_pointer_cast<FunctionSymbol>(userDefinedType->getTypeSymbol()->scope->resolve(identifier));
    } else {
      functionSymbol = std::dynamic_pointer_cast<FunctionSymbol>(scope->resolve(identifier));
    }
    if (!functionSymbol) {
      throw std::runtime_error("Function not found: " + identifier);
    }
    if (functionSymbol->name == "print" || functionSymbol->name == "println") {
      // special case for print/println, we need to add a raw instruction
      auto rawInstruction = std::make_shared<IRRawInstruction>("getstatic java/lang/System.out java/io/PrintStream");
      currentFunction->instructions.push_back(rawInstruction);
    }
    if (functionSymbol->scope->resolve("this") && currentFunction->isStructMethod) {
      auto loadThis = std::make_shared<IRRawInstruction>("aload 0");
      currentFunction->instructions.push_back(loadThis);
    }
  } else {
    throw std::runtime_error("No scope found for function call context");
  }
}

void BytecodeIRGeneratorListener::exitFunction_call(cgullParser::Function_callContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    // the expressions in the parameters are now evaluated and on the stack, so put a function call instruction
    // on the stack
    std::string identifierName = ctx->IDENTIFIER()->getText();
    std::string specialToken = ctx->FN_SPECIAL() ? ctx->FN_SPECIAL()->getText() : "";
    std::string identifier = specialToken + identifierName;
    auto constructor = constructorMap.find(identifier);
    std::shared_ptr<FunctionSymbol> calledFunction;
    if (constructor != constructorMap.end()) {
      calledFunction = constructor->second;
    } else if (lastFieldType) {
      // is part of a field access, check the struct scope instead
      auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(lastFieldType);
      calledFunction =
          std::dynamic_pointer_cast<FunctionSymbol>(userDefinedType->getTypeSymbol()->scope->resolve(identifier));
    } else {
      // check scope as normal
      calledFunction = std::dynamic_pointer_cast<FunctionSymbol>(scope->resolve(identifier));
    }
    if (!calledFunction) {
      throw std::runtime_error("Function not found: " + identifier);
    }
    // program will jump and handle it, for generating IR we are done here, just add the call instruction
    auto callInstruction = std::make_shared<IRCallInstruction>(calledFunction);
    currentFunction->instructions.push_back(callInstruction);
  } else {
    throw std::runtime_error("No scope found for function call context");
  }
}

void BytecodeIRGeneratorListener::enterExpression(cgullParser::ExpressionContext* ctx) {
  auto parent = ctx->parent;
  if (!parent)
    return;
  auto forStmt = dynamic_cast<cgullParser::For_statementContext*>(parent);
  // first expression is the condition
  if (forStmt && forStmt->expression(0) == ctx) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // place label for the condition
      auto labelInst = std::make_shared<IRRawInstruction>(labels.conditionLabel + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  }
  if (forStmt && forStmt->expression(1) == ctx) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // place label for the update expr
      auto labelInst = std::make_shared<IRRawInstruction>(labels.updateLabel + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  }
  // check if part of expression_list that is part of an array_expression, dup the array ref and place the index
  auto expressionList = dynamic_cast<cgullParser::Expression_listContext*>(ctx->parent);
  if (expressionList) {
    auto arrayExpr = dynamic_cast<cgullParser::Array_expressionContext*>(expressionList->parent);
    if (arrayExpr) {
      auto rawInstruction = std::make_shared<IRRawInstruction>("dup");
      currentFunction->instructions.push_back(rawInstruction);
      // find the index of the expression in the expression list
      for (size_t i = 0; i < expressionList->expression().size(); ++i) {
        if (expressionList->expression(i) == ctx) {
          auto indexInst = std::make_shared<IRRawInstruction>("ldc " + std::to_string(i));
          currentFunction->instructions.push_back(indexInst);
          break;
        }
      }
    }
  }
}

void BytecodeIRGeneratorListener::exitExpression(cgullParser::ExpressionContext* ctx) {
  generateStringConversion(ctx);

  // check if this expression is part of an if statement condition so we can place jumps
  auto parent = ctx->parent;
  if (!parent)
    return;
  auto ifStmt = dynamic_cast<cgullParser::If_statementContext*>(parent);
  if (ifStmt) {
    // only process the first condition
    if (ifStmt->expression(0) == ctx) {
      auto it = ifLabelsMap.find(ifStmt);
      if (it != ifLabelsMap.end()) {
        auto& labels = it->second;
        // if condition is false, jump to the first elseif/else branch or end
        std::string jumpTarget = labels.conditionLabels.size() > 1 ? labels.conditionLabels[1] : labels.endIfLabel;

        auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + jumpTarget);
        currentFunction->instructions.push_back(jumpInst);
      }
    }
    // handle elseif conditions
    else {
      // find which elseif condition this is
      for (size_t i = 0; i < ifStmt->ELSE_IF().size(); ++i) {
        // +1 because index 0 is the main if condition
        if (ifStmt->expression(i + 1) == ctx) {
          auto it = ifLabelsMap.find(ifStmt);
          if (it != ifLabelsMap.end()) {
            auto& labels = it->second;

            // if this elseif condition is false, jump to the next elseif/else branch or end
            std::string jumpTarget;
            if (i + 2 < labels.conditionLabels.size()) {
              jumpTarget = labels.conditionLabels[i + 2];
            } else {
              jumpTarget = labels.endIfLabel;
            }

            auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + jumpTarget);
            currentFunction->instructions.push_back(jumpInst);
            break;
          }
        }
      }
    }
  }
  auto whileStmt = dynamic_cast<cgullParser::While_statementContext*>(parent);
  if (whileStmt) {
    // place the jump to the end of the loop if the condition is false
    auto it = whileLabelsMap.find(whileStmt);
    if (it != whileLabelsMap.end()) {
      auto& labels = it->second;
      auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + labels.endLabel);
      currentFunction->instructions.push_back(jumpInst);
    }
  }
  auto untilStmt = dynamic_cast<cgullParser::Until_statementContext*>(parent);
  if (untilStmt) {
    auto it = untilLabelsMap.find(untilStmt);
    if (it != untilLabelsMap.end()) {
      auto& labels = it->second;
      // this is the expression after the branch block, jump to the top of the loop if the condition is false
      auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + labels.startLabel);
      currentFunction->instructions.push_back(jumpInst);
    }
  }
  auto forStmt = dynamic_cast<cgullParser::For_statementContext*>(parent);
  if (forStmt && forStmt->expression(0) == ctx) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // jump to the end of the loop if false, this is the condition, otherwise jump to the branch block
      auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + labels.endLabel);
      currentFunction->instructions.push_back(jumpInst);
      // jump to the branch block
      auto jumpInst2 = std::make_shared<IRRawInstruction>("goto " + labels.startLabel);
      currentFunction->instructions.push_back(jumpInst2);
    }
  }
  if (forStmt && forStmt->expression(1) == ctx) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // pop the value from the stack, we aren't using it
      auto popInst = std::make_shared<IRRawInstruction>("pop");
      currentFunction->instructions.push_back(popInst);
      // jump back to the conditional, we're exiting the update expr
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.conditionLabel);
      currentFunction->instructions.push_back(jumpInst);
    }
  }
  // check if parent is index_expression and not the last expression in the index_expression, if so place an aaload
  // instruction
  auto indexExpr = dynamic_cast<cgullParser::Index_expressionContext*>(parent);
  if (indexExpr && indexExpr->expression(indexExpr->expression().size() - 1) != ctx) {
    auto rawInstruction = std::make_shared<IRRawInstruction>("aaload");
    currentFunction->instructions.push_back(rawInstruction);
  }
  // if its the last expression and not used in an assignment, load based on type (e.g. iaload, aaload, etc.)
  if (indexExpr && indexExpr->expression(indexExpr->expression().size() - 1) == ctx) {
    // check if we're in an assignment context by walking up the parent chain
    bool isAssignment = false;
    auto current = ctx->parent;
    while (current) {
      if (dynamic_cast<cgullParser::Assignment_statementContext*>(current)) {
        isAssignment = true;
        break;
      }
      current = current->parent;
    }

    if (!isAssignment) {
      auto type = expressionTypes[indexExpr];
      if (!type) {
        throw std::runtime_error("Type not found for expression: " + indexExpr->getText());
      }
      auto rawInstruction = std::make_shared<IRRawInstruction>(getArrayOperationInstruction(type, false));
      currentFunction->instructions.push_back(rawInstruction);
    }
  }
  // check if part of expression_list that is part of an array_expression, we will need to store based on type
  auto expressionList = dynamic_cast<cgullParser::Expression_listContext*>(ctx->parent);
  if (expressionList) {
    auto arrayExpr = dynamic_cast<cgullParser::Array_expressionContext*>(expressionList->parent);
    if (arrayExpr) {
      auto arrayType = std::dynamic_pointer_cast<ArrayType>(expressionTypes[arrayExpr]);
      if (!arrayType) {
        throw std::runtime_error("Type not found for expression: " + arrayExpr->getText());
      }
      auto type = arrayType->getElementType();
      if (!type) {
        throw std::runtime_error("Type not found for expression: " + arrayExpr->getText());
      }
      auto rawInstruction = std::make_shared<IRRawInstruction>(getArrayOperationInstruction(type, true));
      currentFunction->instructions.push_back(rawInstruction);
    }
  }
}

void BytecodeIRGeneratorListener::enterBase_expression(cgullParser::Base_expressionContext* ctx) {
  // we're in a struct, don't generate instructions
  if (!currentFunction) {
    return;
  }

  if (ctx->AND_OP() || ctx->OR_OP()) {
    // reserve and setup metadata for logical expressions
    auto it = expressionLabelsMap.find(ctx);
    if (it == expressionLabelsMap.end()) {
      handleLogicalExpression(ctx);
    }
  }

  // handle pushing literals
  if (ctx->literal()) {
    // check what type of literal it is from our expression types
    auto literal = ctx->literal();
    auto type = expressionTypes[literal];
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
    auto pointerType = std::dynamic_pointer_cast<PointerType>(type);

    if (primitiveType) {
      PrimitiveType::PrimitiveKind primitiveKind = primitiveType->getPrimitiveKind();
      switch (primitiveKind) {
      case PrimitiveType::PrimitiveKind::BOOLEAN: {
        if (literal->getText() == "true") {
          auto rawInstruction = std::make_shared<IRRawInstruction>("iconst 1");
          currentFunction->instructions.push_back(rawInstruction);
        } else {
          auto rawInstruction = std::make_shared<IRRawInstruction>("iconst 0");
          currentFunction->instructions.push_back(rawInstruction);
        }
        break;
      }
      case PrimitiveType::PrimitiveKind::INT: {
        // handle NUMBER_LITERAL, HEX_LITERAL, BINARY_LITERAL
        std::string literalText = literal->getText();
        if (ctx->literal()->HEX_LITERAL()) {
          // convert hex literal to int
          std::stringstream ss;
          ss << std::hex << literalText;
          int value;
          ss >> value;
          literalText = std::to_string(value);
        } else if (ctx->literal()->BINARY_LITERAL()) {
          // cut off the 0b prefix
          literalText = literalText.substr(2);
          int value = std::stoi(literalText, nullptr, 2);
          literalText = std::to_string(value);
        }
        auto rawInstruction = std::make_shared<IRRawInstruction>("ldc " + literalText);
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      case PrimitiveType::PrimitiveKind::FLOAT:
      case PrimitiveType::PrimitiveKind::STRING: {
        auto rawInstruction = std::make_shared<IRRawInstruction>("ldc " + literal->getText());
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      default:
        throw std::runtime_error("Unsupported literal type: " + primitiveType->toString());
      }
    } else if (pointerType) {
      // handle pointer types
      if (literal->getText() == "nullptr") {
        auto rawInstruction = std::make_shared<IRRawInstruction>("aconst_null");
        currentFunction->instructions.push_back(rawInstruction);
      } else {
        throw std::runtime_error("Unsupported literal type: " + type->toString());
      }
    } else {
      throw std::runtime_error("Unsupported literal type: " + type->toString());
    }
  }
}

void BytecodeIRGeneratorListener::exitBase_expression(cgullParser::Base_expressionContext* ctx) {
  // handle binary operations after both operands have been processed
  auto type = expressionTypes[ctx];
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
  if (!primitiveType) {
    generateStringConversion(ctx);
    return;
  }
  if (primitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
    if (ctx->PLUS_OP()) {
      auto rawInstruction = std::make_shared<IRRawInstruction>(
          "invokedynamic makeConcatWithConstants(java/lang/String,java/lang/String,)java/lang/String { "
          "invokestatic "
          "java/lang/invoke/StringConcatFactory.makeConcatWithConstants(java/lang/invoke/MethodHandles$Lookup,"
          "java/lang/String,java/lang/invoke/MethodType,java/lang/String,[java/lang/Object)"
          "java/lang/invoke/CallSite[\"\u0001\u0001\"]}");
      currentFunction->instructions.push_back(rawInstruction);
    }
  } else {
    std::string prefix;
    switch (primitiveType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::INT:
      prefix = "i";
      break;
    case PrimitiveType::PrimitiveKind::FLOAT:
      prefix = "f";
      break;
    case PrimitiveType::PrimitiveKind::BOOLEAN:
      prefix = "i";
      break;
    default:
      throw std::runtime_error("Unsupported primitive type for binary operation: " + primitiveType->toString());
    }

    if (ctx->PLUS_OP()) {
      auto plusInstruction = std::make_shared<IRRawInstruction>("" + prefix + "add");
      currentFunction->instructions.push_back(plusInstruction);
    } else if (ctx->MINUS_OP()) {
      auto minusInstruction = std::make_shared<IRRawInstruction>("" + prefix + "sub");
      currentFunction->instructions.push_back(minusInstruction);
    } else if (ctx->MULT_OP()) {
      auto multInstruction = std::make_shared<IRRawInstruction>("" + prefix + "mul");
      currentFunction->instructions.push_back(multInstruction);
    } else if (ctx->DIV_OP()) {
      auto divInstruction = std::make_shared<IRRawInstruction>("" + prefix + "div");
      currentFunction->instructions.push_back(divInstruction);
    } else if (ctx->MOD_OP()) {
      auto modInstruction = std::make_shared<IRRawInstruction>("" + prefix + "rem");
      currentFunction->instructions.push_back(modInstruction);
    } else if (ctx->BITWISE_LEFT_SHIFT_OP()) {
      if (prefix == "i") {
        auto shiftLeftInstruction = std::make_shared<IRRawInstruction>("" + prefix + "shl");
        currentFunction->instructions.push_back(shiftLeftInstruction);
      } else {
        throw std::runtime_error("Unsupported shift left operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_RIGHT_SHIFT_OP()) {
      if (prefix == "i") {
        auto shiftRightInstruction = std::make_shared<IRRawInstruction>("" + prefix + "shr");
        currentFunction->instructions.push_back(shiftRightInstruction);
      } else {
        throw std::runtime_error("Unsupported shift right operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_AND_OP()) {
      if (prefix == "i") {
        auto bitwiseAndInstruction = std::make_shared<IRRawInstruction>("" + prefix + "and");
        currentFunction->instructions.push_back(bitwiseAndInstruction);
      } else {
        throw std::runtime_error("Unsupported bitwise and operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_OR_OP()) {
      if (prefix == "i") {
        auto bitwiseOrInstruction = std::make_shared<IRRawInstruction>("" + prefix + "or");
        currentFunction->instructions.push_back(bitwiseOrInstruction);
      } else {
        throw std::runtime_error("Unsupported bitwise or operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_XOR_OP()) {
      if (prefix == "i") {
        auto bitwiseXorInstruction = std::make_shared<IRRawInstruction>("" + prefix + "xor");
        currentFunction->instructions.push_back(bitwiseXorInstruction);
      } else {
        throw std::runtime_error("Unsupported bitwise xor operation for type: " + primitiveType->toString());
      }
    } else if (ctx->EQUAL_OP() || ctx->NOT_EQUAL_OP() || ctx->LESS_OP() || ctx->GREATER_OP() || ctx->LESS_EQUAL_OP() ||
               ctx->GREATER_EQUAL_OP()) {
      // evaluate the expression
      std::string trueLabel = generateLabel();
      std::string endLabel = generateLabel();

      // handle strings
      auto leftExpr = ctx->base_expression(0);
      auto rightExpr = ctx->base_expression(1);
      auto leftType = expressionTypes[leftExpr];
      auto rightType = expressionTypes[rightExpr];
      auto leftPrimitiveType = std::dynamic_pointer_cast<PrimitiveType>(leftType);
      auto rightPrimitiveType = std::dynamic_pointer_cast<PrimitiveType>(rightType);

      if (leftPrimitiveType && rightPrimitiveType &&
          (leftPrimitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING ||
           rightPrimitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING)) {
        if (!ctx->EQUAL_OP() && !ctx->NOT_EQUAL_OP()) {
          throw std::runtime_error("Unsupported string comparison operation: " + ctx->getText());
        }
        // call .equals on the two values
        auto rawInstruction =
            std::make_shared<IRRawInstruction>("invokevirtual java/lang/String.equals(java/lang/Object)Z");
        currentFunction->instructions.push_back(rawInstruction);
        if (ctx->NOT_EQUAL_OP()) {
          auto rawInstructionPushTrue = std::make_shared<IRRawInstruction>("iconst 1");
          currentFunction->instructions.push_back(rawInstructionPushTrue);
          auto rawInstructionXor = std::make_shared<IRRawInstruction>("ixor");
          currentFunction->instructions.push_back(rawInstructionXor);
        }
        return;
      }
      if (leftPrimitiveType && leftPrimitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::INT) {
        if (ctx->EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpeq " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->NOT_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpne " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->LESS_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmplt " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->GREATER_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpgt " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->LESS_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmple " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->GREATER_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpge " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        }
      } else if (leftPrimitiveType && leftPrimitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) {
        if (ctx->EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpeq " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->NOT_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_icmpne " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else {
          throw std::runtime_error("Unsupported comparison operation for boolean type");
        }
      } else if (leftPrimitiveType && leftPrimitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::FLOAT) {
        auto rawCmpInstruction = std::make_shared<IRRawInstruction>("fcmpg");
        currentFunction->instructions.push_back(rawCmpInstruction);
        if (ctx->EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("ifeq " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->NOT_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("ifne " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->LESS_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("iflt " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->GREATER_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("ifgt " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->LESS_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("ifle " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->GREATER_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("ifge " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        }
      } else if (leftType->getKind() == Type::TypeKind::USER_DEFINED ||
                 (leftType->getKind() == Type::TypeKind::POINTER &&
                  std::dynamic_pointer_cast<PointerType>(leftType)->getPointedType()->getKind() ==
                      Type::TypeKind::USER_DEFINED)) {
        // for user-defined types, we can only compare with nullptr using == and !=
        if (ctx->EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_acmpeq " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else if (ctx->NOT_EQUAL_OP()) {
          auto rawInstruction = std::make_shared<IRRawInstruction>("if_acmpne " + trueLabel);
          currentFunction->instructions.push_back(rawInstruction);
        } else {
          throw std::runtime_error("Unsupported comparison operation for type: " + leftType->toString());
        }
      } else {
        throw std::runtime_error("Unsupported comparison operation for type: " + leftType->toString());
      }

      // we didn't jump to trueLabel, so the condition is false
      auto pushFalseInstruction = std::make_shared<IRRawInstruction>("iconst 0");
      currentFunction->instructions.push_back(pushFalseInstruction);
      // jump to endLabel
      auto jumpInstruction = std::make_shared<IRRawInstruction>("goto " + endLabel);
      currentFunction->instructions.push_back(jumpInstruction);
      // trueLabel:
      auto trueLabelInstruction = std::make_shared<IRRawInstruction>(trueLabel + ":");
      currentFunction->instructions.push_back(trueLabelInstruction);
      auto pushTrueInstruction = std::make_shared<IRRawInstruction>("iconst 1");
      currentFunction->instructions.push_back(pushTrueInstruction);
      // endLabel:
      auto endLabelInstruction = std::make_shared<IRRawInstruction>(endLabel + ":");
      currentFunction->instructions.push_back(endLabelInstruction);
    } else if (ctx->AND_OP()) {
      // retrieve the labels for this expression
      auto it = expressionLabelsMap.find(ctx);
      if (it == expressionLabelsMap.end()) {
        handleLogicalExpression(ctx);
        it = expressionLabelsMap.find(ctx);
      }

      ExpressionLabels& labels = it->second;

      // only place the exit label if we've processed the expression
      if (labels.processed) {
        auto exitLabel = std::make_shared<IRRawInstruction>(labels.exitLabel + ":");
        currentFunction->instructions.push_back(exitLabel);
      }

    } else if (ctx->OR_OP()) {
      // retrieve the labels for this expression
      auto it = expressionLabelsMap.find(ctx);
      if (it == expressionLabelsMap.end()) {
        handleLogicalExpression(ctx);
        it = expressionLabelsMap.find(ctx);
      }

      ExpressionLabels& labels = it->second;

      // only place the exit label if we've processed the expression
      if (labels.processed) {
        auto exitLabel = std::make_shared<IRRawInstruction>(labels.exitLabel + ":");
        currentFunction->instructions.push_back(exitLabel);
      }
    }
  }
  generateStringConversion(ctx);

  // handle if expressions

  auto parent = ctx->parent;
  if (!parent)
    return;
  auto ifExpr = dynamic_cast<cgullParser::If_expressionContext*>(parent);
  if (ifExpr && ifExpr->base_expression(0) == ctx) {
    // jump to the else expression if stack is false
    auto it = ifExpressionLabelsMap.find(ifExpr);
    if (it != ifExpressionLabelsMap.end()) {
      auto& labels = it->second;
      auto jumpInst = std::make_shared<IRRawInstruction>("ifeq " + labels.conditionLabels[0]);
      currentFunction->instructions.push_back(jumpInst);
    }
  } else if (ifExpr && ifExpr->base_expression(1) == ctx) {
    // jump to the end of the if expression
    auto it = ifExpressionLabelsMap.find(ifExpr);
    if (it != ifExpressionLabelsMap.end()) {
      auto& labels = it->second;
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.endIfLabel);
      currentFunction->instructions.push_back(jumpInst);
      // place label for jumping to the else condition
      auto labelInst = std::make_shared<IRRawInstruction>(labels.conditionLabels[0] + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  } else if (ifExpr && ifExpr->base_expression(2) == ctx) {
    // place label for jumping to the end of the if expression
    auto it = ifExpressionLabelsMap.find(ifExpr);
    if (it != ifExpressionLabelsMap.end()) {
      auto& labels = it->second;
      auto labelInst = std::make_shared<IRRawInstruction>(labels.endIfLabel + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  }

  // handle AND/OR parent relationships for short-circuiting
  if (ctx->parent) {
    auto parentCtx = dynamic_cast<cgullParser::Base_expressionContext*>(ctx->parent);
    if (parentCtx) {
      auto it = expressionLabelsMap.find(parentCtx);
      if (it != expressionLabelsMap.end() && !it->second.processed) {
        ExpressionLabels& labels = it->second;

        if (parentCtx->AND_OP() && ctx == parentCtx->base_expression(0)) {
          // left side of AND finished evaluating, if it was false jump to fallthrough
          auto leftFalseJump = std::make_shared<IRRawInstruction>("ifeq " + labels.fallthroughLabel);
          currentFunction->instructions.push_back(leftFalseJump);
        } else if (parentCtx->OR_OP() && ctx == parentCtx->base_expression(0)) {
          // left side of OR finished evaluating, if it was true jump to fallthrough (opposite of AND)
          auto leftTrueJump = std::make_shared<IRRawInstruction>("ifne " + labels.fallthroughLabel);
          currentFunction->instructions.push_back(leftTrueJump);
        } else if (parentCtx->AND_OP() && ctx == parentCtx->base_expression(1)) {
          // right side of AND finished evaluating, if it was false jump to fallthrough
          auto rightFalseJump = std::make_shared<IRRawInstruction>("ifeq " + labels.fallthroughLabel);
          currentFunction->instructions.push_back(rightFalseJump);

          // push true since both operands are true
          auto pushTrue = std::make_shared<IRRawInstruction>("iconst 1");
          currentFunction->instructions.push_back(pushTrue);

          // jump to the exit label (both operands are true)
          auto jumpToExit = std::make_shared<IRRawInstruction>("goto " + labels.exitLabel);
          currentFunction->instructions.push_back(jumpToExit);

          // place the fallthrough label here (one of the operands was false)
          auto fallthroughLabel = std::make_shared<IRRawInstruction>(labels.fallthroughLabel + ":");
          currentFunction->instructions.push_back(fallthroughLabel);

          // push false since one of the operands was false
          auto pushFalse = std::make_shared<IRRawInstruction>("iconst 0");
          currentFunction->instructions.push_back(pushFalse);

          // mark this expression as processed to avoid duplicate label placement
          labels.processed = true;
        } else if (parentCtx->OR_OP() && ctx == parentCtx->base_expression(1)) {
          // right side of OR finished evaluating, if it was true jump to fallthrough
          auto rightTrueJump = std::make_shared<IRRawInstruction>("ifne " + labels.fallthroughLabel);
          currentFunction->instructions.push_back(rightTrueJump);

          // push false since both operands are false
          auto pushFalse = std::make_shared<IRRawInstruction>("iconst 0");
          currentFunction->instructions.push_back(pushFalse);

          // jump to the exit label (both operands are false)
          auto jumpToExit = std::make_shared<IRRawInstruction>("goto " + labels.exitLabel);
          currentFunction->instructions.push_back(jumpToExit);

          // place the fallthrough label here (one of the operands was true)
          auto fallthroughLabel = std::make_shared<IRRawInstruction>(labels.fallthroughLabel + ":");
          currentFunction->instructions.push_back(fallthroughLabel);
          auto pushTrue = std::make_shared<IRRawInstruction>("iconst 1");
          currentFunction->instructions.push_back(pushTrue);

          // again, mark this expression as processed to avoid duplicate label placement
          labels.processed = true;
        }
      }
    }
  }
}

void BytecodeIRGeneratorListener::enterVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));
    // if we're not in a function, this is a struct variable
    if (!currentFunction) {
      auto structClass = currentClassStack.top();
      structClass->variables.push_back(varSymbol);
      return;
    }
    if (varSymbol) {
      // assign a local index to this variable
      int localIndex = assignLocalIndex(varSymbol);

      // if this is a struct field with a default value, store it in the IRClass
      if (varSymbol->isStructMember && varSymbol->hasDefaultValue) {
        auto currentClass = currentClassStack.top();
        if (ctx->expression()) {
          auto type = varSymbol->dataType;
          auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
          if (primitiveType) {
            switch (primitiveType->getPrimitiveKind()) {
            case PrimitiveType::PrimitiveKind::INT:
              currentClass->defaultValues[varSymbol] = "iconst 0";
              break;
            case PrimitiveType::PrimitiveKind::FLOAT:
              currentClass->defaultValues[varSymbol] = "fconst 0";
              break;
            case PrimitiveType::PrimitiveKind::STRING:
              currentClass->defaultValues[varSymbol] = "ldc \"\"";
              break;
            case PrimitiveType::PrimitiveKind::BOOLEAN:
              currentClass->defaultValues[varSymbol] = "iconst 0";
              break;
            default:
              currentClass->defaultValues[varSymbol] = "aconst_null";
            }
          } else {
            currentClass->defaultValues[varSymbol] = "aconst_null";
          }
        }
      }
    }
  }
}

void BytecodeIRGeneratorListener::exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  // we're in a struct, don't generate instructions
  if (!currentFunction) {
    return;
  }

  // if there's an initialization expression, store its result in the variable
  if (ctx->expression()) {
    auto scope = getCurrentScope(ctx);
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol) {
      auto type = varSymbol->dataType;
      auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
      auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
      auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);
      auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(type);

      // if this is a struct field with a default value, store it in the IRClass
      if (varSymbol->isStructMember && varSymbol->hasDefaultValue) {
        auto currentClass = currentClassStack.top();
        currentClass->variables.push_back(varSymbol);
        // the default value is already on the stack, so we can store it in the field
        auto storeField = std::make_shared<IRRawInstruction>("putfield " + currentClass->name + "." + identifier + " " +
                                                             BytecodeCompiler::typeToJVMType(type));
        currentFunction->instructions.push_back(storeField);
        return;
      }

      if (userDefinedType) {
        auto storeInst = std::make_shared<IRRawInstruction>("astore " + std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(storeInst);
        return;
      }

      if (primitiveType) {
        std::string storeInstruction;
        switch (primitiveType->getPrimitiveKind()) {
        case PrimitiveType::PrimitiveKind::INT:
          storeInstruction = "istore " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::FLOAT:
          storeInstruction = "fstore " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::STRING:
          storeInstruction = "astore " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::BOOLEAN:
          storeInstruction = "istore " + std::to_string(varSymbol->localIndex);
          break;
        default:
          throw std::runtime_error("Unsupported variable type for storage: " + primitiveType->toString());
        }

        auto storeInst = std::make_shared<IRRawInstruction>(storeInstruction);
        currentFunction->instructions.push_back(storeInst);
      }
      if (pointerType || arrayType) {
        auto storeInstruction = std::make_shared<IRRawInstruction>("astore " + std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(storeInstruction);
      }
    }
  }
}

void BytecodeIRGeneratorListener::enterVariable(cgullParser::VariableContext* ctx) {
  if (ctx->IDENTIFIER()) {
    auto scope = getCurrentScope(ctx);
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol && varSymbol->isStructMember) {
      auto loadInstruction = std::make_shared<IRRawInstruction>("aload 0");
      currentFunction->instructions.push_back(loadInstruction);
    }
  }
}

void BytecodeIRGeneratorListener::exitVariable(cgullParser::VariableContext* ctx) {
  // if we're evaluating a variable as a value (not as a target), load its value onto the stack
  if (ctx->IDENTIFIER() && ctx->parent && !dynamic_cast<cgullParser::Assignment_statementContext*>(ctx->parent)) {
    auto scope = getCurrentScope(ctx);
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol) {
      auto type = varSymbol->dataType;
      auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
      auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
      auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);
      auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(type);

      if (varSymbol->isStructMember) {
        std::string className = varSymbol->parentStructType->name;
        auto getField = std::make_shared<IRRawInstruction>(
            "getfield " + className + "." + ctx->IDENTIFIER()->getText() + " " + BytecodeCompiler::typeToJVMType(type));
        currentFunction->instructions.push_back(getField);
      } else {
        if (pointerType || arrayType || userDefinedType) {
          auto loadInstruction = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
          currentFunction->instructions.push_back(loadInstruction);
        }

        if (primitiveType) {
          // load value from the local variable onto the stack
          auto loadInst = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                             std::to_string(varSymbol->localIndex));
          currentFunction->instructions.push_back(loadInst);
        }
      }
    }
  }
}

void BytecodeIRGeneratorListener::exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) {
  if (ctx->dereference_expression()) {
    auto scope = getCurrentScope(ctx);
    auto expression = ctx->expression();
    auto expressionType = expressionTypes[expression];
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(expressionType);
    if (!primitiveType) {
      throw std::runtime_error("Unsupported assignment type: " + expressionType->toString());
    }
    auto wrapperClass = PrimitiveWrapperGenerator::generateWrapperClass(primitiveType->getPrimitiveKind());
    auto method = wrapperClass->getMethod("setValue");
    auto invokeInst =
        std::make_shared<IRRawInstruction>("invokevirtual " + wrapperClass->name + "." + method->getMangledName() +
                                           "(" + BytecodeCompiler::typeToJVMType(expressionType) + ")V");
    currentFunction->instructions.push_back(invokeInst);
  } else if (ctx->variable() && ctx->expression()) {
    auto scope = getCurrentScope(ctx);
    auto variable = ctx->variable();
    auto type = expressionTypes[variable];

    if (variable->field_access()) {
      auto fieldAccess = variable->field_access();
      auto lastField = fieldAccess->field(fieldAccess->field().size() - 1);
      auto lastStruct = fieldAccess->field(fieldAccess->field().size() - 2);
      auto structType = expressionTypes[lastStruct];
      auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(structType);
      if (userDefinedType && lastField->IDENTIFIER()) {
        auto structSymbol = userDefinedType->getTypeSymbol();
        auto fieldTypeSymbol =
            std::dynamic_pointer_cast<VariableSymbol>(structSymbol->scope->resolve(lastField->IDENTIFIER()->getText()));
        if (fieldTypeSymbol) {
          auto putFieldInst =
              std::make_shared<IRRawInstruction>("putfield " + structSymbol->name + "." + fieldTypeSymbol->name + " " +
                                                 BytecodeCompiler::typeToJVMType(fieldTypeSymbol->dataType));
          currentFunction->instructions.push_back(putFieldInst);
        }
      } else if (userDefinedType && lastField->index_expression()) {
        auto structSymbol = userDefinedType->getTypeSymbol();
        auto fieldTypeSymbol = std::dynamic_pointer_cast<VariableSymbol>(
            structSymbol->scope->resolve(lastField->index_expression()->indexable()->IDENTIFIER()->getText()));
        if (fieldTypeSymbol) {
          auto arrayType = std::dynamic_pointer_cast<ArrayType>(fieldTypeSymbol->dataType);
          auto arrayOperationInstruction = getArrayOperationInstruction(arrayType->getElementType(), true);
          auto putFieldInst = std::make_shared<IRRawInstruction>(arrayOperationInstruction);
          currentFunction->instructions.push_back(putFieldInst);
        }
      }
    } else {
      auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(variable->IDENTIFIER()->getText()));
      if (varSymbol) {
        auto type = varSymbol->dataType;
        auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
        auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
        auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);
        auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(type);

        if (varSymbol->isStructMember) {
          auto putFieldInst =
              std::make_shared<IRRawInstruction>("putfield " + varSymbol->parentStructType->name + "." +
                                                 varSymbol->name + " " + BytecodeCompiler::typeToJVMType(type));
          currentFunction->instructions.push_back(putFieldInst);
        } else {
          if (pointerType || arrayType || userDefinedType) {
            auto storeInstruction =
                std::make_shared<IRRawInstruction>("astore " + std::to_string(varSymbol->localIndex));
            currentFunction->instructions.push_back(storeInstruction);
          }

          if (primitiveType) {
            // expression result is already on the stack, store it in the variable
            std::string storeInstruction;
            switch (primitiveType->getPrimitiveKind()) {
            case PrimitiveType::PrimitiveKind::INT:
              storeInstruction = "istore " + std::to_string(varSymbol->localIndex);
              break;
            case PrimitiveType::PrimitiveKind::FLOAT:
              storeInstruction = "fstore " + std::to_string(varSymbol->localIndex);
              break;
            case PrimitiveType::PrimitiveKind::STRING:
              storeInstruction = "astore " + std::to_string(varSymbol->localIndex);
              break;
            case PrimitiveType::PrimitiveKind::BOOLEAN:
              storeInstruction = "istore " + std::to_string(varSymbol->localIndex);
              break;

            default:
              throw std::runtime_error("Unsupported variable type for assignment: " + primitiveType->toString());
            }

            auto storeInst = std::make_shared<IRRawInstruction>(storeInstruction);
            currentFunction->instructions.push_back(storeInst);
          }
        }
      }
    }
  } else if (ctx->index_expression()) {
    auto scope = getCurrentScope(ctx);
    auto type = expressionTypes[ctx->index_expression()];
    auto rawInstruction = std::make_shared<IRRawInstruction>(getArrayOperationInstruction(type, true));
    currentFunction->instructions.push_back(rawInstruction);
  }
}

void BytecodeIRGeneratorListener::exitReturn_statement(cgullParser::Return_statementContext* ctx) {
  std::shared_ptr<IRRawInstruction> returnInst;
  auto returnType = currentFunction->returnTypes[0];
  auto voidType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);
  if (returnType->equals(voidType)) {
    returnInst = std::make_shared<IRRawInstruction>("return");
  } else if (returnType->equals(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::INT))) {
    returnInst = std::make_shared<IRRawInstruction>("ireturn");
  } else if (returnType->equals(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::FLOAT))) {
    returnInst = std::make_shared<IRRawInstruction>("freturn");
  } else if (returnType->equals(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::BOOLEAN))) {
    returnInst = std::make_shared<IRRawInstruction>("ireturn");
  } else {
    returnInst = std::make_shared<IRRawInstruction>("areturn");
  }
  currentFunction->instructions.push_back(returnInst);
}

void BytecodeIRGeneratorListener::exitUnary_expression(cgullParser::Unary_expressionContext* ctx) {
  // expression result is already on the stack
  auto expressionCtx = ctx->expression();
  auto expressionType = expressionTypes[expressionCtx];
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(expressionType);
  if (!primitiveType) {
    return;
  }
  auto typeKind = primitiveType->getPrimitiveKind();
  if (ctx->PLUS_OP() || ctx->MINUS_OP() || ctx->NOT_OP() || ctx->BITWISE_NOT_OP() || ctx->INCREMENT_OP() ||
      ctx->DECREMENT_OP()) {
    if (!primitiveType || !primitiveType->isNumeric()) {
      throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
    }

    if (ctx->PLUS_OP()) {
      // there is no + unary operator in java,
      // so essentially insert an if statement where if its less than 0, multiply by -1
      auto endLabel = generateLabel();
      auto dupInstruction = std::make_shared<IRRawInstruction>("dup");
      currentFunction->instructions.push_back(dupInstruction);
      if (typeKind == PrimitiveType::PrimitiveKind::INT) {
        // just use ifge to skip the negation if already positive
        auto rawInstruction = std::make_shared<IRRawInstruction>("ifge " + endLabel);
        currentFunction->instructions.push_back(rawInstruction);
        auto rawInstructionNegate = std::make_shared<IRRawInstruction>("ineg");
        currentFunction->instructions.push_back(rawInstructionNegate);
      } else if (typeKind == PrimitiveType::PrimitiveKind::FLOAT) {
        // use fcmpl with 0, then ifgt to skip the negation if already positive
        auto pushZeroInstruction = std::make_shared<IRRawInstruction>("fconst 0");
        currentFunction->instructions.push_back(pushZeroInstruction);
        auto rawInstructionFcmpl = std::make_shared<IRRawInstruction>("fcmpl");
        currentFunction->instructions.push_back(rawInstructionFcmpl);
        auto rawInstruction = std::make_shared<IRRawInstruction>("ifge " + endLabel);
        currentFunction->instructions.push_back(rawInstruction);
        auto rawInstructionNegate = std::make_shared<IRRawInstruction>("fneg");
        currentFunction->instructions.push_back(rawInstructionNegate);
      } else {
        throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
      }
      auto rawInstructionEndLabel = std::make_shared<IRRawInstruction>(endLabel + ":");
      currentFunction->instructions.push_back(rawInstructionEndLabel);
    } else if (ctx->MINUS_OP()) {
      // always negate
      if (typeKind == PrimitiveType::PrimitiveKind::INT) {
        auto rawInstructionNegate = std::make_shared<IRRawInstruction>("ineg");
        currentFunction->instructions.push_back(rawInstructionNegate);
      } else if (typeKind == PrimitiveType::PrimitiveKind::FLOAT) {
        auto rawInstructionNegate = std::make_shared<IRRawInstruction>("fneg");
        currentFunction->instructions.push_back(rawInstructionNegate);
      } else {
        throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
      }
    } else if (ctx->NOT_OP()) {
      // only works on bools, already checked, use xor to flip
      auto rawInstructionPushTrue = std::make_shared<IRRawInstruction>("iconst 1");
      currentFunction->instructions.push_back(rawInstructionPushTrue);
      auto rawInstructionXor = std::make_shared<IRRawInstruction>("ixor");
      currentFunction->instructions.push_back(rawInstructionXor);
    } else if (ctx->BITWISE_NOT_OP()) {
      // only works on ints, already checked
      if (typeKind == PrimitiveType::PrimitiveKind::INT) {
        auto rawInstructionNot = std::make_shared<IRRawInstruction>("ldc -1");
        currentFunction->instructions.push_back(rawInstructionNot);
        auto rawInstructionXor = std::make_shared<IRRawInstruction>("ixor");
        currentFunction->instructions.push_back(rawInstructionXor);
      } else {
        throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
      }
    } else if (ctx->INCREMENT_OP() || ctx->DECREMENT_OP()) {
      if (typeKind == PrimitiveType::PrimitiveKind::INT || typeKind == PrimitiveType::PrimitiveKind::FLOAT) {
        std::string prefix = typeKind == PrimitiveType::PrimitiveKind::INT ? "i" : "f";
        auto rawInstructionPushOne = std::make_shared<IRRawInstruction>(prefix + "const 1");
        currentFunction->instructions.push_back(rawInstructionPushOne);
        if (ctx->INCREMENT_OP()) {
          auto rawInstructionAdd = std::make_shared<IRRawInstruction>(prefix + "add");
          currentFunction->instructions.push_back(rawInstructionAdd);
        } else if (ctx->DECREMENT_OP()) {
          auto rawInstructionSubtract = std::make_shared<IRRawInstruction>(prefix + "sub");
          currentFunction->instructions.push_back(rawInstructionSubtract);
        }
        // store the result back in the variable (duplicate the value on the stack)
        auto rawInstructionDup = std::make_shared<IRRawInstruction>("dup");
        currentFunction->instructions.push_back(rawInstructionDup);
        auto scope = getCurrentScope(ctx);
        // kinda janky, but type checking should guarantee this is an identifier for now...
        std::string identifier = ctx->expression()->getText();
        auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));
        auto rawInstructionStore = std::make_shared<IRRawInstruction>(getStoreInstruction(primitiveType) + " " +
                                                                      std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(rawInstructionStore);
      } else {
        throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
      }
    }
  }
}

void BytecodeIRGeneratorListener::exitUnary_statement(cgullParser::Unary_statementContext* ctx) {
  // remove the evaluation of the expression, it won't be used
  auto popInst = std::make_shared<IRRawInstruction>("pop");
  currentFunction->instructions.push_back(popInst);
}

// needs more work in HW5 for sure
void BytecodeIRGeneratorListener::exitPostfix_expression(cgullParser::Postfix_expressionContext* ctx) {
  if (ctx->IDENTIFIER()) {
    auto scope = getCurrentScope(ctx);
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol) {
      auto type = varSymbol->dataType;
      auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
      auto typeKind = primitiveType->getPrimitiveKind();
      if (primitiveType) {
        // load the value in question (it's an identifier, so its not on the stack)
        if (varSymbol->isStructMember) {
          auto loadThis = std::make_shared<IRRawInstruction>("aload 0");
          currentFunction->instructions.push_back(loadThis);
          auto getField =
              std::make_shared<IRRawInstruction>("getfield " + varSymbol->parentStructType->name + "." +
                                                 varSymbol->name + " " + BytecodeCompiler::typeToJVMType(type));
          currentFunction->instructions.push_back(getField);
        } else {
          auto loadInstruction = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                                    std::to_string(varSymbol->localIndex));
          currentFunction->instructions.push_back(loadInstruction);
        }

        // duplicate the value on the stack
        auto dupInst = std::make_shared<IRRawInstruction>("dup");
        currentFunction->instructions.push_back(dupInst);

        // increment or decrement the value and store it back in the variable
        // then the previous value is left on the stack to be consumed
        if (typeKind == PrimitiveType::PrimitiveKind::INT) {
          auto pushOneInst = std::make_shared<IRRawInstruction>("iconst 1");
          currentFunction->instructions.push_back(pushOneInst);
          if (ctx->INCREMENT_OP()) {
            auto addInst = std::make_shared<IRRawInstruction>("iadd");
            currentFunction->instructions.push_back(addInst);
          } else if (ctx->DECREMENT_OP()) {
            auto subInst = std::make_shared<IRRawInstruction>("isub");
            currentFunction->instructions.push_back(subInst);
          }
        } else if (typeKind == PrimitiveType::PrimitiveKind::FLOAT) {
          auto pushOneInst = std::make_shared<IRRawInstruction>("fconst 1");
          currentFunction->instructions.push_back(pushOneInst);
          if (ctx->INCREMENT_OP()) {
            auto addInst = std::make_shared<IRRawInstruction>("fadd");
            currentFunction->instructions.push_back(addInst);
          } else if (ctx->DECREMENT_OP()) {
            auto subInst = std::make_shared<IRRawInstruction>("fsub");
            currentFunction->instructions.push_back(subInst);
          }
        } else {
          throw std::runtime_error("Unsupported variable type for postfix expression: " + primitiveType->toString());
        }

        // store the new value back in the variable
        if (varSymbol->isStructMember) {
          auto putField =
              std::make_shared<IRRawInstruction>("putfield " + varSymbol->parentStructType->name + "." +
                                                 varSymbol->name + " " + BytecodeCompiler::typeToJVMType(type));
          currentFunction->instructions.push_back(putField);
        } else {
          auto storeInst = std::make_shared<IRRawInstruction>(getStoreInstruction(primitiveType) + " " +
                                                              std::to_string(varSymbol->localIndex));
          currentFunction->instructions.push_back(storeInst);
        }
      }
    }
  }
}

std::string BytecodeIRGeneratorListener::getLoadInstruction(const std::shared_ptr<PrimitiveType>& primitiveType) {
  switch (primitiveType->getPrimitiveKind()) {
  case PrimitiveType::PrimitiveKind::INT:
    return "iload";
  case PrimitiveType::PrimitiveKind::FLOAT:
    return "fload";
  case PrimitiveType::PrimitiveKind::BOOLEAN:
    return "iload";
  case PrimitiveType::PrimitiveKind::STRING:
    return "aload";
  default:
    throw std::runtime_error("Unsupported variable type for loading: " + primitiveType->toString());
  }
}

std::string BytecodeIRGeneratorListener::getStoreInstruction(const std::shared_ptr<PrimitiveType>& primitiveType) {
  switch (primitiveType->getPrimitiveKind()) {
  case PrimitiveType::PrimitiveKind::INT:
    return "istore";
  case PrimitiveType::PrimitiveKind::FLOAT:
    return "fstore";
  case PrimitiveType::PrimitiveKind::BOOLEAN:
    return "istore";
  case PrimitiveType::PrimitiveKind::STRING:
    return "astore";
  default:
    throw std::runtime_error("Unsupported variable type for storing: " + primitiveType->toString());
  }
}

void BytecodeIRGeneratorListener::enterIf_statement(cgullParser::If_statementContext* ctx) {
  // essentially, collect all the labels and have the listener place them as it traverses
  // this feels not ideal at all but it works without introducing visitors
  std::string endIfLabel = generateLabel();
  std::vector<std::string> branchLabels;

  branchLabels.push_back(generateLabel());
  for (size_t i = 0; i < ctx->ELSE_IF().size(); ++i) {
    branchLabels.push_back(generateLabel());
  }
  if (ctx->ELSE()) {
    branchLabels.push_back(generateLabel());
  }

  IfLabels labels;
  labels.endIfLabel = endIfLabel;
  labels.conditionLabels = branchLabels;
  ifLabelsMap[ctx] = labels;
}

void BytecodeIRGeneratorListener::enterIf_expression(cgullParser::If_expressionContext* ctx) {
  std::string endIfLabel = generateLabel();
  std::vector<std::string> branchLabels = {generateLabel()};

  IfLabels labels;
  labels.endIfLabel = endIfLabel;
  labels.conditionLabels = branchLabels;
  ifExpressionLabelsMap[ctx] = labels;
}

void BytecodeIRGeneratorListener::exitBranch_block(cgullParser::Branch_blockContext* ctx) {
  auto parentCtx = ctx->parent;
  if (!parentCtx)
    return;

  // handle loop labels first before break labels

  auto whileStmt = dynamic_cast<cgullParser::While_statementContext*>(parentCtx);
  if (whileStmt) {
    auto it = whileLabelsMap.find(whileStmt);
    if (it != whileLabelsMap.end()) {
      auto& labels = it->second;
      // jump to the top of the loop
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.startLabel);
      currentFunction->instructions.push_back(jumpInst);
      // place the label for the end of the loop
      auto endLabelInst = std::make_shared<IRRawInstruction>(labels.endLabel + ":");
      currentFunction->instructions.push_back(endLabelInst);
      whileLabelsMap.erase(whileStmt);
    }
  }

  auto infiniteLoopStmt = dynamic_cast<cgullParser::Infinite_loop_statementContext*>(parentCtx);
  if (infiniteLoopStmt) {
    auto it = infiniteLoopLabelsMap.find(infiniteLoopStmt);
    if (it != infiniteLoopLabelsMap.end()) {
      auto& labels = it->second;
      // jump to the top of the loop
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.startLabel);
      currentFunction->instructions.push_back(jumpInst);
      // breaks already act as our end label for infinite loops
    }
  }

  auto forStmt = dynamic_cast<cgullParser::For_statementContext*>(parentCtx);
  if (forStmt) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // jump to the update expr at the end of the loop
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.updateLabel);
      currentFunction->instructions.push_back(jumpInst);
      // place the label for the end of the loop
      auto endLabelInst = std::make_shared<IRRawInstruction>(labels.endLabel + ":");
      currentFunction->instructions.push_back(endLabelInst);
    }
  }

  // untils don't need to do anything at the end of a branch block (handled by the expression following it)

  // handle break labels

  if (dynamic_cast<cgullParser::Until_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::While_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::For_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::Infinite_loop_statementContext*>(parentCtx)) {
    if (!breakLabels.empty()) {
      auto breakLabel = breakLabels.top();
      breakLabels.pop();

      auto labelInst = std::make_shared<IRRawInstruction>(breakLabel + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  } else if (auto ifStmt = dynamic_cast<cgullParser::If_statementContext*>(parentCtx)) {
    // branch is part of an if statement, handle jumps
    auto it = ifLabelsMap.find(ifStmt);
    if (it != ifLabelsMap.end()) {
      auto& labels = it->second;

      // after a branch block, jump to the end of the if statement
      auto jumpInst = std::make_shared<IRRawInstruction>("goto " + labels.endIfLabel);
      currentFunction->instructions.push_back(jumpInst);

      // find which branch block this is
      size_t branchIndex = 0;
      for (size_t i = 0; i < ifStmt->branch_block().size(); ++i) {
        if (ifStmt->branch_block(i) == ctx) {
          branchIndex = i;
          break;
        }
      }

      // add the label for the next branch if this is not the last branch
      if (branchIndex + 1 < labels.conditionLabels.size()) {
        auto labelInst = std::make_shared<IRRawInstruction>(labels.conditionLabels[branchIndex + 1] + ":");
        currentFunction->instructions.push_back(labelInst);
      }

      // add end label if this *is* the last branch block
      if (branchIndex == ifStmt->branch_block().size() - 1) {
        auto endLabelInst = std::make_shared<IRRawInstruction>(labels.endIfLabel + ":");
        currentFunction->instructions.push_back(endLabelInst);
        ifLabelsMap.erase(ifStmt);
      }
    }
  }
}

void BytecodeIRGeneratorListener::enterBranch_block(cgullParser::Branch_blockContext* ctx) {
  // if this branch block is part of a loop, push a new break label
  auto parentCtx = ctx->parent;
  if (!parentCtx)
    return;

  if (dynamic_cast<cgullParser::Loop_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::Until_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::While_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::For_statementContext*>(parentCtx) ||
      dynamic_cast<cgullParser::Infinite_loop_statementContext*>(parentCtx)) {
    breakLabels.push(generateLabel());
  } else if (auto ifStmt = dynamic_cast<cgullParser::If_statementContext*>(parentCtx)) {
    size_t branchIndex = 0;
    for (size_t i = 0; i < ifStmt->branch_block().size(); ++i) {
      if (ifStmt->branch_block(i) == ctx) {
        branchIndex = i;
        break;
      }
    }

    // if this is the first branch block, add its label
    if (branchIndex == 0) {
      auto it = ifLabelsMap.find(ifStmt);
      if (it != ifLabelsMap.end()) {
        auto& labels = it->second;
        auto labelInst = std::make_shared<IRRawInstruction>(labels.conditionLabels[0] + ":");
        currentFunction->instructions.push_back(labelInst);
      }
    }
  }

  auto forStmt = dynamic_cast<cgullParser::For_statementContext*>(parentCtx);
  if (forStmt) {
    auto it = forLabelsMap.find(forStmt);
    if (it != forLabelsMap.end()) {
      auto& labels = it->second;
      // place label for the start of the block
      auto labelInst = std::make_shared<IRRawInstruction>(labels.startLabel + ":");
      currentFunction->instructions.push_back(labelInst);
    }
  }
}

void BytecodeIRGeneratorListener::exitBreak_statement(cgullParser::Break_statementContext* ctx) {
  // if there's a break label on the stack, add a jump to it
  if (!breakLabels.empty()) {
    auto breakLabel = breakLabels.top();
    auto jumpInst = std::make_shared<IRRawInstruction>("goto " + breakLabel);
    currentFunction->instructions.push_back(jumpInst);
  } else {
    // this should already be type checked
    throw std::runtime_error("Break statement outside of loop");
  }
}

void BytecodeIRGeneratorListener::enterWhile_statement(cgullParser::While_statementContext* ctx) {
  auto startLabel = generateLabel();
  auto endLabel = generateLabel();

  SimpleLoopLabels labels;
  labels.startLabel = startLabel;
  labels.endLabel = endLabel;
  whileLabelsMap[ctx] = labels;

  // place start label, since a while loop just starts at the top
  auto labelInst = std::make_shared<IRRawInstruction>(startLabel + ":");
  currentFunction->instructions.push_back(labelInst);
}

void BytecodeIRGeneratorListener::enterInfinite_loop_statement(cgullParser::Infinite_loop_statementContext* ctx) {
  auto startLabel = generateLabel();

  SimpleLoopLabels labels;
  labels.startLabel = startLabel;
  // end label unused, breaks already have an end label
  infiniteLoopLabelsMap[ctx] = labels;

  // place start label, since an infinite loop just starts at the top
  auto labelInst = std::make_shared<IRRawInstruction>(startLabel + ":");
  currentFunction->instructions.push_back(labelInst);
}

void BytecodeIRGeneratorListener::enterUntil_statement(cgullParser::Until_statementContext* ctx) {
  auto startLabel = generateLabel();

  SimpleLoopLabels labels;
  labels.startLabel = startLabel;
  // end label unused, until fallsthrough if true
  untilLabelsMap[ctx] = labels;

  // place start label, since an until loop just starts at the top
  auto labelInst = std::make_shared<IRRawInstruction>(startLabel + ":");
  currentFunction->instructions.push_back(labelInst);
}

void BytecodeIRGeneratorListener::enterFor_statement(cgullParser::For_statementContext* ctx) {
  auto startLabel = generateLabel();
  auto endLabel = generateLabel();
  auto conditionLabel = generateLabel();
  auto updateLabel = generateLabel();

  ForLoopLabels labels;
  labels.startLabel = startLabel;
  labels.endLabel = endLabel;
  labels.conditionLabel = conditionLabel;
  labels.updateLabel = updateLabel;
  forLabelsMap[ctx] = labels;
}

void BytecodeIRGeneratorListener::handleLogicalExpression(cgullParser::Base_expressionContext* ctx) {
  bool isAndOp = ctx->AND_OP() != nullptr;
  bool isOrOp = ctx->OR_OP() != nullptr;

  if (!isAndOp && !isOrOp)
    return;

  ExpressionLabels labels;
  labels.fallthroughLabel = generateLabel();
  labels.exitLabel = generateLabel();
  labels.isAndOperator = isAndOp;
  expressionLabelsMap[ctx] = labels;

  // set parent relationship for the operands
  auto leftExpr = ctx->base_expression(0);
  auto rightExpr = ctx->base_expression(1);

  if (leftExpr) {
    parentExpressionMap[leftExpr] = ctx;
  }

  if (rightExpr) {
    parentExpressionMap[rightExpr] = ctx;
  }
}

void BytecodeIRGeneratorListener::exitCast_expression(cgullParser::Cast_expressionContext* ctx) {
  auto castType = std::dynamic_pointer_cast<PrimitiveType>(
      TypeCheckingListener::resolvePrimitiveType(ctx->primitive_type()->getText()));
  std::shared_ptr<Type> currentType;

  // if its an expression, it will already be resolved on the stack
  // if its an identifier, we need to load the value from the variable
  if (ctx->IDENTIFIER()) {
    // load the value from the variable
    auto scope = getCurrentScope(ctx);
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(ctx->IDENTIFIER()->getText()));
    currentType = varSymbol->dataType;
    if (auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(currentType)) {
      auto loadInst = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                         std::to_string(varSymbol->localIndex));
      currentFunction->instructions.push_back(loadInst);
    } else if (auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(currentType)) {
      if (varSymbol->isStructMember) {
        auto aloadInst = std::make_shared<IRRawInstruction>("aload 0");
        currentFunction->instructions.push_back(aloadInst);
        auto getFieldInst =
            std::make_shared<IRRawInstruction>("getfield " + varSymbol->parentStructType->name + "." + varSymbol->name +
                                               " " + BytecodeCompiler::typeToJVMType(varSymbol->dataType));
        currentFunction->instructions.push_back(getFieldInst);
      } else {
        auto loadInst = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(loadInst);
      }
    } else {
      // handle pointer type
      auto loadInst = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
      currentFunction->instructions.push_back(loadInst);
    }
  } else {
    currentType = expressionTypes[ctx->expression()];
  }

  // pointer to int conversion
  if (auto pointerType = std::dynamic_pointer_cast<PointerType>(currentType)) {
    if (castType && castType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::INT) {
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/System.identityHashCode(java/lang/Object)I");
      currentFunction->instructions.push_back(rawInstruction);
      return;
    }
  }

  // user defined type conversions
  if (auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(currentType)) {
    if (castType && castType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
      auto rawInstruction = std::make_shared<IRRawInstruction>(
          "invokevirtual " + userDefinedType->getTypeSymbol()->name + ".$toString_() java/lang/String");
      currentFunction->instructions.push_back(rawInstruction);
      return;
    }
  }

  // primitive conversions
  if (auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(currentType)) {
    convertPrimitiveToPrimitive(primitiveType, castType);
  } else {
    throw std::runtime_error("Unsupported cast from " + currentType->toString() + " to " + castType->toString());
  }
}

void BytecodeIRGeneratorListener::convertPrimitiveToPrimitive(const std::shared_ptr<PrimitiveType>& fromType,
                                                              const std::shared_ptr<PrimitiveType>& toType) {
  if (fromType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::INT ||
      fromType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::BOOLEAN) {
    switch (toType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::FLOAT: {
      auto rawInstruction = std::make_shared<IRRawInstruction>("i2f");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::STRING: {
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/Integer.toString (I)java/lang/String");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::INT:
    case PrimitiveType::PrimitiveKind::BOOLEAN:
      // string conversion is already handled elsewhere
      // booleans are the same as ints in jvm
      break;
    default:
      throw std::runtime_error("Unsupported conversion from int to " + toType->toString());
    }
  } else if (fromType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::FLOAT) {
    switch (toType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::BOOLEAN:
    case PrimitiveType::PrimitiveKind::INT: {
      auto rawInstruction = std::make_shared<IRRawInstruction>("f2i");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::STRING: {
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/Float.toString (F)java/lang/String");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::FLOAT:
      break;
    default:
      throw std::runtime_error("Unsupported conversion from float to " + toType->toString());
    }
  } else if (fromType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
    switch (toType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::INT: {
      // call Int.parseInt
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/Integer.parseInt (java/lang/String)I");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::FLOAT: {
      // call Float.parseFloat
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/Float.parseFloat (java/lang/String)F");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::BOOLEAN: {
      // call Boolean.parseBoolean
      auto rawInstruction =
          std::make_shared<IRRawInstruction>("invokestatic java/lang/Boolean.parseBoolean (java/lang/String)Z");
      currentFunction->instructions.push_back(rawInstruction);
      break;
    }
    case PrimitiveType::PrimitiveKind::STRING:
      break;
    default:
      throw std::runtime_error("Unsupported conversion from string to " + toType->toString());
    }
  }
}

void BytecodeIRGeneratorListener::enterAllocate_primitive(cgullParser::Allocate_primitiveContext* ctx) {
  // place creation of the object first, as the expression will be a parameter
  if (ctx->primitive_type()) {
    std::string typeName = ctx->primitive_type()->getText();
    auto baseType = TypeCheckingListener::resolvePrimitiveType(typeName);
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(baseType);
    auto newInst = std::make_shared<IRRawInstruction>(
        "new " + PrimitiveWrapperGenerator::getClassName(primitiveType->getPrimitiveKind()));
    currentFunction->instructions.push_back(newInst);

    auto dupInst = std::make_shared<IRRawInstruction>("dup");
    currentFunction->instructions.push_back(dupInst);
  }
}

void BytecodeIRGeneratorListener::exitAllocate_primitive(cgullParser::Allocate_primitiveContext* ctx) {
  if (ctx->primitive_type()) {
    std::string typeName = ctx->primitive_type()->getText();
    auto baseType = TypeCheckingListener::resolvePrimitiveType(typeName);
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(baseType);

    if (primitiveType) {
      std::string refClassName = PrimitiveWrapperGenerator::getClassName(primitiveType->getPrimitiveKind());
      std::string paramType = BytecodeCompiler::typeToJVMType(primitiveType);

      auto initInst =
          std::make_shared<IRRawInstruction>("invokespecial " + refClassName + ".<init>(" + paramType + ")V");
      currentFunction->instructions.push_back(initInst);
    } else {
      throw std::runtime_error("Invalid primitive type in allocation: " + typeName);
    }
  }
}

void BytecodeIRGeneratorListener::exitDereferenceable(cgullParser::DereferenceableContext* ctx) {
  // figure out the type of the dereferenceable
  auto scope = getCurrentScope(ctx);
  // type checked, so we can assume that this deref SHOULD become this type
  auto derefType = expressionTypes[dynamic_cast<antlr4::ParserRuleContext*>(ctx->parent)];
  if (!derefType) {
    return;
  }
  // identifiers need their object loaded onto the stack
  if (ctx->IDENTIFIER()) {
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(ctx->IDENTIFIER()->getText()));
    auto loadInst = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
    currentFunction->instructions.push_back(loadInst);
  }
  if (!dereferenceAssignment) {
    generateDereference(dynamic_cast<antlr4::ParserRuleContext*>(ctx->parent));
  }
  dereferenceAssignment = false;
}

void BytecodeIRGeneratorListener::enterDereference_expression(cgullParser::Dereference_expressionContext* ctx) {
  auto parent = ctx->parent;
  if (parent && dynamic_cast<cgullParser::Assignment_statementContext*>(parent)) {
    dereferenceAssignment = true;
  }
}

void BytecodeIRGeneratorListener::exitAllocate_array(cgullParser::Allocate_arrayContext* ctx) {
  auto arrayType = std::dynamic_pointer_cast<ArrayType>(expressionTypes[ctx]);
  if (!arrayType) {
    throw std::runtime_error("Invalid array type in allocation: " + ctx->type()->getText());
  }
  auto baseType = arrayType->getElementType();

  if (ctx->expression().size() > 0) {
    // all dimension sizes are on the stack
    std::string typeString = BytecodeCompiler::typeToJVMType(arrayType);
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(baseType);
    auto newInst = std::make_shared<IRRawInstruction>("multianewarray " + typeString + " " +
                                                      std::to_string(ctx->expression().size()));
    currentFunction->instructions.push_back(newInst);
  }
}

void BytecodeIRGeneratorListener::enterArray_expression(cgullParser::Array_expressionContext* ctx) {
  auto arrayType = std::dynamic_pointer_cast<ArrayType>(expressionTypes[ctx]);
  if (!arrayType) {
    throw std::runtime_error("Invalid array type in allocation: " + ctx->getText());
  }
  // determine the array index counts from size of expression list
  size_t indexCounts = ctx->expression_list()->expression().size();
  auto sizeInstruction = std::make_shared<IRRawInstruction>("ldc " + std::to_string(indexCounts));
  currentFunction->instructions.push_back(sizeInstruction);
  std::string typeString = BytecodeCompiler::typeToJVMType(arrayType);
  // the rest of the dimensions are initialized by the array expression
  auto newInst = std::make_shared<IRRawInstruction>("multianewarray " + typeString + " 1");
  currentFunction->instructions.push_back(newInst);
}

void BytecodeIRGeneratorListener::enterIndexable(cgullParser::IndexableContext* ctx) {
  // load identifier based on resolved scope and type
  auto scope = getCurrentScope(ctx);
  std::string identifier = ctx->IDENTIFIER()->getText();
  auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

  if (varSymbol) {
    auto type = varSymbol->dataType;
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
    auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
    auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);

    if (varSymbol->isStructMember) {
      auto loadThis = std::make_shared<IRRawInstruction>("aload 0");
      currentFunction->instructions.push_back(loadThis);
      auto getField = std::make_shared<IRRawInstruction>("getfield " + varSymbol->parentStructType->name + "." +
                                                         identifier + " " + BytecodeCompiler::typeToJVMType(type));
      currentFunction->instructions.push_back(getField);
    } else if (pointerType || arrayType) {
      auto loadInstruction = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
      currentFunction->instructions.push_back(loadInstruction);
    }

    if (primitiveType) {
      // load value from the local variable onto the stack
      auto loadInst = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                         std::to_string(varSymbol->localIndex));
      currentFunction->instructions.push_back(loadInst);
    }
  }
}

void BytecodeIRGeneratorListener::enterStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  // create a new class for the struct
  auto structName = ctx->IDENTIFIER()->getText();
  auto structClass = std::make_shared<IRClass>();
  structClass->name = structName;
  currentClassStack.push(structClass);
  classes.push_back(structClass);
}

void BytecodeIRGeneratorListener::exitStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  // generate the constructor method with all the public fields
  auto structClass = currentClassStack.top();
  auto constructor = constructorMap[structClass->name];
  if (!constructor) {
    throw std::runtime_error("Constructor not found for struct: " + structClass->name);
  }
  constructor->name = "<init>";
  std::vector<std::shared_ptr<VariableSymbol>> parameters;
  for (auto variable : structClass->variables) {
    if (!variable->isPrivate) {
      parameters.push_back(variable);
    }
  }
  constructor->parameters = parameters;

  // initialize the object
  auto initInst = std::make_shared<IRRawInstruction>("aload 0");
  constructor->instructions.push_back(initInst);
  auto invokeInst = std::make_shared<IRRawInstruction>("invokespecial java/lang/Object.<init>()V");
  constructor->instructions.push_back(invokeInst);

  // create the instructions to putfield for each variable
  for (int i = 0; i < structClass->variables.size(); i++) {
    auto variable = std::dynamic_pointer_cast<VariableSymbol>(structClass->variables[i]);
    if (!variable->isPrivate) {
      // load in the object
      auto thisInst = std::make_shared<IRRawInstruction>("aload 0");
      constructor->instructions.push_back(thisInst);
      // load based on type
      if (variable->dataType->getKind() == Type::TypeKind::PRIMITIVE) {
        auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(variable->dataType);
        auto loadInst =
            std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " + std::to_string(i + 1));
        constructor->instructions.push_back(loadInst);
      } else {
        auto loadInst = std::make_shared<IRRawInstruction>("aload " + std::to_string(i + 1));
        constructor->instructions.push_back(loadInst);
      }
      // putfield
      auto putFieldInst = std::make_shared<IRRawInstruction>("putfield " + structClass->name + "." + variable->name +
                                                             " " + BytecodeCompiler::typeToJVMType(variable->dataType));
      constructor->instructions.push_back(putFieldInst);
    }
  }
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>("return"));
  // return void
  constructor->returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));
  structClass->methods.push_back(constructor);

  if (!currentClassStack.empty()) {
    currentClassStack.pop();
  }
}

void BytecodeIRGeneratorListener::enterField_access(cgullParser::Field_accessContext* ctx) {
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
}

void BytecodeIRGeneratorListener::exitField_access(cgullParser::Field_accessContext* ctx) { lastFieldType = nullptr; }

void BytecodeIRGeneratorListener::enterField(cgullParser::FieldContext* ctx) {
  // generate getfield for index_expressions on structs
  if (ctx->index_expression() && lastFieldType) {
    auto structSymbol = std::dynamic_pointer_cast<UserDefinedType>(lastFieldType)->getTypeSymbol();
    auto fieldSymbol = std::dynamic_pointer_cast<VariableSymbol>(
        structSymbol->scope->resolve(ctx->index_expression()->indexable()->IDENTIFIER()->getText()));
    if (!fieldSymbol) {
      throw std::runtime_error("Field not found: " + ctx->index_expression()->indexable()->IDENTIFIER()->getText());
    }
    auto getFieldInst =
        std::make_shared<IRRawInstruction>("getfield " + structSymbol->name + "." + fieldSymbol->name + " " +
                                           BytecodeCompiler::typeToJVMType(fieldSymbol->dataType));
    currentFunction->instructions.push_back(getFieldInst);
  }
}

void BytecodeIRGeneratorListener::exitField(cgullParser::FieldContext* ctx) {
  auto parent = dynamic_cast<cgullParser::Field_accessContext*>(ctx->parent);
  // if first item, just load the identifier
  if (!lastFieldType) {
    if (ctx->IDENTIFIER()) {
      auto scope = getCurrentScope(ctx);
      std::string identifier = ctx->IDENTIFIER()->getText();
      auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));
      // if its a member access, we need to get field instead of aload
      if (varSymbol->isStructMember) {
        auto aloadInst = std::make_shared<IRRawInstruction>("aload 0");
        currentFunction->instructions.push_back(aloadInst);
        auto getFieldInst =
            std::make_shared<IRRawInstruction>("getfield " + varSymbol->parentStructType->name + "." + identifier +
                                               " " + BytecodeCompiler::typeToJVMType(varSymbol->dataType));
        currentFunction->instructions.push_back(getFieldInst);
      } else {
        auto loadInstruction = std::make_shared<IRRawInstruction>("aload " + std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(loadInstruction);
      }
      lastFieldType = varSymbol->dataType;
    } else if (ctx->index_expression()) {
      lastFieldType = expressionTypes[ctx->index_expression()];
    } else if (ctx->function_call()) {
      lastFieldType = resolvedMethodSymbols[ctx->function_call()]->returnTypes[0];
    } else {
      lastFieldType = expressionTypes[ctx->expression()];
    }
    // if this context should be dereferenced, do so
    if (isDereferenceContexts[ctx]) {
      generateDereference(ctx);
    }
    return;
  }
  // take appropriate action based on last field type loaded...
  auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(lastFieldType);
  auto assignmentContext = dynamic_cast<cgullParser::Assignment_statementContext*>(ctx->parent->parent->parent);
  auto parentFieldAccess = dynamic_cast<cgullParser::Field_accessContext*>(ctx->parent);
  if (assignmentContext && parentFieldAccess) {
    auto lastField = parentFieldAccess->field(parentFieldAccess->field().size() - 1);
    if (ctx == lastField) {
      return;
    }
  }
  if (userDefinedType) {
    // already type checked, just load the field
    auto structSymbol = userDefinedType->getTypeSymbol();
    if (ctx->function_call()) {
      // method call handled in exitFunction_call
      lastFieldType = resolvedMethodSymbols[ctx->function_call()]->returnTypes[0];
    } else if (ctx->index_expression()) {
      auto lastField = parentFieldAccess->field(parentFieldAccess->field().size() - 1);
      if (!(ctx == lastField)) {
        auto loadInst = std::make_shared<IRRawInstruction>(getArrayOperationInstruction(lastFieldType, false));
        currentFunction->instructions.push_back(loadInst);
      }
      lastFieldType = expressionTypes[ctx->index_expression()];
    } else {
      // handle field access
      auto fieldSymbol =
          std::dynamic_pointer_cast<VariableSymbol>(structSymbol->scope->resolve(ctx->IDENTIFIER()->getText()));
      if (!fieldSymbol) {
        throw std::runtime_error("Field not found: " + ctx->IDENTIFIER()->getText());
      }
      auto getFieldInst =
          std::make_shared<IRRawInstruction>("getfield " + structSymbol->name + "." + fieldSymbol->name + " " +
                                             BytecodeCompiler::typeToJVMType(fieldSymbol->dataType));
      currentFunction->instructions.push_back(getFieldInst);
      lastFieldType = fieldSymbol->dataType;
    }
    if (isDereferenceContexts[ctx]) {
      generateDereference(ctx);
    }
  } else {
    throw std::runtime_error("Cannot access field of non-struct: " + lastFieldType->toString());
  }
}

std::string BytecodeIRGeneratorListener::getArrayOperationInstruction(const std::shared_ptr<Type>& type, bool isStore) {
  auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);
  auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
  auto userDefinedType = std::dynamic_pointer_cast<UserDefinedType>(type);

  if (arrayType || pointerType || userDefinedType) {
    return isStore ? "aastore" : "aaload";
  } else if (primitiveType) {
    auto primitiveKind = primitiveType->getPrimitiveKind();
    std::string prefix = primitiveKind == PrimitiveType::PrimitiveKind::INT         ? "i"
                         : (primitiveKind == PrimitiveType::PrimitiveKind::FLOAT)   ? "f"
                         : (primitiveKind == PrimitiveType::PrimitiveKind::BOOLEAN) ? "b"
                                                                                    : "a";
    return prefix + (isStore ? "astore" : "aload");
  }
  throw std::runtime_error("Unsupported type for array operation: " + type->toString());
}

void BytecodeIRGeneratorListener::generateDereference(antlr4::ParserRuleContext* ctx) {
  auto derefType = expressionTypes[ctx];
  if (derefType->getKind() == Type::TypeKind::PRIMITIVE) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(derefType);
    auto irClass = primitiveWrappers[primitiveType->getPrimitiveKind()];
    if (!irClass) {
      throw std::runtime_error("Primitive type " + primitiveType->toString() + " has no wrapper class");
    }
    // run the getter method
    auto valueMethod = irClass->getMethod("getValue");
    if (!valueMethod) {
      throw std::runtime_error("Primitive type " + primitiveType->toString() + " has no getValue method");
    }
    std::string retType = BytecodeCompiler::typeToJVMType(derefType);
    auto invokeInst = std::make_shared<IRRawInstruction>("invokevirtual " + irClass->name + "." +
                                                         valueMethod->getMangledName() + "() " + retType);
    currentFunction->instructions.push_back(invokeInst);
  } else {
    throw std::runtime_error("Invalid dereferenceable: " + ctx->getText());
  }
}
