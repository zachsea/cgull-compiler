#ifndef SYMBOL_COLLECTION_LISTENER_H
#define SYMBOL_COLLECTION_LISTENER_H

#include "../errors/error_reporter.h"
#include "../symbols/symbol.h"
#include <cgullBaseListener.h>
#include <string>
#include <unordered_map>

class SymbolCollectionListener : public cgullBaseListener {
public:
  SymbolCollectionListener(ErrorReporter& errorReporter, std::shared_ptr<Scope> existingScope = nullptr);

  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> getScopeMapping();
  std::shared_ptr<Scope> getCurrentScope();

  /* strictly symbol related */
  virtual void enterVariable_declaration(cgullParser::Variable_declarationContext* ctx) override;
  virtual void exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) override;

  virtual void enterDestructuring_item(cgullParser::Destructuring_itemContext* ctx) override;
  virtual void exitDestructuring_item(cgullParser::Destructuring_itemContext* ctx) override;

  /* scope related */

  virtual void enterProgram(cgullParser::ProgramContext* ctx) override;
  virtual void exitProgram(cgullParser::ProgramContext* ctx) override;

  virtual void enterStruct_definition(cgullParser::Struct_definitionContext* ctx) override;
  virtual void exitStruct_definition(cgullParser::Struct_definitionContext* ctx) override;

  virtual void enterFunction_definition(cgullParser::Function_definitionContext* ctx) override;
  virtual void exitFunction_definition(cgullParser::Function_definitionContext* ctx) override;

  virtual void enterBranch_block(cgullParser::Branch_blockContext* ctx) override;
  virtual void exitBranch_block(cgullParser::Branch_blockContext* ctx) override;

  virtual void enterUntil_statement(cgullParser::Until_statementContext* ctx) override;
  virtual void exitUntil_statement(cgullParser::Until_statementContext* ctx) override;

  virtual void enterWhile_statement(cgullParser::While_statementContext* ctx) override;
  virtual void exitWhile_statement(cgullParser::While_statementContext* ctx) override;

  virtual void enterFor_statement(cgullParser::For_statementContext* ctx) override;
  virtual void exitFor_statement(cgullParser::For_statementContext* ctx) override;

  virtual void enterInfinite_loop_statement(cgullParser::Infinite_loop_statementContext* ctx) override;
  virtual void exitInfinite_loop_statement(cgullParser::Infinite_loop_statementContext* ctx) override;

  virtual void enterAccess_block(cgullParser::Access_blockContext* ctx) override;
  virtual void exitAccess_block(cgullParser::Access_blockContext* ctx) override;

  virtual void enterTop_level_struct_statement(cgullParser::Top_level_struct_statementContext* ctx) override;
  virtual void exitTop_level_struct_statement(cgullParser::Top_level_struct_statementContext* ctx) override;

  /* checking if used before defined */

  virtual void enterIndexable(cgullParser::IndexableContext* ctx) override;

  virtual void enterDereferenceable(cgullParser::DereferenceableContext* ctx) override;

  virtual void enterFunction_call(cgullParser::Function_callContext* ctx) override;

  virtual void enterAllocate_struct(cgullParser::Allocate_structContext* ctx) override;

  virtual void enterCast_expression(cgullParser::Cast_expressionContext* ctx) override;

  virtual void enterPostfix_expression(cgullParser::Postfix_expressionContext* ctx) override;

  virtual void enterVariable(cgullParser::VariableContext* ctx) override;

private:
  std::shared_ptr<Scope> currentScope;
  std::shared_ptr<Scope> globalScope;
  ErrorReporter& errorReporter;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopes;
  bool inPrivateScope = false;

  std::shared_ptr<Type> resolveType(cgullParser::TypeContext* typeCtx);
  std::shared_ptr<Type> resolvePrimitiveType(const std::string& typeName);
  std::shared_ptr<VariableSymbol> createAndRegisterVariableSymbol(const std::string& identifier,
                                                                  cgullParser::TypeContext* typeCtx, bool isConst,
                                                                  int line, int column);
  std::pair<bool, std::shared_ptr<TypeSymbol>> isStructScope(std::shared_ptr<Scope> scope);
};

#endif // SYMBOL_COLLECTION_LISTENER_H
