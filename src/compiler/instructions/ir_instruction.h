#ifndef IR_INSTRUCTION_H
#define IR_INSTRUCTION_H

#include <memory>
#include <string>

class FunctionSymbol;

class IRInstruction {
public:
  virtual ~IRInstruction() = default;
  virtual std::string toString() const = 0;
};

class IRCallInstruction : public IRInstruction {
public:
  IRCallInstruction(const std::shared_ptr<FunctionSymbol>& function);
  std::shared_ptr<FunctionSymbol> function;

  std::string toString() const override;
};

class IRRawInstruction : public IRInstruction {
public:
  IRRawInstruction(const std::string& instruction);
  std::string instruction;
  std::string toString() const override;
};

#endif // IR_INSTRUCTION_H
