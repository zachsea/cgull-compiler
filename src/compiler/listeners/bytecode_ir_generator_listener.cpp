#include "bytecode_ir_generator_listener.h"

BytecodeIRGeneratorListener::BytecodeIRGeneratorListener(
    ErrorReporter& errorReporter, std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes)
    : errorReporter(errorReporter), scopes(scopes), expressionTypes(expressionTypes) {}

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

void BytecodeIRGeneratorListener::enterBase_expression(cgullParser::Base_expressionContext* ctx) {
  // handle pushing literals
  if (ctx->literal()) {
    std::cout << "Literal: " << ctx->literal()->getText() << "\n";
    // check what type of literal it is from our expression types
    auto literal = ctx->literal();
    auto type = expressionTypes[literal];
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
    if (primitiveType) {
      PrimitiveType::PrimitiveKind primitiveKind = primitiveType->getPrimitiveKind();
      switch (primitiveKind) {
      case PrimitiveType::PrimitiveKind::INT:
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

void BytecodeIRGeneratorListener::exitBase_expression(cgullParser::Base_expressionContext* ctx) {}
