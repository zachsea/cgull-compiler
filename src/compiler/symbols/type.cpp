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
  case PrimitiveKind::SHORT:
  case PrimitiveKind::LONG:
  case PrimitiveKind::UNSIGNED_INT:
  case PrimitiveKind::UNSIGNED_SHORT:
  case PrimitiveKind::UNSIGNED_LONG:
  case PrimitiveKind::SIGNED_INT:
  case PrimitiveKind::SIGNED_SHORT:
  case PrimitiveKind::SIGNED_LONG:
  case PrimitiveKind::SIGNED_CHAR:
    return true;
  default:
    return false;
  }
}

std::string PrimitiveType::toString() const {
  switch (primitiveKind) {
  case PrimitiveKind::INT:
    return "int";
  case PrimitiveKind::SHORT:
    return "short";
  case PrimitiveKind::LONG:
    return "long";
  case PrimitiveKind::FLOAT:
    return "float";
  case PrimitiveKind::CHAR:
    return "char";
  case PrimitiveKind::BOOLEAN:
    return "bool";
  case PrimitiveKind::STRING:
    return "string";
  case PrimitiveKind::VOID:
    return "void";
  case PrimitiveKind::UNSIGNED_INT:
    return "unsigned int";
  case PrimitiveKind::UNSIGNED_SHORT:
    return "unsigned short";
  case PrimitiveKind::UNSIGNED_LONG:
    return "unsigned long";
  case PrimitiveKind::UNSIGNED_CHAR:
    return "unsigned char";
  case PrimitiveKind::SIGNED_INT:
    return "signed int";
  case PrimitiveKind::SIGNED_SHORT:
    return "signed short";
  case PrimitiveKind::SIGNED_LONG:
    return "signed long";
  case PrimitiveKind::SIGNED_CHAR:
    return "signed char";
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

std::string PointerType::toString() const { return pointeeType->toString() + "*"; }

UnresolvedType::UnresolvedType(const std::string& name) : Type(TypeKind::UNRESOLVED), name(name) {}

std::string UnresolvedType::getName() const { return name; }

std::string UnresolvedType::toString() const { return "unresolved<" + name + ">"; }
