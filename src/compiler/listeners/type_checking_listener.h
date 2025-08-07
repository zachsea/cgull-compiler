#ifndef TYPE_CHECKING_LISTENER_H
#define TYPE_CHECKING_LISTENER_H

#include "../errors/error_reporter.h"
#include "../symbols/symbol.h"
#include "../symbols/type.h"
#include <cgullBaseListener.h>
#include <unordered_map>

class TypeCheckingListener : public cgullBaseListener {
public:
  TypeCheckingListener(ErrorReporter& errorReporter,
                       const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
                       std::shared_ptr<Scope> globalScope);

  // evaluate the end result of an expression
  std::shared_ptr<Type> getExpressionType(antlr4::ParserRuleContext* ctx) const;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> getExpressionTypes() const;
  std::unordered_set<antlr4::ParserRuleContext*> getExpectingStringConversion() const;

private:
  ErrorReporter& errorReporter;
  std::shared_ptr<Scope> currentScope;
  std::shared_ptr<Scope> globalScope;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;

  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;

  std::unordered_set<antlr4::ParserRuleContext*> expectingStringConversion;

  // helper methods
  std::shared_ptr<Type> resolveType(cgullParser::TypeContext* typeCtx);
  std::shared_ptr<Type> resolvePrimitiveType(const std::string& typeName);
  bool areTypesCompatible(const std::shared_ptr<Type>& sourceType, const std::shared_ptr<Type>& targetType,
                          antlr4::ParserRuleContext* sourceCtx = nullptr,
                          antlr4::ParserRuleContext* targetCtx = nullptr);
  std::shared_ptr<Type> getFieldType(const std::shared_ptr<Type>& baseType, const std::string& fieldName);
  std::shared_ptr<Type> getElementType(const std::shared_ptr<Type>& arrayType);
  void setExpressionType(antlr4::ParserRuleContext* ctx, std::shared_ptr<Type> type);

  bool hasToStringMethod(const std::shared_ptr<Type>& type);
  bool canConvertToString(const std::shared_ptr<Type>& type);

  std::vector<std::shared_ptr<Type>> currentFunctionReturnTypes;

  // store temporary context for field access
  std::unordered_map<antlr4::ParserRuleContext*, std::stack<std::shared_ptr<Type>>> fieldAccessContexts;
  std::unordered_map<antlr4::ParserRuleContext*, bool> isDereferenceContexts;

  void enterEveryRule(antlr4::ParserRuleContext* ctx) override;

  void exitVariable(cgullParser::VariableContext* ctx) override;
  void exitLiteral(cgullParser::LiteralContext* ctx) override;
  void exitIndexable(cgullParser::IndexableContext* ctx) override;

  void exitExpression(cgullParser::ExpressionContext* ctx) override;
  void exitBase_expression(cgullParser::Base_expressionContext* ctx) override;

  void enterField_access(cgullParser::Field_accessContext* ctx) override;
  void exitField_access(cgullParser::Field_accessContext* ctx) override;
  void exitField(cgullParser::FieldContext* ctx) override;
  void exitIndex_expression(cgullParser::Index_expressionContext* ctx) override;

  void exitFunction_call(cgullParser::Function_callContext* ctx) override;

  void exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) override;
  void exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) override;

  void exitCast_expression(cgullParser::Cast_expressionContext* ctx) override;
  void exitDereference_expression(cgullParser::Dereference_expressionContext* ctx) override;
  void exitReference_expression(cgullParser::Reference_expressionContext* ctx) override;

  void exitTuple_expression(cgullParser::Tuple_expressionContext* ctx) override;

  void exitAllocate_expression(cgullParser::Allocate_expressionContext* ctx) override;
  void exitAllocate_primitive(cgullParser::Allocate_primitiveContext* ctx) override;
  void exitAllocate_array(cgullParser::Allocate_arrayContext* ctx) override;
  void exitAllocate_struct(cgullParser::Allocate_structContext* ctx) override;

  void exitUnary_expression(cgullParser::Unary_expressionContext* ctx) override;
  void exitPostfix_expression(cgullParser::Postfix_expressionContext* ctx) override;

  void exitIf_expression(cgullParser::If_expressionContext* ctx) override;

  void enterFunction_definition(cgullParser::Function_definitionContext* ctx) override;
  void exitFunction_definition(cgullParser::Function_definitionContext* ctx) override;
  void exitReturn_statement(cgullParser::Return_statementContext* ctx) override;

  void exitDestructuring_item(cgullParser::Destructuring_itemContext* ctx) override;
  void exitDestructuring_statement(cgullParser::Destructuring_statementContext* ctx) override;

  std::shared_ptr<Type> checkAssignmentCompatibility(std::shared_ptr<Type> leftType, std::shared_ptr<Type> rightType,
                                                     antlr4::ParserRuleContext* ctx);

  std::vector<std::shared_ptr<Type>> collectArgumentTypes(cgullParser::Expression_listContext* exprList);

  void checkArgumentCompatibility(const std::vector<std::shared_ptr<Type>>& argumentTypes,
                                  const std::vector<std::shared_ptr<Type>>& parameterTypes,
                                  cgullParser::Expression_listContext* exprList, const std::string& functionName,
                                  int line, int pos);

  void setFunctionCallReturnType(cgullParser::Function_callContext* ctx,
                                 const std::vector<std::shared_ptr<Type>>& returnTypes);
};

#endif // TYPE_CHECKING_LISTENER_H
