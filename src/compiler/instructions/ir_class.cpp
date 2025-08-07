#include "ir_class.h"

std::shared_ptr<FunctionSymbol> IRClass::getMethod(const std::string& name) {
  for (auto method : methods) {
    if (method->name == name) {
      return method;
    }
  }
  return nullptr;
}
