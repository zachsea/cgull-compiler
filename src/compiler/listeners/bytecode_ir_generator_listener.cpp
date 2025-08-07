#include "bytecode_ir_generator_listener.h"

BytecodeIRGeneratorListener::BytecodeIRGeneratorListener(
    ErrorReporter& errorReporter, const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
    const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes)
    : errorReporter(errorReporter), scopes(scopes), expressionTypes(expressionTypes) {}

const std::vector<IRInstruction>& BytecodeIRGeneratorListener::getInstructions() const { return instructions; }

std::shared_ptr<Scope> BytecodeIRGeneratorListener::getCurrentScope(antlr4::ParserRuleContext* ctx) const {
  auto it = scopes.find(ctx);
  if (it != scopes.end()) {
    return it->second;
  }
  return nullptr;
}

std::string BytecodeIRGeneratorListener::generateLabel() { return "L" + std::to_string(labelCounter++); }
