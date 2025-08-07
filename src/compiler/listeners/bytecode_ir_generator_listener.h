#ifndef BYTECODE_IR_GENERATOR_LISTENER_H
#define BYTECODE_IR_GENERATOR_LISTENER_H

#include "../errors/error_reporter.h"
#include "../instructions/ir_class.h"
#include "../symbols/symbol.h"
#include "cgullBaseListener.h"

class BytecodeIRGeneratorListener : public cgullBaseListener {
public:
  BytecodeIRGeneratorListener(ErrorReporter& errorReporter,
                              std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes,
                              std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>>& expressionTypes,
                              std::unordered_set<antlr4::ParserRuleContext*>& expectingStringConversion);

  const std::vector<std::shared_ptr<IRClass>>& getClasses() const;

private:
  ErrorReporter& errorReporter;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;
  std::unordered_set<antlr4::ParserRuleContext*> expectingStringConversion;
  std::vector<std::shared_ptr<IRClass>> classes;
  // currently not used, no additional classes besides main currently supported
  std::stack<std::shared_ptr<IRClass>> currentClassStack;
  std::shared_ptr<FunctionSymbol> currentFunction;
  int currentLocalIndex = 0;

  int labelCounter = 0;
  std::stack<std::string> breakLabels;

  std::shared_ptr<Scope> getCurrentScope(antlr4::ParserRuleContext* ctx) const;
  std::string generateLabel();

  int assignLocalIndex(const std::shared_ptr<VariableSymbol>& variable);
  int getLocalIndex(const std::string& variableName, std::shared_ptr<Scope> scope);
  void generateStringConversion(antlr4::ParserRuleContext* ctx);

  virtual void enterProgram(cgullParser::ProgramContext* ctx) override;
  virtual void exitProgram(cgullParser::ProgramContext* ctx) override;

  virtual void enterFunction_definition(cgullParser::Function_definitionContext* ctx) override;
  virtual void exitFunction_definition(cgullParser::Function_definitionContext* ctx) override;

  virtual void enterFunction_call(cgullParser::Function_callContext* ctx) override;
  virtual void exitFunction_call(cgullParser::Function_callContext* ctx) override;

  virtual void exitExpression(cgullParser::ExpressionContext* ctx) override;

  virtual void enterBase_expression(cgullParser::Base_expressionContext* ctx) override;
  virtual void exitBase_expression(cgullParser::Base_expressionContext* ctx) override;

  virtual void enterVariable_declaration(cgullParser::Variable_declarationContext* ctx) override;
  virtual void exitVariable_declaration(cgullParser::Variable_declarationContext* ctx) override;

  virtual void exitVariable(cgullParser::VariableContext* ctx) override;

  virtual void exitAssignment_statement(cgullParser::Assignment_statementContext* ctx) override;

  virtual void exitReturn_statement(cgullParser::Return_statementContext* ctx) override;
};

#endif // BYTECODE_IR_GENERATOR_LISTENER_H
