#ifndef IR_CLASS_H
#define IR_CLASS_H

#include "../symbols/symbol.h"
#include "ir_instruction.h"
#include <memory>
#include <string>
#include <vector>

struct IRClass {
  std::string name;
  std::vector<std::shared_ptr<IRInstruction>> instructions;
  std::vector<std::shared_ptr<FunctionSymbol>> methods;
};

#endif // IR_CLASS_H
