#include "type.h"
#include "symbol.h"
#include <sstream>

Type::Type(TypeKind kind) : kind(kind) {}

Type::TypeKind Type::getKind() const { return kind; }

PrimitiveType::PrimitiveType(PrimitiveKind primitiveKind) : Type(TypeKind::PRIMITIVE), primitiveKind(primitiveKind) {}

PrimitiveType::PrimitiveKind PrimitiveType::getPrimitiveKind() const { return primitiveKind; }

bool PrimitiveType::isNumeric() const {
  switch (primitiveKind) {
  case PrimitiveKind::STRING:
  case PrimitiveKind::VOID:
    return false;
  default:
    return true;
  }
}

bool PrimitiveType::isInteger() const {
  switch (primitiveKind) {
  case PrimitiveKind::INT:
    return true;
  default:
    return false;
  }
}

std::string PrimitiveType::toString() const {
  switch (primitiveKind) {
  case PrimitiveKind::INT:
    return "int";
  case PrimitiveKind::FLOAT:
    return "float";
  case PrimitiveKind::BOOLEAN:
    return "bool";
  case PrimitiveKind::STRING:
    return "string";
  case PrimitiveKind::VOID:
    return "void";
  default:
    return "unknown";
  }
}

bool PrimitiveType::equals(const std::shared_ptr<Type>& other) const {
  if (other->getKind() != TypeKind::PRIMITIVE) {
    return false;
  }

  auto otherPrimitive = std::dynamic_pointer_cast<PrimitiveType>(other);
  return primitiveKind == otherPrimitive->primitiveKind;
}

UserDefinedType::UserDefinedType(std::shared_ptr<TypeSymbol> typeSymbol)
    : Type(TypeKind::USER_DEFINED), typeSymbol(typeSymbol) {}

std::shared_ptr<TypeSymbol> UserDefinedType::getTypeSymbol() const { return typeSymbol; }

bool UserDefinedType::equals(const std::shared_ptr<Type>& other) const {
  if (other->getKind() != TypeKind::USER_DEFINED) {
    return false;
  }

  auto otherUserDefined = std::dynamic_pointer_cast<UserDefinedType>(other);
  return typeSymbol == otherUserDefined->typeSymbol;
}

std::string UserDefinedType::toString() const { return typeSymbol ? typeSymbol->name : "unknown"; }

ArrayType::ArrayType(std::shared_ptr<Type> elementType) : Type(TypeKind::ARRAY), elementType(elementType) {}

std::shared_ptr<Type> ArrayType::getElementType() const { return elementType; }

bool ArrayType::equals(const std::shared_ptr<Type>& other) const {
  if (other->getKind() != TypeKind::ARRAY) {
    return false;
  }

  auto otherArray = std::dynamic_pointer_cast<ArrayType>(other);
  return elementType->equals(otherArray->elementType);
}

int ArrayType::getDimensions() const {
  if (elementType->getKind() == TypeKind::ARRAY) {
    return 1 + std::dynamic_pointer_cast<ArrayType>(elementType)->getDimensions();
  }
  return 1;
}

std::string ArrayType::toString() const { return elementType->toString() + "[]"; }

TupleType::TupleType(const std::vector<std::shared_ptr<Type>>& elementTypes)
    : Type(TypeKind::TUPLE), elementTypes(elementTypes) {}

const std::vector<std::shared_ptr<Type>>& TupleType::getElementTypes() const { return elementTypes; }

bool TupleType::equals(const std::shared_ptr<Type>& other) const {
  if (other == nullptr) {
    return false;
  }
  if (other->getKind() != TypeKind::TUPLE) {
    return false;
  }

  auto otherTuple = std::dynamic_pointer_cast<TupleType>(other);
  if (elementTypes.size() != otherTuple->elementTypes.size()) {
    return false;
  }

  for (size_t i = 0; i < elementTypes.size(); ++i) {
    if (!elementTypes[i]->equals(otherTuple->elementTypes[i])) {
      return false;
    }
  }
  return true;
}

std::string TupleType::toString() const {
  std::stringstream ss;
  ss << "tuple<";
  for (size_t i = 0; i < elementTypes.size(); ++i) {
    if (i > 0)
      ss << ", ";
    ss << elementTypes[i]->toString();
  }
  ss << ">";
  return ss.str();
}

PointerType::PointerType(std::shared_ptr<Type> pointeeType) : Type(TypeKind::POINTER), pointeeType(pointeeType) {}

std::shared_ptr<Type> PointerType::getPointedType() const { return pointeeType; }

bool PointerType::equals(const std::shared_ptr<Type>& other) const {
  if (other->getKind() != TypeKind::POINTER) {
    return false;
  }

  auto otherPointer = std::dynamic_pointer_cast<PointerType>(other);
  return pointeeType->equals(otherPointer->pointeeType);
}

std::string PointerType::toString() const {
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(pointeeType);
  if (!primitiveType) {
    return "UnknownReference";
  }
  switch (primitiveType->getPrimitiveKind()) {
  case PrimitiveType::PrimitiveKind::INT:
    return "IntReference";
  case PrimitiveType::PrimitiveKind::FLOAT:
    return "FloatReference";
  case PrimitiveType::PrimitiveKind::BOOLEAN:
    return "BoolReference";
  case PrimitiveType::PrimitiveKind::STRING:
    return "StringReference";
  case PrimitiveType::PrimitiveKind::VOID:
    return "VoidReference";
  default:
    return "UnknownReference";
  }
}

UnresolvedType::UnresolvedType(const std::string& name) : Type(TypeKind::UNRESOLVED), name(name) {}

std::string UnresolvedType::getName() const { return name; }

std::string UnresolvedType::toString() const { return "unresolved<" + name + ">"; }
