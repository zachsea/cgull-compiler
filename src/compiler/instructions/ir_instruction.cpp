#include "ir_instruction.h"
#include "../symbols/symbol.h"

IRCallInstruction::IRCallInstruction(const std::shared_ptr<FunctionSymbol>& function) : function(function) {}

std::string IRCallInstruction::toString() const {
  // temp
  return "call " + function->getMangledName();
}

IRRawInstruction::IRRawInstruction(const std::string& instruction) : instruction(instruction) {}

std::string IRRawInstruction::toString() const { return instruction; }
