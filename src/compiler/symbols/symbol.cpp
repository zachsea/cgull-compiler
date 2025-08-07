#include "symbol.h"

Symbol::Symbol(const std::string& name, SymbolType type, int line, int column, std::shared_ptr<Scope> scope)
    : name(name), type(type), definedAtLine(line), definedAtColumn(column), isDefined(false), scope(scope) {}

VariableSymbol::VariableSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope,
                               bool isConst)
    : Symbol(name, SymbolType::VARIABLE, line, column, scope), isConstant(isConst) {}

ArraySymbol::ArraySymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope)
    : Symbol(name, SymbolType::ARRAY, line, column, scope) {}

TypeSymbol::TypeSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope)
    : Symbol(name, SymbolType::TYPE, line, column, scope) {}

FunctionSymbol::FunctionSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope)
    : Symbol(name, SymbolType::FUNCTION, line, column, scope) {}

std::string FunctionSymbol::getMangledName() const {
  std::string mangledName = name;
  mangledName += "_";

  for (const auto& param : parameters) {
    if (param->dataType) {
      mangledName += param->dataType->toString();
    } else {
      mangledName += "unknown";
    }
    mangledName += "_";
  }

  return mangledName;
}

bool FunctionSymbol::canOverloadWith(const FunctionSymbol& other) const {
  if (parameters.size() != other.parameters.size()) {
    return true;
  }

  for (size_t i = 0; i < parameters.size(); i++) {
    auto thisType = parameters[i]->dataType;
    auto otherType = other.parameters[i]->dataType;

    if (!thisType || !otherType) {
      return false;
    }

    if (thisType->toString() != otherType->toString()) {
      return true;
    }
  }
  // all are the same, cannot overload
  return false;
}

Scope::Scope(std::shared_ptr<Scope> parent) : parent(parent) {}

std::shared_ptr<Symbol> Scope::resolve(const std::string& name) {
  auto it = symbols.find(name);
  if (it != symbols.end()) {
    return it->second;
  }

  auto overloadIt = functionOverloads.find(name);
  if (overloadIt != functionOverloads.end() && !overloadIt->second.empty()) {
    return overloadIt->second.front();
  }

  if (parent != nullptr) {
    return parent->resolve(name);
  }

  return nullptr;
}

bool Scope::add(std::shared_ptr<Symbol> symbol) {
  if (symbol->type == SymbolType::FUNCTION) {
    return addFunction(std::dynamic_pointer_cast<FunctionSymbol>(symbol));
  }
  if (symbols.find(symbol->name) != symbols.end()) {
    return false;
  }
  symbols[symbol->name] = symbol;
  return true;
}

bool Scope::addFunction(std::shared_ptr<FunctionSymbol> functionSymbol) {
  // always use the mangled name for storing functions
  std::string mangledName = functionSymbol->getMangledName();

  if (symbols.find(mangledName) != symbols.end()) {
    return false;
  }
  symbols[mangledName] = functionSymbol;
  functionOverloads[functionSymbol->name].push_back(functionSymbol);

  return true;
}

std::shared_ptr<FunctionSymbol> Scope::resolveFunctionCall(const std::string& name,
                                                           const std::vector<std::shared_ptr<Type>>& argTypes) {
  // get all function overloads for this name
  auto overloadIt = functionOverloads.find(name);
  if (overloadIt == functionOverloads.end()) {
    if (parent) {
      return parent->resolveFunctionCall(name, argTypes);
    }
    return nullptr;
  }

  const auto& overloads = overloadIt->second;
  std::shared_ptr<FunctionSymbol> bestMatch = nullptr;

  for (const auto& func : overloads) {
    if (func->parameters.size() != argTypes.size()) {
      continue;
    }

    bool isMatch = true;
    for (size_t i = 0; i < argTypes.size(); i++) {
      if (!argTypes[i] || !func->parameters[i]->dataType) {
        isMatch = false;
        break;
      }

      if (argTypes[i]->toString() != func->parameters[i]->dataType->toString()) {
        isMatch = false;
        break;
      }
    }

    if (isMatch) {
      bestMatch = func;
      break;
    }
  }

  if (!bestMatch) {
    for (const auto& func : overloads) {
      // Still require parameter count to match
      if (func->parameters.size() != argTypes.size()) {
        continue;
      }

      bestMatch = func;
      break;
    }
  }

  return bestMatch;
}

bool Scope::isTypeCompatible(const std::shared_ptr<Type>& sourceType, const std::shared_ptr<Type>& targetType) {
  if (sourceType->toString() == targetType->toString()) {
    return true;
  }

  // check for pointer compatibility
  auto sourcePointer = std::dynamic_pointer_cast<PointerType>(sourceType);
  auto targetPointer = std::dynamic_pointer_cast<PointerType>(targetType);
  if (sourcePointer && targetPointer) {
    // allow void* assignable to any pointer
    auto targetPointedType = targetPointer->getPointedType();
    auto sourcePointedType = sourcePointer->getPointedType();

    auto sourcePrim = std::dynamic_pointer_cast<PrimitiveType>(sourcePointedType);
    if (sourcePrim && sourcePrim->getPrimitiveKind() == PrimitiveType::PrimitiveKind::VOID) {
      return true;
    }

    // check pointed types recursively
    return isTypeCompatible(sourcePointedType, targetPointedType);
  }

  // check for numeric compatibility
  auto sourcePrim = std::dynamic_pointer_cast<PrimitiveType>(sourceType);
  auto targetPrim = std::dynamic_pointer_cast<PrimitiveType>(targetType);
  if (sourcePrim && targetPrim) {
    // allow conversion between numeric types
    return sourcePrim->isNumeric() && targetPrim->isNumeric();
  }

  return false;
}
