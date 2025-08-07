#include "ir_instruction.h"

IRInstruction::IRInstruction(IROpCode opCode, const std::vector<std::string>& operands)
    : opCode(opCode), operands(operands) {}

IRInstruction::IRInstruction(IROpCode opCode, const std::string& operand) : opCode(opCode), operands({operand}) {}

IRInstruction::IRInstruction(IROpCode opCode, const std::string& operand1, const std::string& operand2)
    : opCode(opCode), operands({operand1, operand2}) {}
