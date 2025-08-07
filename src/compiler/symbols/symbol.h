#ifndef SYMBOL_H
#define SYMBOL_H

#include "../instructions/ir_instruction.h"
#include "type.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolType { VARIABLE, FUNCTION, STRUCT, PARAMETER, TYPE };

class Scope;

class Symbol {
public:
  Symbol(const std::string& name, SymbolType type, int line, int column, std::shared_ptr<Scope> scope);
  virtual ~Symbol() = default;

  std::string name;
  SymbolType type;
  int definedAtLine;
  int definedAtColumn;
  bool isDefined = false;
  bool isPrivate = false;
  bool isBuiltin = false;
  std::shared_ptr<Scope> scope;
};

class VariableSymbol : public Symbol {
public:
  VariableSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope, bool isConst = false,
                 bool isStructMember = false);
  std::shared_ptr<Type> dataType;
  std::shared_ptr<TypeSymbol> parentStructType;
  bool isConstant;
  bool isStructMember;
  bool hasDefaultValue = false;
  int localIndex = -1;
};

class TypeSymbol : public Symbol {
public:
  TypeSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope);

  std::shared_ptr<Scope> memberScope;
  std::shared_ptr<Type> typeRepresentation;
};

class FunctionSymbol : public Symbol {
public:
  FunctionSymbol(const std::string& name, int line, int column, std::shared_ptr<Scope> scope);
  bool isStructMethod = false;
  std::vector<std::shared_ptr<VariableSymbol>> parameters;
  std::vector<std::shared_ptr<Type>> returnTypes;
  std::vector<std::shared_ptr<IRInstruction>> instructions;

  // includes parameter types to avoid name collisions
  std::string getMangledName() const;
  bool canOverloadWith(const FunctionSymbol& other) const;
};

class Scope {
public:
  Scope(std::shared_ptr<Scope> parent);
  ~Scope() = default;

  std::shared_ptr<Symbol> resolve(const std::string& name);
  bool add(std::shared_ptr<Symbol> symbol);
  bool addFunction(std::shared_ptr<FunctionSymbol> functionSymbol);
  std::shared_ptr<FunctionSymbol> resolveFunctionCall(const std::string& name,
                                                      const std::vector<std::shared_ptr<Type>>& argTypes);

  bool isTypeCompatible(const std::shared_ptr<Type>& sourceType, const std::shared_ptr<Type>& targetType);

  std::shared_ptr<Scope> parent;
  std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
  std::unordered_map<std::string, std::vector<std::shared_ptr<FunctionSymbol>>> functionOverloads;
  std::unordered_map<std::string, std::shared_ptr<Symbol>> unresolved;
};

#endif // SYMBOL_H
