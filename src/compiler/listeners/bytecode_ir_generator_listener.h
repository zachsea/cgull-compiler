#ifndef BYTECODE_IR_GENERATOR_LISTENER_H
#define BYTECODE_IR_GENERATOR_LISTENER_H

#include "../errors/error_reporter.h"
#include "../instructions/ir_instruction.h"
#include "../symbols/symbol.h"
#include "cgullBaseListener.h"

class BytecodeIRGeneratorListener : public cgullBaseListener {
public:
  BytecodeIRGeneratorListener(
      ErrorReporter& errorReporter,
      const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
      const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes);

  const std::vector<IRInstruction>& getInstructions() const;

private:
  ErrorReporter& errorReporter;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;
  std::vector<IRInstruction> instructions;
  int labelCounter = 0;
  std::stack<std::string> breakLabels;

  std::shared_ptr<Scope> getCurrentScope(antlr4::ParserRuleContext* ctx) const;
  std::string generateLabel();
};

#endif // BYTECODE_IR_GENERATOR_LISTENER_H