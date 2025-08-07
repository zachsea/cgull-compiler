#pragma once

#include "instructions/ir_class.h"
#include "symbols/type.h"

class PrimitiveWrapperGenerator {
public:
  static std::shared_ptr<IRClass> generateWrapperClass(PrimitiveType::PrimitiveKind kind);
  static std::string getClassName(PrimitiveType::PrimitiveKind kind);

private:
  static std::string getInstructionPrefix(PrimitiveType::PrimitiveKind kind);
};
