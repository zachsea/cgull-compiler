#include "bytecode_ir_generator_listener.h"

BytecodeIRGeneratorListener::BytecodeIRGeneratorListener(
    ErrorReporter& errorReporter, std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes,
    std::unordered_set<antlr4::ParserRuleContext*>& expectingStringConversion)
    : errorReporter(errorReporter), scopes(scopes), expressionTypes(expressionTypes),
      expectingStringConversion(expectingStringConversion) {}

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
    if (primitiveType) {
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
      case PrimitiveType::PrimitiveKind::LONG: {
        // convert long to string
        auto rawInstruction =
            std::make_shared<IRRawInstruction>("invokestatic java/lang/Long.toString (J)java/lang/String");
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
    // replace with error reporter
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

    auto functionSymbol = scope->resolve(ctx->IDENTIFIER()->getText());
    currentFunction = std::dynamic_pointer_cast<FunctionSymbol>(functionSymbol);
    auto currentClass = currentClassStack.top();
    currentClass->methods.push_back(currentFunction);
  } else {
    // replace with error reporter
    throw std::runtime_error("No scope found for function definition context");
  }
}

void BytecodeIRGeneratorListener::exitFunction_definition(cgullParser::Function_definitionContext* ctx) {
  if (currentFunction) {
    currentFunction = nullptr;
  }
}

void BytecodeIRGeneratorListener::enterFunction_call(cgullParser::Function_callContext* ctx) {
  // for cases where methods are called on objects, to be filled later HW5
  auto scope = getCurrentScope(ctx);
  if (scope) {
    auto functionSymbol = scope->resolve(ctx->IDENTIFIER()->getText());
    auto calledFunction = std::dynamic_pointer_cast<FunctionSymbol>(functionSymbol);
    if (calledFunction->name == "println") {
      // special case for println, we need to add a raw instruction
      auto rawInstruction = std::make_shared<IRRawInstruction>("getstatic java/lang/System.out java/io/PrintStream");
      currentFunction->instructions.push_back(rawInstruction);
    }
  } else {
    // replace with error reporter
    throw std::runtime_error("No scope found for function call context");
  }
}

void BytecodeIRGeneratorListener::exitFunction_call(cgullParser::Function_callContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    // the expressions in the parameters are now evaluated and on the stack, so put a function call instruction
    // on the stack
    auto functionSymbol = scope->resolve(ctx->IDENTIFIER()->getText());
    auto calledFunction = std::dynamic_pointer_cast<FunctionSymbol>(functionSymbol);
    // program will jump and handle it, for generating IR we are done here, just add the call instruction
    auto callInstruction = std::make_shared<IRCallInstruction>(calledFunction);
    currentFunction->instructions.push_back(callInstruction);
  } else {
    // replace with error reporter
    throw std::runtime_error("No scope found for function call context");
  }
}

void BytecodeIRGeneratorListener::exitExpression(cgullParser::ExpressionContext* ctx) { generateStringConversion(ctx); }

