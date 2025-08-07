#include "primitive_wrapper_generator.h"

std::shared_ptr<IRClass> PrimitiveWrapperGenerator::generateWrapperClass(PrimitiveType::PrimitiveKind kind) {
  std::string className = getClassName(kind);
  auto irClass = std::make_shared<IRClass>();
  irClass->name = className;

  // bools are a special case, using iload/istore but Z for the field type
  // similar with strings, using a/astore but Ljava/lang/String; for the field type
  std::string fieldType = kind == PrimitiveType::PrimitiveKind::BOOLEAN
                              ? "Z"
                              : (kind == PrimitiveType::PrimitiveKind::STRING
                                     ? "java/lang/String"
                                     : std::string(1, std::toupper(getInstructionPrefix(kind)[0])));

  // generate field
  auto valueType = std::make_shared<PrimitiveType>(kind);
  auto fieldScope = std::make_shared<Scope>(nullptr);
  auto valueField = std::make_shared<VariableSymbol>("value", 0, 0, fieldScope);
  valueField->dataType = valueType;
  valueField->isDefined = true;

  // constructor method
  auto constructorScope = std::make_shared<Scope>(nullptr);
  auto constructor = std::make_shared<FunctionSymbol>("<init>", 0, 0, constructorScope);
  constructor->isDefined = true;

  // constructor parameter
  auto paramSymbol = std::make_shared<VariableSymbol>("value", 0, 0, constructorScope);
  paramSymbol->dataType = valueType;
  paramSymbol->isDefined = true;
  constructor->parameters.push_back(paramSymbol);

  // constructor return type (void)
  constructor->returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));

  // constructor instructions
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>("aload 0"));
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>("invokespecial java/lang/Object.<init>()V"));
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>("aload 0"));
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>(getInstructionPrefix(kind) + "load 1"));
  constructor->instructions.push_back(
      std::make_shared<IRRawInstruction>("putfield " + className + ".value " + fieldType));
  constructor->instructions.push_back(std::make_shared<IRRawInstruction>("return"));

  // getter method
  auto getterScope = std::make_shared<Scope>(nullptr);
  auto getter = std::make_shared<FunctionSymbol>("getValue", 0, 0, getterScope);
  getter->isDefined = true;
  getter->isStructMethod = true;
  getter->returnTypes.push_back(valueType);

  // getter instructions
  getter->instructions.push_back(std::make_shared<IRRawInstruction>("aload 0"));
  getter->instructions.push_back(std::make_shared<IRRawInstruction>("getfield " + className + ".value " + fieldType));
  getter->instructions.push_back(std::make_shared<IRRawInstruction>(getInstructionPrefix(kind) + "return"));
  // setter method
  auto setterScope = std::make_shared<Scope>(nullptr);
  auto setter = std::make_shared<FunctionSymbol>("setValue", 0, 0, setterScope);
  setter->isDefined = true;
  setter->isStructMethod = true;
  setter->returnTypes.push_back(std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID));

  // setter parameter
  auto setterParam = std::make_shared<VariableSymbol>("value", 0, 0, setterScope);
  setterParam->dataType = valueType;
  setterParam->isDefined = true;
  setter->parameters.push_back(setterParam);

  // setter instructions
  setter->instructions.push_back(std::make_shared<IRRawInstruction>("aload 0"));
  setter->instructions.push_back(std::make_shared<IRRawInstruction>(getInstructionPrefix(kind) + "load 1"));
  setter->instructions.push_back(std::make_shared<IRRawInstruction>("putfield " + className + ".value " + fieldType));
  setter->instructions.push_back(std::make_shared<IRRawInstruction>("return"));

  // add methods to class
  irClass->methods.push_back(constructor);
  irClass->methods.push_back(getter);
  irClass->methods.push_back(setter);

  return irClass;
}

std::string PrimitiveWrapperGenerator::getClassName(PrimitiveType::PrimitiveKind kind) {
  switch (kind) {
  case PrimitiveType::PrimitiveKind::INT:
    return "IntReference";
  case PrimitiveType::PrimitiveKind::FLOAT:
    return "FloatReference";
  case PrimitiveType::PrimitiveKind::BOOLEAN:
    return "BoolReference";
  case PrimitiveType::PrimitiveKind::STRING:
    return "StringReference";
  default:
    return "UnknownReference";
  }
}

std::string PrimitiveWrapperGenerator::getInstructionPrefix(PrimitiveType::PrimitiveKind kind) {
  switch (kind) {
  case PrimitiveType::PrimitiveKind::BOOLEAN:
  case PrimitiveType::PrimitiveKind::INT:
    return "i";
  case PrimitiveType::PrimitiveKind::FLOAT:
    return "f";
  case PrimitiveType::PrimitiveKind::STRING:
    return "a";
  default:
    return "";
  }
}
