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
      std::cout << "condition label: " << labels.conditionLabel << std::endl;
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
      std::cout << "forStmt: " << forStmt->expression(0)->getText() << std::endl;
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
}

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
        auto loadInst = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                           std::to_string(varSymbol->localIndex));
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
    } else if (ctx->INCREMENT_OP()) {
      if (typeKind == PrimitiveType::PrimitiveKind::INT) {
        auto rawInstructionIncrement = std::make_shared<IRRawInstruction>("iinc");
        currentFunction->instructions.push_back(rawInstructionIncrement);
      } else {
        throw std::runtime_error("Unsupported unary expression type: " + expressionType->toString());
      }
    } else if (ctx->DECREMENT_OP()) {
      // there are no decrement instructions, so just subtract 1 normally
      if (typeKind == PrimitiveType::PrimitiveKind::INT) {
        auto rawInstructionSubtract = std::make_shared<IRRawInstruction>("iconst 1");
        currentFunction->instructions.push_back(rawInstructionSubtract);
        auto rawInstructionSubtract2 = std::make_shared<IRRawInstruction>("isub");
        currentFunction->instructions.push_back(rawInstructionSubtract2);
      } else if (typeKind == PrimitiveType::PrimitiveKind::FLOAT) {
        auto rawInstructionSubtract = std::make_shared<IRRawInstruction>("fconst 1");
        currentFunction->instructions.push_back(rawInstructionSubtract);
        auto rawInstructionSubtract2 = std::make_shared<IRRawInstruction>("fsub");
        currentFunction->instructions.push_back(rawInstructionSubtract2);
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
        auto loadInstruction = std::make_shared<IRRawInstruction>(getLoadInstruction(primitiveType) + " " +
                                                                  std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(loadInstruction);

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

        auto storeInst = std::make_shared<IRRawInstruction>(getStoreInstruction(primitiveType) + " " +
                                                            std::to_string(varSymbol->localIndex));
        currentFunction->instructions.push_back(storeInst);
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