void BytecodeIRGeneratorListener::enterBase_expression(cgullParser::Base_expressionContext* ctx) {
  // handle pushing literals
  if (ctx->literal()) {
    // check what type of literal it is from our expression types
    auto literal = ctx->literal();
    auto type = expressionTypes[literal];
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
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
      case PrimitiveType::PrimitiveKind::LONG:
      case PrimitiveType::PrimitiveKind::INT:
      case PrimitiveType::PrimitiveKind::FLOAT:
      case PrimitiveType::PrimitiveKind::STRING: {
        auto rawInstruction = std::make_shared<IRRawInstruction>("ldc " + literal->getText());
        currentFunction->instructions.push_back(rawInstruction);
        break;
      }
      default:
        throw std::runtime_error("Unsupported literal type: " + primitiveType->toString());
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
    return;
  } else {
    std::string prefix;
    switch (primitiveType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::INT:
      prefix = "i";
      break;
    case PrimitiveType::PrimitiveKind::FLOAT:
      prefix = "f";
      break;
    case PrimitiveType::PrimitiveKind::LONG:
      prefix = "l";
      break;
    case PrimitiveType::PrimitiveKind::BOOLEAN:
      prefix = "i";
      break;
    default:
      std::cout << "Expression text: " << ctx->getText() << std::endl;
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
      if (prefix == "i" || prefix == "l") {
        auto shiftLeftInstruction = std::make_shared<IRRawInstruction>("" + prefix + "shl");
        currentFunction->instructions.push_back(shiftLeftInstruction);
      } else {
        throw std::runtime_error("Unsupported shift left operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_RIGHT_SHIFT_OP()) {
      if (prefix == "i" || prefix == "l") {
        auto shiftRightInstruction = std::make_shared<IRRawInstruction>("" + prefix + "shr");
        currentFunction->instructions.push_back(shiftRightInstruction);
      } else {
        throw std::runtime_error("Unsupported shift right operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_AND_OP()) {
      if (prefix == "i" || prefix == "l") {
        auto bitwiseAndInstruction = std::make_shared<IRRawInstruction>("" + prefix + "and");
        currentFunction->instructions.push_back(bitwiseAndInstruction);
      } else {
        throw std::runtime_error("Unsupported bitwise and operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_OR_OP()) {
      if (prefix == "i" || prefix == "l") {
        auto bitwiseOrInstruction = std::make_shared<IRRawInstruction>("" + prefix + "or");
        currentFunction->instructions.push_back(bitwiseOrInstruction);
      } else {
        throw std::runtime_error("Unsupported bitwise or operation for type: " + primitiveType->toString());
      }
    } else if (ctx->BITWISE_XOR_OP()) {
      if (prefix == "i" || prefix == "l") {
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

      if (ctx->EQUAL_OP()) {
        // generate code for ==
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmpeq " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
      } else if (ctx->NOT_EQUAL_OP()) {
        // generate code for !=
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmpne " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
      } else if (ctx->LESS_OP()) {
        // generate code for <
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmplt " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
      } else if (ctx->GREATER_OP()) {
        // generate code for >
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmpgt " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
      } else if (ctx->LESS_EQUAL_OP()) {
        // generate code for <=
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmple " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
      } else if (ctx->GREATER_EQUAL_OP()) {
        // generate code for >=
        auto rawInstruction = std::make_shared<IRRawInstruction>("if_" + prefix + "cmpge " + trueLabel);
        currentFunction->instructions.push_back(rawInstruction);
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
      // reserve labels
      auto falseLabel = generateLabel();
      auto endLabel = generateLabel();

      // the right operand value is already on stack, we don't want that yet
      auto popRightInstruction = std::make_shared<IRRawInstruction>("pop");
      currentFunction->instructions.push_back(popRightInstruction);

      // if the left operand is false, we can just return false
      auto leftRawInstruction = std::make_shared<IRRawInstruction>("ifeq " + falseLabel);
      currentFunction->instructions.push_back(leftRawInstruction);

      // otherwise, we need to evaluate the right operand
      auto rightExpr = ctx->base_expression(1);
      if (rightExpr) {
        // Process base_expression manually
        enterBase_expression(rightExpr);
        exitBase_expression(rightExpr);

        // if the right operand is false, we can just return false
        auto rightRawInstruction = std::make_shared<IRRawInstruction>("ifeq " + falseLabel);
        currentFunction->instructions.push_back(rightRawInstruction);
      }

      // if both operands are true, we can return true
      auto pushTrueInstruction = std::make_shared<IRRawInstruction>("iconst 1");
      currentFunction->instructions.push_back(pushTrueInstruction);

      // jump past falseLabel to endLabel
      auto jumpInstruction = std::make_shared<IRRawInstruction>("goto " + endLabel);
      currentFunction->instructions.push_back(jumpInstruction);

      // falseLabel: one of the operands was false
      auto falseLabelInstruction = std::make_shared<IRRawInstruction>(falseLabel + ":");
      currentFunction->instructions.push_back(falseLabelInstruction);

      // push false (0) onto the stack
      auto pushFalseInstruction = std::make_shared<IRRawInstruction>("iconst 0");
      currentFunction->instructions.push_back(pushFalseInstruction);

      // endLabel:
      auto endLabelInstruction = std::make_shared<IRRawInstruction>(endLabel + ":");
      currentFunction->instructions.push_back(endLabelInstruction);
    } else if (ctx->OR_OP()) {
      // reserve labels
      auto trueLabel = generateLabel();
      auto endLabel = generateLabel();

      // the right operand value is already on stack, we don't want that yet
      auto popRightInstruction = std::make_shared<IRRawInstruction>("pop");
      currentFunction->instructions.push_back(popRightInstruction);

      // if the left operand is true, we can just return true
      auto leftRawInstruction = std::make_shared<IRRawInstruction>("ifne " + trueLabel);
      currentFunction->instructions.push_back(leftRawInstruction);

      // otherwise, we need to evaluate the right operand
      auto rightExpr = ctx->base_expression(1);
      if (rightExpr) {
        // Process base_expression manually
        enterBase_expression(rightExpr);
        exitBase_expression(rightExpr);

        // if the right operand is true, we can just return true
        auto rightRawInstruction = std::make_shared<IRRawInstruction>("ifne " + trueLabel);
        currentFunction->instructions.push_back(rightRawInstruction);
      }

      // if both operands are false, we can return false
      auto pushFalseInstruction = std::make_shared<IRRawInstruction>("iconst 0");
      currentFunction->instructions.push_back(pushFalseInstruction);

      // jump past trueLabel to endLabel
      auto jumpInstruction = std::make_shared<IRRawInstruction>("goto " + endLabel);
      currentFunction->instructions.push_back(jumpInstruction);

      // trueLabel: one of the operands was true
      auto trueLabelInstruction = std::make_shared<IRRawInstruction>(trueLabel + ":");
      currentFunction->instructions.push_back(trueLabelInstruction);

      // push true (1) onto the stack
      auto pushTrueInstruction = std::make_shared<IRRawInstruction>("iconst 1");
      currentFunction->instructions.push_back(pushTrueInstruction);

      // endLabel:
      auto endLabelInstruction = std::make_shared<IRRawInstruction>(endLabel + ":");
      currentFunction->instructions.push_back(endLabelInstruction);
    }
  }
  generateStringConversion(ctx);
}

void BytecodeIRGeneratorListener::enterVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  auto scope = getCurrentScope(ctx);
  if (scope) {
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol) {
      // assign a local index to this variable
      int localIndex = assignLocalIndex(varSymbol);
    }
  }
}

void BytecodeIRGeneratorListener::exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) {
  // if there's an initialization expression, store its result in the variable
  if (ctx->expression()) {
    auto scope = getCurrentScope(ctx);
    std::string identifier = ctx->IDENTIFIER()->getText();
    auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

    if (varSymbol) {
      auto type = varSymbol->dataType;
      auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);

      if (primitiveType) {
        std::string storeInstruction;
        switch (primitiveType->getPrimitiveKind()) {
        case PrimitiveType::PrimitiveKind::INT:
          storeInstruction = "istore " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::FLOAT:
          storeInstruction = "fstore " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::LONG:
          storeInstruction = "lstore " + std::to_string(varSymbol->localIndex);
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

      if (primitiveType) {
        // load value from the local variable onto the stack
        std::string loadInstruction;
        switch (primitiveType->getPrimitiveKind()) {
        case PrimitiveType::PrimitiveKind::INT:
          loadInstruction = "iload " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::FLOAT:
          loadInstruction = "fload " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::STRING:
          loadInstruction = "aload " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::LONG:
          loadInstruction = "lload " + std::to_string(varSymbol->localIndex);
          break;
        case PrimitiveType::PrimitiveKind::BOOLEAN:
          loadInstruction = "iload " + std::to_string(varSymbol->localIndex);
          break;
        default:
          throw std::runtime_error("Unsupported variable type for loading: " + primitiveType->toString());
        }

        auto loadInst = std::make_shared<IRRawInstruction>(loadInstruction);
        currentFunction->instructions.push_back(loadInst);
      }
    }
  }
}

void BytecodeIRGeneratorListener::exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) {
  if (ctx->variable() && ctx->expression()) {
    auto scope = getCurrentScope(ctx);
    auto variable = ctx->variable();

    if (variable->IDENTIFIER()) {
      std::string identifier = variable->IDENTIFIER()->getText();
      auto varSymbol = std::dynamic_pointer_cast<VariableSymbol>(scope->resolve(identifier));

      if (varSymbol) {
        auto type = varSymbol->dataType;
        auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);

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
          case PrimitiveType::PrimitiveKind::LONG:
            storeInstruction = "lstore " + std::to_string(varSymbol->localIndex);
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
}

void BytecodeIRGeneratorListener::exitReturn_statement(cgullParser::Return_statementContext* ctx) {
  // for now, we don't need to evaluate anything since we're just using main
  auto returnInst = std::make_shared<IRRawInstruction>("return");
  currentFunction->instructions.push_back(returnInst);
}
