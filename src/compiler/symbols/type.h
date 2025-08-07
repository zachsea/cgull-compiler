#ifndef TYPE_H
#define TYPE_H

#include <memory>
#include <string>
#include <vector>

class IRClass;

class TypeSymbol;

class Type {
public:
  enum class TypeKind { PRIMITIVE, USER_DEFINED, ARRAY, TUPLE, POINTER, UNRESOLVED };

  Type(TypeKind kind);
  virtual ~Type() = default;
  TypeKind getKind() const;
  virtual std::string toString() const = 0;
  virtual bool equals(const std::shared_ptr<Type>& other) const { return kind == other->getKind(); }

private:
  TypeKind kind;
};

class PrimitiveType : public Type {
public:
  enum class PrimitiveKind {
    INT,
    FLOAT,
    BOOLEAN,
    STRING,
    VOID,
  };

  PrimitiveType(PrimitiveKind primitiveKind);
  PrimitiveKind getPrimitiveKind() const;
  bool isNumeric() const;
  bool isInteger() const;
  std::string toString() const override;
  bool equals(const std::shared_ptr<Type>& other) const override;

private:
  PrimitiveKind primitiveKind;
  const std::shared_ptr<IRClass> wrapperClass;
};

class UserDefinedType : public Type {
public:
  UserDefinedType(std::shared_ptr<TypeSymbol> typeSymbol);
  std::shared_ptr<TypeSymbol> getTypeSymbol() const;
  std::string toString() const override;
  bool equals(const std::shared_ptr<Type>& other) const override;

private:
  std::shared_ptr<TypeSymbol> typeSymbol;
};

class ArrayType : public Type {
public:
  ArrayType(std::shared_ptr<Type> elementType);
  std::shared_ptr<Type> getElementType() const;
  std::string toString() const override;
  bool equals(const std::shared_ptr<Type>& other) const override;

private:
  std::shared_ptr<Type> elementType;
};

class TupleType : public Type {
public:
  std::vector<std::shared_ptr<Type>> elementTypes;

  TupleType(const std::vector<std::shared_ptr<Type>>& elementTypes);
  const std::vector<std::shared_ptr<Type>>& getElementTypes() const;
  std::string toString() const override;
  bool equals(const std::shared_ptr<Type>& other) const override;
};

class PointerType : public Type {
public:
  PointerType(std::shared_ptr<Type> pointeeType);
  std::shared_ptr<Type> getPointedType() const;
  bool equals(const std::shared_ptr<Type>& other) const override;
  std::string toString() const override;

private:
  std::shared_ptr<Type> pointeeType;
};

class UnresolvedType : public Type {
public:
  UnresolvedType(const std::string& name);
  std::string getName() const;
  std::string toString() const override;

private:
  std::string name;
};

#endif // TYPE_H
