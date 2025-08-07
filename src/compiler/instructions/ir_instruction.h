#ifndef IR_INSTRUCTION_H
#define IR_INSTRUCTION_H

#include <string>
#include <vector>

enum IROpCode {
  OP_PUSH_INT,
  OP_PUSH_FLOAT,
  OP_PUSH_STRING,
  OP_PUSH_BOOL,
  OP_DECLARE_VAR,
  OP_LOAD_VAR,
  OP_STORE_VAR,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_AND,
  OP_OR,
  OP_NOT,
  OP_EQUAL,
  OP_NOT_EQUAL,
  OP_LESS,
  OP_GREATER,
  OP_LESS_EQUAL,
  OP_GREATER_EQUAL,
  OP_LABEL,
  OP_JUMP,
  OP_JUMP_IF_TRUE,
  OP_JUMP_IF_FALSE,
  OP_PRINT,
  OP_PRINTLN,
  OP_READLINE,
  CONVERT
};

class IRInstruction {
public:
  IROpCode opCode;
  std::vector<std::string> operands;

  IRInstruction(IROpCode opCode, const std::vector<std::string>& operands);

  // common operand counts, easier to use with emplace
  IRInstruction(IROpCode opCode, const std::string& operand);
  IRInstruction(IROpCode opCode, const std::string& operand1, const std::string& operand2);
};

#endif // IR_INSTRUCTION_H
